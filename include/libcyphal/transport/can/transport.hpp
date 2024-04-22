/// @copyright
/// Copyright (C) OpenCyphal Development Team  <opencyphal.org>
/// Copyright Amazon.com Inc. or its affiliates.
/// SPDX-License-Identifier: MIT

#ifndef LIBCYPHAL_TRANSPORT_CAN_TRANSPORT_HPP_INCLUDED
#define LIBCYPHAL_TRANSPORT_CAN_TRANSPORT_HPP_INCLUDED

#include "media.hpp"
#include "delegate.hpp"
#include "msg_rx_session.hpp"
#include "msg_tx_session.hpp"
#include "libcyphal/transport/transport.hpp"
#include "libcyphal/transport/multiplexer.hpp"

#include <canard.h>

#include <algorithm>
#include <chrono>

namespace libcyphal
{
namespace transport
{
namespace can
{

class ICanTransport : public ITransport
{};

namespace detail
{
class TransportImpl final : public ICanTransport, private TransportDelegate
{
    // In use to disable public construction.
    // See https://seanmiddleditch.github.io/enabling-make-unique-with-private-constructors/
    struct Tag
    {
        explicit Tag()  = default;
        using Interface = ICanTransport;
        using Concrete  = TransportImpl;
    };

public:
    CETL_NODISCARD static Expected<UniquePtr<ICanTransport>, FactoryError> make(
        cetl::pmr::memory_resource&  memory,
        IMultiplexer&                multiplexer,
        const cetl::span<IMedia*>    media,
        const std::size_t            tx_capacity,
        const cetl::optional<NodeId> local_node_id)
    {
        // Verify input arguments:
        // - At least one media interface must be provided, but no more than the maximum allowed (255).
        // - If a local node ID is provided, it must be within the valid range.
        //
        const auto media_count = static_cast<std::size_t>(
            std::count_if(media.begin(), media.end(), [](IMedia* const media) { return media != nullptr; }));
        if ((media_count == 0) || (media_count > std::numeric_limits<uint8_t>::max()))
        {
            return ArgumentError{};
        }
        if (local_node_id.has_value() && local_node_id.value() > CANARD_NODE_ID_MAX)
        {
            return ArgumentError{};
        }

        const auto canard_node_id = static_cast<CanardNodeID>(local_node_id.value_or(CANARD_NODE_ID_UNSET));

        auto transport = libcyphal::detail::makeUniquePtr<Tag>(memory,
                                                               Tag{},
                                                               memory,
                                                               multiplexer,
                                                               media_count,
                                                               media,
                                                               tx_capacity,
                                                               canard_node_id);
        if (transport == nullptr)
        {
            return MemoryError{};
        }

        return transport;
    }

    TransportImpl(Tag,
                  cetl::pmr::memory_resource& memory,
                  IMultiplexer&               multiplexer,
                  const std::size_t           media_count,
                  const cetl::span<IMedia*>   media_interfaces,
                  const std::size_t           tx_capacity,
                  const CanardNodeID          canard_node_id)
        : TransportDelegate{memory}
        , media_array_{make_media_array(memory, tx_capacity, media_count, media_interfaces)}
    {
        // TODO: Use it!
        (void) multiplexer;

        canard_instance().node_id = canard_node_id;
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
    CETL_NODISCARD ProtocolParams getProtocolParams() const noexcept override
    {
        const auto min_mtu =
            reduceMedia(std::numeric_limits<std::size_t>::max(),
                        [](const std::size_t mtu, IMedia& media) { return std::min(mtu, media.getMtu()); });

        return ProtocolParams{1 << CANARD_TRANSFER_ID_BIT_LENGTH, min_mtu, CANARD_NODE_ID_MAX + 1};
    }

    CETL_NODISCARD Expected<UniquePtr<IMessageRxSession>, AnyError> makeMessageRxSession(
        const MessageRxParams& params) override
    {
        auto any_error = ensureNewSessionFor(CanardTransferKindMessage, params.subject_id, CANARD_SUBJECT_ID_MAX);
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
        auto any_error = ensureNewSessionFor(CanardTransferKindRequest, params.service_id, CANARD_SERVICE_ID_MAX);
        if (any_error.has_value())
        {
            return any_error.value();
        }

        return NotImplementedError{};
    }

    CETL_NODISCARD Expected<UniquePtr<IRequestTxSession>, AnyError> makeRequestTxSession(
        const RequestTxParams&) override
    {
        return NotImplementedError{};
    }

    CETL_NODISCARD Expected<UniquePtr<IResponseRxSession>, AnyError> makeResponseRxSession(
        const ResponseRxParams& params) override
    {
        auto any_error = ensureNewSessionFor(CanardTransferKindResponse, params.service_id, CANARD_SERVICE_ID_MAX);
        if (any_error.has_value())
        {
            return any_error.value();
        }

        return NotImplementedError{};
    }

    CETL_NODISCARD Expected<UniquePtr<IResponseTxSession>, AnyError> makeResponseTxSession(
        const ResponseTxParams&) override
    {
        return NotImplementedError{};
    }

    // MARK: IRunnable

    void run(const TimePoint) override
    {
        std::array<cetl::byte, CANARD_MTU_MAX> payload{};

        for (const Media& media : media_array_)
        {
            // TODO: Handle errors.
            const auto pop_result = media.interface.pop(payload);
            if (const auto opt_rx_meta = cetl::get_if<cetl::optional<RxMetadata>>(&pop_result))
            {
                if (opt_rx_meta->has_value())
                {
                    const auto& rx_meta = opt_rx_meta->value();

                    const auto timestamp_us =
                        std::chrono::duration_cast<std::chrono::microseconds>(rx_meta.timestamp.time_since_epoch());
                    const CanardFrame canard_frame{rx_meta.can_id, rx_meta.payload_size, payload.cbegin()};

                    CanardRxTransfer      out_transfer{};
                    CanardRxSubscription* out_subscription{};

                    // TODO: Handle errors.
                    const auto result = canardRxAccept(&canard_instance(),
                                                       static_cast<CanardMicrosecond>(timestamp_us.count()),
                                                       &canard_frame,
                                                       media.index,
                                                       &out_transfer,
                                                       &out_subscription);
                    if (result > 0)
                    {
                        CETL_DEBUG_ASSERT(out_subscription != nullptr, "Expected subscription.");
                        CETL_DEBUG_ASSERT(out_subscription->user_reference != nullptr, "Expected session delegate.");

                        const auto delegate = static_cast<SessionDelegate*>(out_subscription->user_reference);
                        delegate->acceptRxTransfer(out_transfer);
                    }
                }
            }

        }  // for each media
    }

    // MARK: TransportDelegate

    CETL_NODISCARD inline TransportDelegate& asDelegate()
    {
        return *this;
    }

    // MARK: Privates:

    class Media final
    {
    public:
        Media(const std::size_t _index, IMedia& _interface, const std::size_t tx_capacity)
            : index{static_cast<uint8_t>(_index)}
            , interface{_interface}
            , canard_tx_queue{canardTxInit(tx_capacity, _interface.getMtu())}
        {
        }
        Media(const Media&)                = delete;
        Media(Media&&) noexcept            = default;
        Media& operator=(const Media&)     = delete;
        Media& operator=(Media&&) noexcept = delete;

        const uint8_t index;
        IMedia&       interface;
        CanardTxQueue canard_tx_queue;
    };
    using MediaArray = libcyphal::detail::VarArray<Media>;

    template <typename T, typename Reducer>
    CETL_NODISCARD T reduceMedia(const T init, Reducer reducer) const
    {
        T acc = init;
        for (const Media& media : media_array_)
        {
            acc = reducer(std::forward<T>(acc), media.interface);
        }
        return acc;
    }

    CETL_NODISCARD cetl::optional<AnyError> ensureNewSessionFor(const CanardTransferKind transfer_kind,
                                                                const PortId             port_id,
                                                                const PortId             max_port_id) noexcept
    {
        if (port_id > max_port_id)
        {
            return ArgumentError{};
        }

        const auto hasSubscription = canardRxHasSubscription(&canard_instance(), transfer_kind, port_id);
        if (hasSubscription < 0)
        {
            return anyErrorFromCanard(hasSubscription);
        }
        if (hasSubscription > 0)
        {
            return SessionAlreadyExistsError{};
        }

        return {};
    }

    static MediaArray make_media_array(cetl::pmr::memory_resource& memory,
                                       const std::size_t           tx_capacity,
                                       const std::size_t           media_count,
                                       const cetl::span<IMedia*>   media_interfaces)
    {
        MediaArray media_array{media_count, &memory};
        media_array.reserve(media_count);

        std::size_t index = 0;
        for (IMedia* const media_interface : media_interfaces)
        {
            if (media_interface != nullptr)
            {
                IMedia& media = *media_interface;
                media_array.emplace_back(index++, media, tx_capacity);
            }
        }
        CETL_DEBUG_ASSERT(!media_array.empty() && (media_array.size() == media_count) && (index == media_count), "");

        return media_array;
    }

    // MARK: Data members:

    const MediaArray media_array_;

};  // TransportImpl

}  // namespace detail

CETL_NODISCARD inline Expected<UniquePtr<ICanTransport>, FactoryError> makeTransport(
    cetl::pmr::memory_resource&  memory,
    IMultiplexer&                multiplexer,
    const cetl::span<IMedia*>    media,
    const std::size_t            tx_capacity,
    const cetl::optional<NodeId> local_node_id)
{
    return detail::TransportImpl::make(memory, multiplexer, media, tx_capacity, local_node_id);
}

}  // namespace can
}  // namespace transport
}  // namespace libcyphal

#endif  // LIBCYPHAL_TRANSPORT_CAN_TRANSPORT_HPP_INCLUDED
