/// @copyright
/// Copyright (C) OpenCyphal Development Team  <opencyphal.org>
/// Copyright Amazon.com Inc. or its affiliates.
/// SPDX-License-Identifier: MIT

#ifndef LIBCYPHAL_TRANSPORT_CAN_MSG_RX_SESSION_HPP_INCLUDED
#define LIBCYPHAL_TRANSPORT_CAN_MSG_RX_SESSION_HPP_INCLUDED

#include "delegate.hpp"
#include "libcyphal/transport/msg_sessions.hpp"

#include <canard.h>

#include <chrono>

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
class MessageRxSession final : public IMessageRxSession, private SessionDelegate
{
    // In use to disable public construction.
    // See https://seanmiddleditch.github.io/enabling-make-unique-with-private-constructors/
    struct Tag
    {
        explicit Tag()  = default;
        using Interface = IMessageRxSession;
        using Concrete  = MessageRxSession;
    };

public:
    CETL_NODISCARD static Expected<UniquePtr<IMessageRxSession>, AnyError> make(TransportDelegate&     delegate,
                                                                                const MessageRxParams& params)
    {
        auto session = libcyphal::detail::makeUniquePtr<Tag>(delegate.memory(), Tag{}, delegate, params);
        if (session == nullptr)
        {
            return MemoryError{};
        }

        return session;
    }

    MessageRxSession(Tag, TransportDelegate& delegate, const MessageRxParams& params)
        : delegate_{delegate}
        , params_{params}
    {
        const auto result = ::canardRxSubscribe(&delegate.canard_instance(),
                                                CanardTransferKindMessage,
                                                static_cast<CanardPortID>(params_.subject_id),
                                                static_cast<std::size_t>(params_.extent_bytes),
                                                CANARD_DEFAULT_TRANSFER_ID_TIMEOUT_USEC,
                                                &subscription_);
        (void) result;
        CETL_DEBUG_ASSERT(result >= 0, "There is no way currently to get an error here.");
        CETL_DEBUG_ASSERT(result > 0, "New subscription supposed to be made.");

        subscription_.user_reference = static_cast<SessionDelegate*>(this);
    }

    ~MessageRxSession() final
    {
        ::canardRxUnsubscribe(&delegate_.canard_instance(),
                              CanardTransferKindMessage,
                              static_cast<CanardPortID>(params_.subject_id));
    }

private:
    // MARK: IMessageRxSession

    CETL_NODISCARD MessageRxParams getParams() const noexcept final
    {
        return params_;
    }

    CETL_NODISCARD cetl::optional<MessageRxTransfer> receive() final
    {
        cetl::optional<MessageRxTransfer> result{};
        result.swap(last_rx_transfer_);
        return result;
    }

    // MARK: IRxSession

    void setTransferIdTimeout(const Duration timeout) final
    {
        const auto timeout_us = std::chrono::duration_cast<std::chrono::microseconds>(timeout);
        if (timeout_us.count() > 0)
        {
            subscription_.transfer_id_timeout_usec = static_cast<CanardMicrosecond>(timeout_us.count());
        }
    }

    // MARK: IRunnable

    void run(const TimePoint) final
    {
        // Nothing to do here currently.
    }

    // MARK: SessionDelegate

    void acceptRxTransfer(const CanardRxTransfer& transfer) final
    {
        const auto priority    = static_cast<Priority>(transfer.metadata.priority);
        const auto transfer_id = static_cast<TransferId>(transfer.metadata.transfer_id);
        const auto timestamp   = TimePoint{std::chrono::microseconds{transfer.timestamp_usec}};

        const cetl::optional<NodeId> publisher_node_id =
            transfer.metadata.remote_node_id > CANARD_NODE_ID_MAX
                ? cetl::nullopt
                : cetl::make_optional<NodeId>(transfer.metadata.remote_node_id);

        const MessageTransferMetadata   meta{{transfer_id, timestamp, priority}, publisher_node_id};
        TransportDelegate::CanardMemory canard_memory{delegate_, transfer.payload, transfer.payload_size};

        last_rx_transfer_.emplace(MessageRxTransfer{meta, ScatteredBuffer{std::move(canard_memory)}});
    }

    // MARK: Data members:

    TransportDelegate&    delegate_;
    const MessageRxParams params_;

    CanardRxSubscription              subscription_{};
    cetl::optional<MessageRxTransfer> last_rx_transfer_{};

};  // MessageRxSession

}  // namespace detail
}  // namespace can
}  // namespace transport
}  // namespace libcyphal

#endif  // LIBCYPHAL_TRANSPORT_CAN_MSG_RX_SESSION_HPP_INCLUDED