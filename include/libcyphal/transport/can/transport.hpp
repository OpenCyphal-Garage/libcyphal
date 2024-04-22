/// @copyright
/// Copyright (C) OpenCyphal Development Team  <opencyphal.org>
/// Copyright Amazon.com Inc. or its affiliates.
/// SPDX-License-Identifier: MIT

#ifndef LIBCYPHAL_TRANSPORT_CAN_TRANSPORT_HPP_INCLUDED
#define LIBCYPHAL_TRANSPORT_CAN_TRANSPORT_HPP_INCLUDED

#include "media.hpp"
#include "delegate.hpp"
#include "msg_rx_session.hpp"
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
        cetl::pmr::memory_resource&                    memory,
        IMultiplexer&                                  multiplexer,
        const std::array<IMedia*, MaxMediaInterfaces>& media,
        const cetl::optional<NodeId>                   local_node_id)
    {
        // Verify input arguments:
        // - At least one valid media interface must be provided.
        // - If a local node ID is provided, it must be within the valid range.
        //
        auto       valid_media_predicate = [](IMedia* const media) { return media != nullptr; };
        const auto media_count =
            static_cast<std::size_t>(std::count_if(media.cbegin(), media.cend(), valid_media_predicate));
        if (media_count == 0)
        {
            return ArgumentError{};
        }
        if (local_node_id.has_value() && local_node_id.value() > CANARD_NODE_ID_MAX)
        {
            return ArgumentError{};
        }

        libcyphal::detail::VarArray<IMedia*> media_array{MaxMediaInterfaces, &memory};
        media_array.reserve(media_count);
        std::copy_if(media.cbegin(), media.cend(), std::back_inserter(media_array), valid_media_predicate);
        CETL_DEBUG_ASSERT(!media_array.empty() && (media_array.size() == media_count), "");

        const auto canard_node_id = static_cast<CanardNodeID>(local_node_id.value_or(CANARD_NODE_ID_UNSET));

        auto transport = libcyphal::detail::makeUniquePtr<Tag>(memory,
                                                               Tag{},
                                                               memory,
                                                               multiplexer,
                                                               std::move(media_array),
                                                               canard_node_id);
        if (transport == nullptr)
        {
            return MemoryError{};
        }

        return transport;
    }

    TransportImpl(Tag,
                  cetl::pmr::memory_resource&            memory,
                  IMultiplexer&                          multiplexer,
                  libcyphal::detail::VarArray<IMedia*>&& media_array,
                  const CanardNodeID                     canard_node_id)
        : TransportDelegate{memory}
        , media_array_{std::move(media_array)}
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
        const MessageTxParams&) override
    {
        return NotImplementedError{};
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

        for (std::size_t media_index = 0; media_index < media_array_.size(); ++media_index)
        {
            CETL_DEBUG_ASSERT(media_array_[media_index] != nullptr, "Expected media interface.");
            IMedia& media = *media_array_[media_index];

            // TODO: Handle errors.
            const auto pop_result = media.pop(payload);
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
                                                       static_cast<uint8_t>(media_index),
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

    template <typename T, typename Reducer>
    CETL_NODISCARD T reduceMedia(const T init, Reducer reducer) const
    {
        T acc = init;
        for (IMedia* const media : media_array_)
        {
            CETL_DEBUG_ASSERT(media != nullptr, "Expected media interface.");
            acc = reducer(std::forward<T>(acc), std::ref(*media));
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

    // MARK: Data members:

    const libcyphal::detail::VarArray<IMedia*> media_array_;

};  // TransportImpl

}  // namespace detail

CETL_NODISCARD inline Expected<UniquePtr<ICanTransport>, FactoryError> makeTransport(
    cetl::pmr::memory_resource&                    memory,
    IMultiplexer&                                  multiplexer,
    const std::array<IMedia*, MaxMediaInterfaces>& media,  // TODO: replace with `cetl::span<IMedia*>`
    const cetl::optional<NodeId>                   local_node_id)
{
    return detail::TransportImpl::make(memory, multiplexer, media, local_node_id);
}

}  // namespace can
}  // namespace transport
}  // namespace libcyphal

#endif  // LIBCYPHAL_TRANSPORT_CAN_TRANSPORT_HPP_INCLUDED
