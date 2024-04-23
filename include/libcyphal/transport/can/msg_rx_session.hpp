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
        cetl::optional<AnyError> any_error{};
        auto session = libcyphal::detail::makeUniquePtr<Tag>(delegate.memory(), Tag{}, delegate, params, any_error);
        if (any_error.has_value())
        {
            return any_error.value();
        }
        if (session == nullptr)
        {
            return MemoryError{};
        }

        return session;
    }

    MessageRxSession(Tag,
                     TransportDelegate&        delegate,
                     const MessageRxParams&    params,
                     cetl::optional<AnyError>& out_error)
        : delegate_{delegate}
        , params_{params}
    {
        const auto result = canardRxSubscribe(&delegate.canard_instance(),
                                              CanardTransferKindMessage,
                                              static_cast<CanardPortID>(params_.subject_id),
                                              static_cast<size_t>(params_.extent_bytes),
                                              CANARD_DEFAULT_TRANSFER_ID_TIMEOUT_USEC,
                                              &subscription_);
        if (result < 0)
        {
            out_error = TransportDelegate::anyErrorFromCanard(result);
            return;
        }
        CETL_DEBUG_ASSERT(result > 0, "New subscription supposed to be made.");

        is_subscribed_               = true;
        subscription_.user_reference = static_cast<SessionDelegate*>(this);
    }

    ~MessageRxSession() override
    {
        if (is_subscribed_)
        {
            canardRxUnsubscribe(&delegate_.canard_instance(),
                                CanardTransferKindMessage,
                                static_cast<CanardPortID>(params_.subject_id));
        }
    }

private:
    // MARK: IMessageRxSession

    CETL_NODISCARD MessageRxParams getParams() const noexcept override
    {
        return params_;
    }

    CETL_NODISCARD cetl::optional<MessageRxTransfer> receive() override
    {
        cetl::optional<MessageRxTransfer> result{};
        result.swap(last_rx_transfer_);
        return result;
    }

    // MARK: IRxSession

    void setTransferIdTimeout(const Duration timeout) override
    {
        const auto timeout_us = std::chrono::duration_cast<std::chrono::microseconds>(timeout);
        if (timeout_us.count() > 0)
        {
            subscription_.transfer_id_timeout_usec = static_cast<CanardMicrosecond>(timeout_us.count());
        }
    }

    // MARK: IRunnable

    void run(const TimePoint) override {}

    // MARK: SessionDelegate

    void acceptRxTransfer(const CanardRxTransfer& transfer) override
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

    bool                              is_subscribed_{false};
    CanardRxSubscription              subscription_{};
    cetl::optional<MessageRxTransfer> last_rx_transfer_{};

};  // MessageRxSession

}  // namespace detail
}  // namespace can
}  // namespace transport
}  // namespace libcyphal

#endif  // LIBCYPHAL_TRANSPORT_CAN_MSG_RX_SESSION_HPP_INCLUDED
