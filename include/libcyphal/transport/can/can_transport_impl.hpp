/// @copyright
/// Copyright (C) OpenCyphal Development Team  <opencyphal.org>
/// Copyright Amazon.com Inc. or its affiliates.
/// SPDX-License-Identifier: MIT

#ifndef LIBCYPHAL_TRANSPORT_CAN_TRANSPORT_IMPL_HPP_INCLUDED
#define LIBCYPHAL_TRANSPORT_CAN_TRANSPORT_IMPL_HPP_INCLUDED

#include "can_transport.hpp"
#include "delegate.hpp"
#include "media.hpp"
#include "msg_rx_session.hpp"
#include "msg_tx_session.hpp"
#include "svc_rx_sessions.hpp"
#include "svc_tx_sessions.hpp"

#include "libcyphal/runnable.hpp"
#include "libcyphal/transport/common/tools.hpp"
#include "libcyphal/transport/contiguous_payload.hpp"
#include "libcyphal/transport/errors.hpp"
#include "libcyphal/transport/msg_sessions.hpp"
#include "libcyphal/transport/svc_sessions.hpp"
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

/// Internal implementation details of the CAN transport.
/// Not supposed to be used directly by the users of the library.
///
namespace detail
{

/// @brief Represents final implementation class of the CAN transport.
///
/// NOSONAR cpp:S4963 for below `class TransportImpl` - we do directly handle resources here;
/// namely: in destructor we have to flush TX queues (otherwise there will be memory leaks).
///
class TransportImpl final : private TransportDelegate, public ICanTransport  // NOSONAR cpp:S4963
{
    /// @brief Defines private specification for making interface unique ptr.
    ///
    struct Spec : libcyphal::detail::UniquePtrSpec<ICanTransport, TransportImpl>
    {
        // `explicit` here is in use to disable public construction of derived private `Spec` structs.
        // See https://seanmiddleditch.github.io/enabling-make-unique-with-private-constructors/
        explicit Spec() = default;
    };

    /// @brief Defines private storage of a media index, its interface and TX queue.
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
    CETL_NODISCARD static Expected<UniquePtr<ICanTransport>, FactoryFailure> make(cetl::pmr::memory_resource& memory,
                                                                                  const cetl::span<IMedia*>   media,
                                                                                  const std::size_t tx_capacity)
    {
        // Verify input arguments:
        // - At least one media interface must be provided, but no more than the maximum allowed (255).
        //
        const auto media_count = static_cast<std::size_t>(
            std::count_if(media.begin(), media.end(), [](const IMedia* const media_ptr) -> bool {
                return media_ptr != nullptr;
            }));
        if ((media_count == 0) || (media_count > std::numeric_limits<std::uint8_t>::max()))
        {
            return ArgumentError{};
        }

        // False positive of clang-tidy - we move `media_array` to the `transport` instance, so can't make it const.
        // NOLINTNEXTLINE(misc-const-correctness)
        MediaArray media_array = makeMediaArray(memory, media_count, media, tx_capacity);
        if (media_array.size() != media_count)
        {
            return MemoryError{};
        }

        auto transport = libcyphal::detail::makeUniquePtr<Spec>(memory, Spec{}, memory, std::move(media_array));
        if (transport == nullptr)
        {
            return MemoryError{};
        }

        return transport;
    }

    TransportImpl(const Spec, cetl::pmr::memory_resource& memory, MediaArray&& media_array)
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
    // MARK: ICanTransport

    void setTransientErrorHandler(TransientErrorHandler handler) override
    {
        transient_error_handler_ = std::move(handler);
    }

    // MARK: ITransport

    CETL_NODISCARD cetl::optional<NodeId> getLocalNodeId() const noexcept override
    {
        if (node_id() > CANARD_NODE_ID_MAX)
        {
            return cetl::nullopt;
        }

        return cetl::make_optional(node_id());
    }

    CETL_NODISCARD cetl::optional<ArgumentError> setLocalNodeId(const NodeId new_node_id) noexcept override
    {
        if (new_node_id > CANARD_NODE_ID_MAX)
        {
            return ArgumentError{};
        }

        // Allow setting the same node ID multiple times, but only once otherwise.
        //
        if (node_id() == new_node_id)
        {
            return cetl::nullopt;
        }
        if (node_id() != CANARD_NODE_ID_UNSET)
        {
            return ArgumentError{};
        }
        setNodeId(new_node_id);

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

    CETL_NODISCARD Expected<UniquePtr<IMessageRxSession>, AnyFailure> makeMessageRxSession(
        const MessageRxParams& params) override
    {
        const cetl::optional<AnyFailure> failure = ensureNewSessionFor(CanardTransferKindMessage, params.subject_id);
        if (failure.has_value())
        {
            return failure.value();
        }

        return MessageRxSession::make(asDelegate(), params);
    }

    CETL_NODISCARD Expected<UniquePtr<IMessageTxSession>, AnyFailure> makeMessageTxSession(
        const MessageTxParams& params) override
    {
        return MessageTxSession::make(asDelegate(), params);
    }

    CETL_NODISCARD Expected<UniquePtr<IRequestRxSession>, AnyFailure> makeRequestRxSession(
        const RequestRxParams& params) override
    {
        const cetl::optional<AnyFailure> failure = ensureNewSessionFor(CanardTransferKindRequest, params.service_id);
        if (failure.has_value())
        {
            return failure.value();
        }

        return SvcRequestRxSession::make(asDelegate(), params);
    }

    CETL_NODISCARD Expected<UniquePtr<IRequestTxSession>, AnyFailure> makeRequestTxSession(
        const RequestTxParams& params) override
    {
        return SvcRequestTxSession::make(asDelegate(), params);
    }

    CETL_NODISCARD Expected<UniquePtr<IResponseRxSession>, AnyFailure> makeResponseRxSession(
        const ResponseRxParams& params) override
    {
        const cetl::optional<AnyFailure> failure = ensureNewSessionFor(CanardTransferKindResponse, params.service_id);
        if (failure.has_value())
        {
            return failure.value();
        }

        return SvcResponseRxSession::make(asDelegate(), params);
    }

    CETL_NODISCARD Expected<UniquePtr<IResponseTxSession>, AnyFailure> makeResponseTxSession(
        const ResponseTxParams& params) override
    {
        return SvcResponseTxSession::make(asDelegate(), params);
    }

    // MARK: IRunnable

    CETL_NODISCARD IRunnable::MaybeFailure run(const TimePoint now) override
    {
        cetl::optional<AnyFailure> failure{};

        // We deliberately first run TX as much as possible, and only then running RX -
        // transmission will release resources (like TX queue items) and make room for new incoming frames.
        //
        failure = runMediaTransmit(now);
        if (failure.has_value())
        {
            return failure.value();
        }
        //
        failure = runMediaReceive();
        if (failure.has_value())
        {
            return failure.value();
        }

        failure = runMediaFilters();
        if (failure.has_value())
        {
            return failure.value();
        }

        return {};
    }

    // MARK: TransportDelegate

    CETL_NODISCARD TransportDelegate& asDelegate()
    {
        return *this;
    }

    CETL_NODISCARD cetl::optional<AnyFailure> sendTransfer(const TimePoint               deadline,
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

            // No Sonar `cpp:S5356` b/c we need to pass payload as a raw data to the libcanard.
            const std::int32_t result = ::canardTxPush(&media.canard_tx_queue(),
                                                       &canard_instance(),
                                                       static_cast<CanardMicrosecond>(deadline_us.count()),
                                                       &metadata,
                                                       payload.size(),
                                                       payload.data());  // NOSONAR cpp:S5356

            cetl::optional<AnyFailure> failure =
                tryHandleTransientCanardResult<TransientErrorReport::CanardTxPush>(media, result);
            if (failure.has_value())
            {
                // The handler (if any) just said that it's NOT fine to continue with pushing to other media TX queues,
                // and the failure should not be ignored but propagated outside.
                return failure;
            }
        }

        return cetl::nullopt;
    }

    void triggerUpdateOfFilters(const FiltersUpdate::Variant& update_var) override
    {
        FiltersUpdateHandler handler_with{*this};
        cetl::visit(handler_with, update_var);

        should_reconfigure_filters_ = true;
    }

    // MARK: Privates:

    using Self = TransportImpl;

    struct FiltersUpdateHandler
    {
        explicit FiltersUpdateHandler(Self& self)
            : self_{self}
        {
        }

        void operator()(const FiltersUpdate::SubjectPort& port) const
        {
            if (port.is_added)
            {
                ++self_.total_message_ports_;
            }
            else
            {
                // We are not going to allow negative number of ports.
                CETL_DEBUG_ASSERT(self_.total_message_ports_ > 0, "");
                self_.total_message_ports_ -= std::min(static_cast<std::size_t>(1), self_.total_message_ports_);
            }
        }

        void operator()(const FiltersUpdate::ServicePort& port) const
        {
            if (port.is_added)
            {
                ++self_.total_service_ports_;
            }
            else
            {
                // We are not going to allow negative number of ports.
                CETL_DEBUG_ASSERT(self_.total_service_ports_ > 0, "");
                self_.total_service_ports_ -= std::min(static_cast<std::size_t>(1), self_.total_service_ports_);
            }
        }

    private:
        Self& self_;

    };  // FiltersUpdateHandler

    template <typename Report>
    CETL_NODISCARD cetl::optional<AnyFailure> tryHandleTransientMediaError(const Media& media, MediaFailure&& error)
    {
        AnyFailure failure = common::detail::anyFailureFromVariant(std::move(error));
        if (!transient_error_handler_)
        {
            return failure;
        }

        TransientErrorReport::Variant report_var{Report{std::move(failure), media.index(), media.interface()}};
        return transient_error_handler_(report_var);
    }

    template <typename Report>
    CETL_NODISCARD cetl::optional<AnyFailure> tryHandleTransientCanardResult(const Media&       media,
                                                                             const std::int32_t result)
    {
        cetl::optional<AnyFailure> failure = optAnyFailureFromCanard(result);
        if (failure.has_value() && transient_error_handler_)
        {
            TransientErrorReport::Variant report_var{
                Report{std::move(failure.value()), media.index(), canard_instance()}};

            failure = transient_error_handler_(report_var);
        }
        return failure;
    }

    CETL_NODISCARD cetl::optional<AnyFailure> ensureNewSessionFor(const CanardTransferKind transfer_kind,
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

    CETL_NODISCARD static MediaArray makeMediaArray(cetl::pmr::memory_resource& memory,
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

    void flushCanardTxQueue(CanardTxQueue& canard_tx_queue) const
    {
        while (const CanardTxQueueItem* const maybe_item = ::canardTxPeek(&canard_tx_queue))
        {
            CanardTxQueueItem* const item = ::canardTxPop(&canard_tx_queue, maybe_item);

            // No Sonar `cpp:S5356` b/c we need to free tx item allocated by libcanard as a raw memory.
            freeCanardMemory(item);  // NOSONAR cpp:S5356
        }
    }

    cetl::optional<AnyFailure> runMediaReceive()
    {
        for (const Media& media : media_array_)
        {
            cetl::optional<AnyFailure> failure = runSingleMediaReceive(media);
            if (failure.has_value())
            {
                return failure;
            }
        }

        return cetl::nullopt;
    }

    cetl::optional<AnyFailure> runSingleMediaReceive(const Media& media)
    {
        std::array<cetl::byte, CANARD_MTU_MAX> payload{};

        Expected<cetl::optional<RxMetadata>, MediaFailure> pop_result = media.interface().pop(payload);
        if (auto* const failure = cetl::get_if<MediaFailure>(&pop_result))
        {
            return tryHandleTransientMediaError<TransientErrorReport::MediaPop>(media, std::move(*failure));
        }
        const auto& opt_rx_meta = cetl::get<cetl::optional<RxMetadata>>(pop_result);
        if (!opt_rx_meta.has_value())
        {
            return cetl::nullopt;
        }

        const RxMetadata& rx_meta = opt_rx_meta.value();

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

        cetl::optional<AnyFailure> failure =
            tryHandleTransientCanardResult<TransientErrorReport::CanardRxAccept>(media, result);
        if ((!failure.has_value()) && (result > 0))
        {
            CETL_DEBUG_ASSERT(out_subscription != nullptr, "Expected subscription.");
            CETL_DEBUG_ASSERT(out_subscription->user_reference != nullptr, "Expected session delegate.");

            // No Sonar `cpp:S5357` b/c the raw `user_reference` is part of libcanard api,
            // and it was set by us at a RX session constructor (see f.e. `MessageRxSession` ctor).
            auto* const delegate =
                static_cast<IRxSessionDelegate*>(out_subscription->user_reference);  // NOSONAR cpp:S5357
            delegate->acceptRxTransfer(out_transfer);
        }

        return failure;
    }

    /// @brief Runs transmission loop for each redundant media interface.
    ///
    CETL_NODISCARD cetl::optional<AnyFailure> runMediaTransmit(const TimePoint now)
    {
        for (Media& media : media_array_)
        {
            cetl::optional<AnyFailure> failure = runSingleMediaTransmit(media, now);
            if (failure.has_value())
            {
                return failure;
            }
        }

        return cetl::nullopt;
    }

    /// @brief Runs transmission loop for a single media interface.
    ///
    /// Transmits as much as possible frames that are ready to be sent by the media interface.
    ///
    CETL_NODISCARD cetl::optional<AnyFailure> runSingleMediaTransmit(Media& media, const TimePoint now)
    {
        while (const CanardTxQueueItem* const tx_item = ::canardTxPeek(&media.canard_tx_queue()))
        {
            // We are dropping any TX item that has expired.
            // Otherwise, we would send it to the media interface.
            // We use strictly `>=` (instead of `>`) to give this frame a chance (one extra 1us) at media level.
            //
            const auto deadline = TimePoint{std::chrono::microseconds{tx_item->tx_deadline_usec}};
            if (now >= deadline)
            {
                // Release whole expired transfer b/c possible next frames of the same transfer are also expired.
                popAndFreeCanardTxQueueItem(&media.canard_tx_queue(), tx_item, true /*whole transfer*/);

                // No Sonar `cpp:S909` b/c it make sense to use `continue` statement here - the corner case of
                // "early" (by deadline) transfer drop. Using `if` would make the code less readable and more nested.
                continue;  // NOSONAR cpp:S909
            }

            // No Sonar `cpp:S5356` and `cpp:S5357` b/c we integrate here with C libcanard API.
            const auto* const buffer =
                static_cast<const cetl::byte*>(tx_item->frame.payload);  // NOSONAR cpp:S5356 cpp:S5357
            const cetl::span<const cetl::byte> payload{buffer, tx_item->frame.payload_size};

            Expected<bool, MediaFailure> maybe_pushed =
                media.interface().push(deadline, tx_item->frame.extended_can_id, payload);

            // In case of media push error we are going to drop this problematic frame
            // (b/c it looks like media can't handle this frame),
            // but we will continue to process with other frames if transient error handler says so.
            // Note that media not being ready/able to push a frame just yet (aka temporary)
            // is not reported as an error (see `is_pushed` below).
            //
            if (auto* const media_failure = cetl::get_if<MediaFailure>(&maybe_pushed))
            {
                // Release whole problematic transfer from the TX queue,
                // so that other transfers in TX queue have their chance.
                // Otherwise, we would be stuck in a run loop trying to push the same frame.
                popAndFreeCanardTxQueueItem(&media.canard_tx_queue(), tx_item, true /*whole transfer*/);

                cetl::optional<AnyFailure> failure =
                    tryHandleTransientMediaError<TransientErrorReport::MediaPush>(media, std::move(*media_failure));
                if (failure.has_value())
                {
                    return failure;
                }

                // The handler just said that it's fine to continue with pushing other frames
                // and ignore such a transient media error (and don't propagate it outside).
            }
            else
            {
                const auto is_pushed = cetl::get<bool>(maybe_pushed);
                if (!is_pushed)
                {
                    // Media interface is busy, so we are done with this media for now,
                    // and will just try again with it later (on next `run`).
                    // Note, we are NOT releasing this item from the queue, so it will be retried on next `run`.
                    break;
                }

                popAndFreeCanardTxQueueItem(&media.canard_tx_queue(), tx_item, false /*single frame*/);
            }

        }  // for each frame

        return cetl::nullopt;
    }

    /// @brief Runs (if needed) reconfiguration of media filters based on the currently active RX ports.
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
    CETL_NODISCARD cetl::optional<AnyFailure> runMediaFilters()
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
            cetl::optional<MediaFailure> media_failure = media.interface().setFilters({filters.data(), filters.size()});
            if (media_failure.has_value())
            {
                was_error = true;

                cetl::optional<AnyFailure> failure =
                    tryHandleTransientMediaError<TransientErrorReport::MediaConfig>(media,
                                                                                    std::move(media_failure.value()));
                if (failure.has_value())
                {
                    // The handler (if any) just said that it's NOT fine to continue with configuring other media,
                    // and the error should not be ignored but propagated outside.
                    // Note that `should_reconfigure_filters_` still stays engaged, so we will try again on next run.
                    //
                    return failure;
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
        const auto        local_node_id      = static_cast<CanardNodeID>(node_id());
        const auto        is_anonymous       = local_node_id > CANARD_NODE_ID_MAX;
        const std::size_t total_active_ports = total_message_ports_ + (is_anonymous ? 0 : total_service_ports_);
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
        if ((total_service_ports_ > 0) && (!is_anonymous))
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
inline Expected<UniquePtr<ICanTransport>, FactoryFailure> makeTransport(cetl::pmr::memory_resource& memory,
                                                                        const cetl::span<IMedia*>   media,
                                                                        const std::size_t           tx_capacity)
{
    return detail::TransportImpl::make(memory, media, tx_capacity);
}

}  // namespace can
}  // namespace transport
}  // namespace libcyphal

#endif  // LIBCYPHAL_TRANSPORT_CAN_TRANSPORT_IMPL_HPP_INCLUDED
