/// @copyright
/// Copyright (C) OpenCyphal Development Team  <opencyphal.org>
/// Copyright Amazon.com Inc. or its affiliates.
/// SPDX-License-Identifier: MIT

#ifndef LIBCYPHAL_TRANSPORT_CAN_TRANSPORT_HPP_INCLUDED
#define LIBCYPHAL_TRANSPORT_CAN_TRANSPORT_HPP_INCLUDED

#include "delegate.hpp"
#include "media.hpp"
#include "msg_rx_session.hpp"
#include "msg_tx_session.hpp"
#include "svc_rx_sessions.hpp"
#include "svc_tx_sessions.hpp"

#include "libcyphal/runnable.hpp"
#include "libcyphal/transport/contiguous_payload.hpp"
#include "libcyphal/transport/errors.hpp"
#include "libcyphal/transport/msg_sessions.hpp"
#include "libcyphal/transport/svc_sessions.hpp"
#include "libcyphal/transport/transport.hpp"
#include "libcyphal/transport/types.hpp"
#include "libcyphal/types.hpp"

#include <canard.h>
#include <cetl/cetl.hpp>
#include <cetl/pf17/cetlpf.hpp>
#include <cetl/pf20/cetlpf.hpp>

#include <algorithm>
#include <array>
#include <chrono>
#include <cstddef>
#include <cstdint>
#include <limits>
#include <utility>

namespace libcyphal
{
namespace transport
{
namespace can
{

class ICanTransport : public ITransport
{
public:
    ICanTransport(const ICanTransport&)                = delete;
    ICanTransport(ICanTransport&&) noexcept            = delete;
    ICanTransport& operator=(const ICanTransport&)     = delete;
    ICanTransport& operator=(ICanTransport&&) noexcept = delete;

protected:
    ICanTransport()  = default;
    ~ICanTransport() = default;
};

/// Internal implementation details of the CAN transport.
/// Not supposed to be used directly by the users of the library.
///
namespace detail
{

/// @brief Represents final implementation class of the CAN transport.
///
/// NOSONAR cpp:S4963 for below `class TransportImpl` - we do directly handle resources here;
/// namely: in destructor we have to unsubscribe, as well as let delegate to know this fact.
///
class TransportImpl final : private TransportDelegate, public ICanTransport  // NOSONAR cpp:S4963
{
    /// @brief Defines specification for making interface unique ptr.
    ///
    struct Spec
    {
        using Interface = ICanTransport;
        using Concrete  = TransportImpl;

        // In use to disable public construction.
        // See https://seanmiddleditch.github.io/enabling-make-unique-with-private-constructors/
        explicit Spec() = default;
    };

    /// @brief Internal (private) storage of a media index, its interface and TX queue.
    ///
    struct Media final
    {
    public:
        Media(const std::size_t index, IMedia& interface, const std::size_t tx_capacity)
            : index_{static_cast<std::uint8_t>(index)}
            , interface_{interface}
            , canard_tx_queue_{::canardTxInit(tx_capacity, interface.getMtu())}
        {
        }

        std::uint8_t index() const
        {
            return index_;
        }

        IMedia& interface() const
        {
            return interface_;
        }

        CanardTxQueue& canard_tx_queue()
        {
            return canard_tx_queue_;
        }

        void propagateMtuToTxQueue()
        {
            canard_tx_queue_.mtu_bytes = interface_.getMtu();
        }

    private:
        const std::uint8_t index_;
        IMedia&            interface_;
        CanardTxQueue      canard_tx_queue_;
    };
    using MediaArray = libcyphal::detail::VarArray<Media>;

public:
    CETL_NODISCARD static Expected<UniquePtr<ICanTransport>, FactoryError> make(cetl::pmr::memory_resource& memory,
                                                                                const cetl::span<IMedia*>   media,
                                                                                const std::size_t           tx_capacity)
    {
        // Verify input arguments:
        // - At least one media interface must be provided, but no more than the maximum allowed (255).
        // - If a local node ID is provided, it must be within the valid range.
        //
        const auto media_count =
            static_cast<std::size_t>(std::count_if(media.begin(), media.end(), [](const IMedia* const media_ptr) {
                return media_ptr != nullptr;
            }));
        if ((media_count == 0) || (media_count > std::numeric_limits<std::uint8_t>::max()))
        {
            return ArgumentError{};
        }

        const MediaArray media_array{make_media_array(memory, media_count, media, tx_capacity)};
        if (media_array.size() != media_count)
        {
            return MemoryError{};
        }

        auto transport = libcyphal::detail::makeUniquePtr<Spec>(memory, Spec{}, memory, media_array);
        if (transport == nullptr)
        {
            return MemoryError{};
        }

        return transport;
    }

    TransportImpl(Spec, cetl::pmr::memory_resource& memory, MediaArray media_array)
        : TransportDelegate{memory}
        , media_array_{std::move(media_array)}
        , should_reconfigure_filters_{false}
        , total_message_ports_{0}
        , total_service_ports_{0}
    {
    }

    TransportImpl(const TransportImpl&)                = delete;
    TransportImpl(TransportImpl&&) noexcept            = delete;
    TransportImpl& operator=(const TransportImpl&)     = delete;
    TransportImpl& operator=(TransportImpl&&) noexcept = delete;

    ~TransportImpl()
    {
        for (Media& media : media_array_)
        {
            flushCanardTxQueue(media.canard_tx_queue());
        }

        CETL_DEBUG_ASSERT(total_message_ports_ == 0, "Message sessions must be destroyed before transport.");
        CETL_DEBUG_ASSERT(total_service_ports_ == 0, "Service sessions must be destroyed before transport.");
    }

private:
    // MARK: ITransport

    CETL_NODISCARD cetl::optional<NodeId> getLocalNodeId() const noexcept override
    {
        if (canard_instance().node_id > CANARD_NODE_ID_MAX)
        {
            return cetl::nullopt;
        }

        return cetl::make_optional(static_cast<NodeId>(canard_instance().node_id));
    }

    CETL_NODISCARD cetl::optional<ArgumentError> setLocalNodeId(const NodeId node_id) noexcept override
    {
        if (node_id > CANARD_NODE_ID_MAX)
        {
            return ArgumentError{};
        }

        // Allow setting the same node ID multiple times, but only once otherwise.
        //
        CanardInstance& ins = canard_instance();
        if (ins.node_id == node_id)
        {
            return cetl::nullopt;
        }
        if (ins.node_id != CANARD_NODE_ID_UNSET)
        {
            return ArgumentError{};
        }

        ins.node_id = static_cast<CanardNodeID>(node_id);

        // We just became non-anonymous node, so we might need to reconfigure media filters
        // in case we have at least one service RX port.
        //
        // @see runMediaFilters
        //
        if (total_service_ports_ > 0)
        {
            should_reconfigure_filters_ = true;
        }

        return cetl::nullopt;
    }

    CETL_NODISCARD ProtocolParams getProtocolParams() const noexcept override
    {
        std::size_t min_mtu = std::numeric_limits<std::size_t>::max();
        for (const Media& media : media_array_)
        {
            min_mtu = std::min(min_mtu, media.interface().getMtu());
        }

        return ProtocolParams{static_cast<TransferId>(1) << CANARD_TRANSFER_ID_BIT_LENGTH,
                              min_mtu,
                              CANARD_NODE_ID_MAX + 1};
    }

    void setTransientErrorHandler(TransientErrorHandler handler) override
    {
        transient_error_handler_ = std::move(handler);
    }

    CETL_NODISCARD Expected<UniquePtr<IMessageRxSession>, AnyError> makeMessageRxSession(
        const MessageRxParams& params) override
    {
        const cetl::optional<AnyError> any_error = ensureNewSessionFor(CanardTransferKindMessage, params.subject_id);
        if (any_error.has_value())
        {
            return any_error.value();
        }

        return MessageRxSession::make(asDelegate(), params);
    }

    CETL_NODISCARD Expected<UniquePtr<IMessageTxSession>, AnyError> makeMessageTxSession(
        const MessageTxParams& params) override
    {
        return MessageTxSession::make(asDelegate(), params);
    }

    CETL_NODISCARD Expected<UniquePtr<IRequestRxSession>, AnyError> makeRequestRxSession(
        const RequestRxParams& params) override
    {
        const cetl::optional<AnyError> any_error = ensureNewSessionFor(CanardTransferKindRequest, params.service_id);
        if (any_error.has_value())
        {
            return any_error.value();
        }

        return SvcRequestRxSession::make(asDelegate(), params);
    }

    CETL_NODISCARD Expected<UniquePtr<IRequestTxSession>, AnyError> makeRequestTxSession(
        const RequestTxParams& params) override
    {
        return SvcRequestTxSession::make(asDelegate(), params);
    }

    CETL_NODISCARD Expected<UniquePtr<IResponseRxSession>, AnyError> makeResponseRxSession(
        const ResponseRxParams& params) override
    {
        const cetl::optional<AnyError> any_error = ensureNewSessionFor(CanardTransferKindResponse, params.service_id);
        if (any_error.has_value())
        {
            return any_error.value();
        }

        return SvcResponseRxSession::make(asDelegate(), params);
    }

    CETL_NODISCARD Expected<UniquePtr<IResponseTxSession>, AnyError> makeResponseTxSession(
        const ResponseTxParams& params) override
    {
        return SvcResponseTxSession::make(asDelegate(), params);
    }

    // MARK: IRunnable

    CETL_NODISCARD IRunnable::MaybeError run(const TimePoint now) override
    {
        cetl::optional<AnyError> any_error{};

        any_error = runMediaTransmit(now);
        if (any_error.has_value())
        {
            return any_error.value();
        }

        any_error = runMediaReceive();
        if (any_error.has_value())
        {
            return any_error.value();
        }

        any_error = runMediaFilters();
        if (any_error.has_value())
        {
            return any_error.value();
        }

        return {};
    }

    // MARK: TransportDelegate

    CETL_NODISCARD TransportDelegate& asDelegate()
    {
        return *this;
    }

    CETL_NODISCARD cetl::optional<AnyError> sendTransfer(const TimePoint               deadline,
                                                         const CanardTransferMetadata& metadata,
                                                         const PayloadFragments        payload_fragments) override
    {
        // libcanard currently does not support fragmented payloads (at `canardTxPush`).
        // so we need to concatenate them when there are more than one non-empty fragment.
        // See https://github.com/OpenCyphal/libcanard/issues/223
        //
        const transport::detail::ContiguousPayload payload{memory(), payload_fragments};
        if ((payload.data() == nullptr) && (payload.size() > 0))
        {
            return MemoryError{};
        }

        const auto deadline_us = std::chrono::duration_cast<std::chrono::microseconds>(deadline.time_since_epoch());

        for (Media& media : media_array_)
        {
            media.propagateMtuToTxQueue();

            const std::int32_t result = ::canardTxPush(&media.canard_tx_queue(),
                                                       &canard_instance(),
                                                       static_cast<CanardMicrosecond>(deadline_us.count()),
                                                       &metadata,
                                                       payload.size(),
                                                       payload.data());

            auto opt_any_error = TransportDelegate::optAnyErrorFromCanard(result);
            if (opt_any_error.has_value())
            {
                if (transient_error_handler_)
                {
                    AnyErrorReport report{std::move(opt_any_error.value()),
                                          AnyErrorReport::Operation::TxPush,
                                          media.index(),
                                          &canard_instance()};
                    opt_any_error = transient_error_handler_(report);
                }

                if (opt_any_error.has_value())
                {
                    return opt_any_error;
                }
            }
        }

        return cetl::nullopt;
    }

    void triggerUpdateOfFilters(const FiltersUpdateCondition condition) noexcept override
    {
        switch (condition)
        {
        case FiltersUpdateCondition::SubjectPortAdded: {
            ++total_message_ports_;
            break;
        }
        case FiltersUpdateCondition::SubjectPortRemoved: {
            // We are not going to allow negative number of ports.
            CETL_DEBUG_ASSERT(total_message_ports_ > 0, "");
            total_message_ports_ -= std::min(static_cast<std::size_t>(1), total_message_ports_);
            break;
        }
        case FiltersUpdateCondition::ServicePortAdded: {
            ++total_service_ports_;
            break;
        }
        case FiltersUpdateCondition::ServicePortRemoved: {
            // We are not going to allow negative number of ports.
            CETL_DEBUG_ASSERT(total_service_ports_ > 0, "");
            total_service_ports_ -= std::min(static_cast<std::size_t>(1), total_service_ports_);
            break;
        }
        default: {
            // NOLINTNEXTLINE(cert-dcl03-c,hicpp-static-assert,misc-static-assert)
            CETL_DEBUG_ASSERT(false, "Unexpected condition.");
            return;
        }
        }

        should_reconfigure_filters_ = true;
    }

    // MARK: Privates:

    template <typename ErrorVariant>
    CETL_NODISCARD static AnyError anyErrorFromVariant(ErrorVariant&& other_error_var)
    {
        return cetl::visit([](auto&& error) -> AnyError { return std::forward<decltype(error)>(error); },
                           std::forward<ErrorVariant>(other_error_var));
    }

    CETL_NODISCARD cetl::optional<AnyError> ensureNewSessionFor(const CanardTransferKind transfer_kind,
                                                                const PortId             port_id) noexcept
    {
        const std::int8_t has_port = ::canardRxGetSubscription(&canard_instance(), transfer_kind, port_id, nullptr);
        CETL_DEBUG_ASSERT(has_port >= 0, "There is no way currently to get an error here.");
        if (has_port > 0)
        {
            return AlreadyExistsError{};
        }

        return cetl::nullopt;
    }

    CETL_NODISCARD static MediaArray make_media_array(cetl::pmr::memory_resource& memory,
                                                      const std::size_t           media_count,
                                                      const cetl::span<IMedia*>   media_interfaces,
                                                      const std::size_t           tx_capacity)
    {
        MediaArray media_array{media_count, &memory};

        // Reserve the space for the whole array (to avoid reallocations).
        // Capacity will be less than requested in case of out of memory.
        media_array.reserve(media_count);
        if (media_array.capacity() >= media_count)
        {
            std::size_t index = 0;
            for (IMedia* const media_interface : media_interfaces)
            {
                if (media_interface != nullptr)
                {
                    IMedia& media = *media_interface;
                    media_array.emplace_back(index, media, tx_capacity);
                    index++;
                }
            }
            CETL_DEBUG_ASSERT(index == media_count, "");
            CETL_DEBUG_ASSERT(media_array.size() == media_count, "");
        }

        return media_array;
    }

    void flushCanardTxQueue(CanardTxQueue& canard_tx_queue)
    {
        while (const CanardTxQueueItem* const maybe_item = ::canardTxPeek(&canard_tx_queue))
        {
            CanardTxQueueItem* const item = ::canardTxPop(&canard_tx_queue, maybe_item);
            freeCanardMemory(item);
        }
    }

    cetl::optional<AnyError> runMediaReceive()
    {
        cetl::optional<AnyError> opt_any_error{};

        for (Media& media : media_array_)
        {
            opt_any_error = runSingleMediaReceive(media);
            if (opt_any_error.has_value())
            {
                break;
            }
        }

        return opt_any_error;
    }

    cetl::optional<AnyError> runSingleMediaReceive(const Media& media)
    {
        std::array<cetl::byte, CANARD_MTU_MAX> payload{};

        Expected<cetl::optional<RxMetadata>, MediaError> pop_result = media.interface().pop(payload);
        if (auto* media_error = cetl::get_if<MediaError>(&pop_result))
        {
            auto any_error = anyErrorFromVariant(std::move(*media_error));
            if (transient_error_handler_)
            {
                AnyErrorReport report{std::move(any_error),
                                      AnyErrorReport::Operation::MediaPop,
                                      media.index(),
                                      &media.interface()};
                return transient_error_handler_(report);
            }
            return any_error;
        }
        const auto* const opt_rx_meta = cetl::get_if<cetl::optional<RxMetadata>>(&pop_result);
        if ((opt_rx_meta == nullptr) || !opt_rx_meta->has_value())
        {
            return cetl::nullopt;
        }

        const RxMetadata& rx_meta = opt_rx_meta->value();

        const auto timestamp_us =
            std::chrono::duration_cast<std::chrono::microseconds>(rx_meta.timestamp.time_since_epoch());
        const CanardFrame canard_frame{rx_meta.can_id, rx_meta.payload_size, payload.cbegin()};

        CanardRxTransfer      out_transfer{};
        CanardRxSubscription* out_subscription{};

        const std::int8_t result = ::canardRxAccept(&canard_instance(),
                                                    static_cast<CanardMicrosecond>(timestamp_us.count()),
                                                    &canard_frame,
                                                    media.index(),
                                                    &out_transfer,
                                                    &out_subscription);

        auto opt_any_error = optAnyErrorFromCanard(result);
        if (opt_any_error.has_value())
        {
            if (transient_error_handler_)
            {
                AnyErrorReport report{std::move(opt_any_error.value()),
                                      AnyErrorReport::Operation::RxAccept,
                                      media.index(),
                                      &canard_instance()};
                opt_any_error = transient_error_handler_(report);
            }
            return opt_any_error;
        }
        if (result > 0)
        {
            CETL_DEBUG_ASSERT(out_subscription != nullptr, "Expected subscription.");
            CETL_DEBUG_ASSERT(out_subscription->user_reference != nullptr, "Expected session delegate.");

            auto* const delegate = static_cast<IRxSessionDelegate*>(out_subscription->user_reference);
            delegate->acceptRxTransfer(out_transfer);
        }

        return cetl::nullopt;
    }

    /// @brief Runs transmission loop for each redundant media interface.
    ///
    CETL_NODISCARD cetl::optional<AnyError> runMediaTransmit(const TimePoint now)
    {
        cetl::optional<AnyError> opt_any_error{};

        for (Media& media : media_array_)
        {
            opt_any_error = runSingleMediaTransmit(media, now);
            if (opt_any_error.has_value())
            {
                break;
            }
        }

        return opt_any_error;
    }

    /// @brief Runs transmission loop for a single media interface.
    ///
    /// Transmits as much as possible frames that are ready to be sent by the media interface.
    ///
    CETL_NODISCARD cetl::optional<AnyError> runSingleMediaTransmit(Media& media, const TimePoint now)
    {
        while (const CanardTxQueueItem* const tx_item = ::canardTxPeek(&media.canard_tx_queue()))
        {
            // We are dropping any TX item that has expired.
            // Otherwise, we would send it to the media interface.
            // We use strictly `<` (instead of `<=`) to give this frame a chance (one extra 1us) at media level.
            //
            const auto deadline = TimePoint{std::chrono::microseconds{tx_item->tx_deadline_usec}};
            if (now < deadline)
            {
                const cetl::span<const cetl::byte> payload{static_cast<const cetl::byte*>(tx_item->frame.payload),
                                                           tx_item->frame.payload_size};

                Expected<bool, MediaError> maybe_pushed =
                    media.interface().push(deadline, tx_item->frame.extended_can_id, payload);

                // In case of media push error we are going to drop this problematic frame
                // (b/c it looks like media can't handle this frame),
                // but we will continue to process with other frames if error handler says so.
                // Note that media not being ready/able to push a frame just yet (aka temporary)
                // is not reported as an error (see `is_pushed` below).
                //
                if (auto* media_error = cetl::get_if<MediaError>(&maybe_pushed))
                {
                    cetl::optional<AnyError> opt_any_error = anyErrorFromVariant(std::move(*media_error));
                    if (transient_error_handler_)
                    {
                        AnyErrorReport report{std::move(opt_any_error.value()),
                                              AnyErrorReport::Operation::MediaPush,
                                              media.index(),
                                              &media.interface()};
                        opt_any_error = transient_error_handler_(report);
                    }
                    if (opt_any_error.has_value())
                    {
                        freeCanardMemory(::canardTxPop(&media.canard_tx_queue(), tx_item));
                        return opt_any_error;
                    }
                }

                const auto* const is_pushed = cetl::get_if<bool>(&maybe_pushed);
                if ((is_pushed != nullptr) && !*is_pushed)
                {
                    // Media interface is busy, so we will just try again with it later (on next `run`).
                    break;
                }
            }

            freeCanardMemory(::canardTxPop(&media.canard_tx_queue(), tx_item));

        }  // for each frame

        return cetl::nullopt;
    }

    /// \brief Runs (if needed) reconfiguration of media filters based on the currently active RX ports.
    ///
    /// Temporary allocates memory buffers for all filters, one per each active port (message or service).
    /// In case of redundant media, each media interface will be called with the same span of filters.
    /// In case of zero ports, we still need to call media interfaces to clear their filters,
    /// though there will be no memory allocation for the empty buffer.
    ///
    /// @note Service RX ports are not considered as active ones for \b anonymous nodes.
    ///
    /// @note If \b whole reconfiguration process was successful,
    /// `should_reconfigure_filters_` will be reset to `false`, so that next time the run won't do any work.
    /// But in case of any failure (memory allocation or media error),
    /// `should_reconfigure_filters_` will stay engaged (`true`), so that we will try again on next run.
    ///
    CETL_NODISCARD cetl::optional<AnyError> runMediaFilters()
    {
        if (!should_reconfigure_filters_)
        {
            return cetl::nullopt;
        }

        libcyphal::detail::VarArray<Filter> filters{&memory()};
        if (!fillMediaFiltersArray(filters))
        {
            // This is out of memory situation. We will just leave this run,
            // but `should_reconfigure_filters_` will stay engaged, so we will try again on next run.
            return MemoryError{};
        }

        // Let each media interface know about the new filters (tracking the fact of possible media error).
        //
        bool was_error = false;
        for (const Media& media : media_array_)
        {
            cetl::optional<MediaError> media_error = media.interface().setFilters({filters.data(), filters.size()});
            if (media_error.has_value())
            {
                was_error                              = true;
                cetl::optional<AnyError> opt_any_error = anyErrorFromVariant(std::move(media_error.value()));

                // If we don't have a transient error handler we will just leave this run with this failure as is.
                // Note that `should_reconfigure_filters_` still stays engaged, so we will try again on next run.
                //
                if (transient_error_handler_)
                {
                    AnyErrorReport report{std::move(opt_any_error.value()),
                                          AnyErrorReport::Operation::MediaConfig,
                                          media.index(),
                                          &media.interface()};
                    opt_any_error = transient_error_handler_(report);
                }

                if (opt_any_error.has_value())
                {
                    return opt_any_error;
                }
            }
        }

        if (!was_error)
        {
            should_reconfigure_filters_ = false;
        }

        return cetl::nullopt;
    }

    /// @brief Fills an array with filters for each active RX port.
    ///
    CETL_NODISCARD bool fillMediaFiltersArray(libcyphal::detail::VarArray<Filter>& filters)
    {
        using RxSubscription     = const CanardRxSubscription;
        using RxSubscriptionTree = CanardConcreteTree<RxSubscription>;

        // Total "active" RX ports depends on the local node ID. For anonymous nodes,
        // we don't account for service ports (b/c they don't work while being anonymous).
        //
        const CanardNodeID local_node_id      = canard_instance().node_id;
        const auto         is_anonymous       = local_node_id > CANARD_NODE_ID_MAX;
        const std::size_t  total_active_ports = total_message_ports_ + (is_anonymous ? 0 : total_service_ports_);
        if (total_active_ports == 0)
        {
            // No need to allocate memory for zero filters.
            return true;
        }

        // Now we know that we have at least one active port,
        // so we need preallocate temp memory for total number of active ports.
        //
        filters.reserve(total_active_ports);
        if (filters.capacity() < total_active_ports)
        {
            // This is out of memory situation.
            return false;
        }

        // `ports_count` counting is just for the sake of debug verification.
        std::size_t ports_count = 0;

        const auto& subs_trees = canard_instance().rx_subscriptions;

        if (total_message_ports_ > 0)
        {
            const auto msg_visitor = [&filters](RxSubscription& rx_subscription) {
                // Make and store a single message filter.
                const auto flt = ::canardMakeFilterForSubject(rx_subscription.port_id);
                filters.emplace_back(Filter{flt.extended_can_id, flt.extended_mask});
            };
            ports_count += RxSubscriptionTree::visitCounting(subs_trees[CanardTransferKindMessage], msg_visitor);
        }

        // No need to make service filters if we don't have a local node ID.
        //
        if ((total_service_ports_ > 0) && !is_anonymous)
        {
            const auto svc_visitor = [&filters, local_node_id](RxSubscription& rx_subscription) {
                // Make and store a single service filter.
                const auto flt = ::canardMakeFilterForService(rx_subscription.port_id, local_node_id);
                filters.emplace_back(Filter{flt.extended_can_id, flt.extended_mask});
            };
            ports_count += RxSubscriptionTree::visitCounting(subs_trees[CanardTransferKindRequest], svc_visitor);
            ports_count += RxSubscriptionTree::visitCounting(subs_trees[CanardTransferKindResponse], svc_visitor);
        }

        (void) ports_count;
        CETL_DEBUG_ASSERT(ports_count == total_active_ports, "");
        return true;
    }

    // MARK: Data members:

    MediaArray            media_array_;
    bool                  should_reconfigure_filters_;
    std::size_t           total_message_ports_;
    std::size_t           total_service_ports_;
    TransientErrorHandler transient_error_handler_;

};  // TransportImpl

}  // namespace detail

/// @brief Makes a new CAN transport instance.
///
/// NB! Lifetime of the transport instance must never outlive `memory` and `media` instances.
///
/// @param memory Reference to a polymorphic memory resource to use for all allocations.
/// @param media Collection of redundant media interfaces to use.
/// @param tx_capacity Total number of frames that can be queued for transmission per `IMedia` instance.
/// @return Unique pointer to the new CAN transport instance or an error.
///
inline Expected<UniquePtr<ICanTransport>, FactoryError> makeTransport(cetl::pmr::memory_resource& memory,
                                                                      const cetl::span<IMedia*>   media,
                                                                      const std::size_t           tx_capacity)
{
    return detail::TransportImpl::make(memory, media, tx_capacity);
}

}  // namespace can
}  // namespace transport
}  // namespace libcyphal

#endif  // LIBCYPHAL_TRANSPORT_CAN_TRANSPORT_HPP_INCLUDED
