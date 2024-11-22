/// @copyright
/// Copyright (C) OpenCyphal Development Team  <opencyphal.org>
/// Copyright Amazon.com Inc. or its affiliates.
/// SPDX-License-Identifier: MIT

#ifndef LIBCYPHAL_TRANSPORT_CAN_MSG_RX_SESSION_HPP_INCLUDED
#define LIBCYPHAL_TRANSPORT_CAN_MSG_RX_SESSION_HPP_INCLUDED

#include "delegate.hpp"

#include "libcyphal/errors.hpp"
#include "libcyphal/transport/errors.hpp"
#include "libcyphal/transport/msg_sessions.hpp"
#include "libcyphal/transport/types.hpp"
#include "libcyphal/types.hpp"

#include <canard.h>
#include <cetl/cetl.hpp>
#include <cetl/pf17/cetlpf.hpp>

#include <chrono>
#include <cstddef>
#include <cstdint>
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

/// @brief A class to represent a message subscriber RX session.
///
/// NOSONAR cpp:S4963 for below `class MessageRxSession` - we do directly handle resources here;
/// namely: in destructor we have to unsubscribe, as well as let transport delegate to know this fact.
///
class MessageRxSession final : IRxSessionDelegate, public IMessageRxSession  // NOSONAR cpp:S4963
{
    /// @brief Defines private specification for making interface unique ptr.
    ///
    struct Spec : libcyphal::detail::UniquePtrSpec<IMessageRxSession, MessageRxSession>
    {
        // `explicit` here is in use to disable public construction of derived private `Spec` structs.
        // See https://seanmiddleditch.github.io/enabling-make-unique-with-private-constructors/
        explicit Spec() = default;
    };

public:
    CETL_NODISCARD static Expected<UniquePtr<IMessageRxSession>, AnyFailure> make(TransportDelegate&     delegate,
                                                                                  const MessageRxParams& params)
    {
        if (params.subject_id > CANARD_SUBJECT_ID_MAX)
        {
            return ArgumentError{};
        }

        auto session = libcyphal::detail::makeUniquePtr<Spec>(delegate.memory(), Spec{}, delegate, params);
        if (session == nullptr)
        {
            return MemoryError{};
        }

        return session;
    }

    MessageRxSession(const Spec, TransportDelegate& delegate, const MessageRxParams& params)
        : delegate_{delegate}
        , params_{params}
        , subscription_{}
    {
        const std::int8_t result = ::canardRxSubscribe(&delegate.canard_instance(),
                                                       CanardTransferKindMessage,
                                                       params_.subject_id,
                                                       params_.extent_bytes,
                                                       CANARD_DEFAULT_TRANSFER_ID_TIMEOUT_USEC,
                                                       &subscription_);
        (void) result;
        CETL_DEBUG_ASSERT(result >= 0, "There is no way currently to get an error here.");
        CETL_DEBUG_ASSERT(result > 0, "New subscription supposed to be made.");

        // No Sonar `cpp:S5356` b/c we integrate here with C libcanard API.
        subscription_.user_reference = static_cast<IRxSessionDelegate*>(this);  // NOSONAR cpp:S5356

        delegate_.onSessionEvent(TransportDelegate::SessionEvent::MsgRxLifetime{true /* is_added */});
    }

    MessageRxSession(const MessageRxSession&)                = delete;
    MessageRxSession(MessageRxSession&&) noexcept            = delete;
    MessageRxSession& operator=(const MessageRxSession&)     = delete;
    MessageRxSession& operator=(MessageRxSession&&) noexcept = delete;

    ~MessageRxSession()
    {
        const std::int8_t result =
            ::canardRxUnsubscribe(&delegate_.canard_instance(), CanardTransferKindMessage, params_.subject_id);
        (void) result;
        CETL_DEBUG_ASSERT(result >= 0, "There is no way currently to get an error here.");
        CETL_DEBUG_ASSERT(result > 0, "Subscription supposed to be made at constructor.");

        delegate_.onSessionEvent(TransportDelegate::SessionEvent::MsgRxLifetime{false /* is_added */});
    }

private:
    // MARK: IMessageRxSession

    CETL_NODISCARD MessageRxParams getParams() const noexcept override
    {
        return params_;
    }

    CETL_NODISCARD cetl::optional<MessageRxTransfer> receive() override
    {
        if (last_rx_transfer_)
        {
            auto transfer = std::move(*last_rx_transfer_);
            last_rx_transfer_.reset();
            return transfer;
        }
        return cetl::nullopt;
    }

    void setOnReceiveCallback(OnReceiveCallback::Function&& function) override
    {
        on_receive_cb_fn_ = std::move(function);
    }

    // MARK: IRxSession

    void setTransferIdTimeout(const Duration timeout) override
    {
        const auto timeout_us = std::chrono::duration_cast<std::chrono::microseconds>(timeout);
        if (timeout_us >= Duration::zero())
        {
            subscription_.transfer_id_timeout_usec = static_cast<CanardMicrosecond>(timeout_us.count());
        }
    }

    // MARK: IRxSessionDelegate

    void acceptRxTransfer(const CanardRxTransfer& transfer) override
    {
        const auto priority    = static_cast<Priority>(transfer.metadata.priority);
        const auto transfer_id = static_cast<TransferId>(transfer.metadata.transfer_id);
        const auto timestamp   = TimePoint{std::chrono::microseconds{transfer.timestamp_usec}};

        const cetl::optional<NodeId> publisher_node_id =
            transfer.metadata.remote_node_id > CANARD_NODE_ID_MAX
                ? cetl::nullopt
                : cetl::make_optional<NodeId>(transfer.metadata.remote_node_id);

        // No Sonar `cpp:S5356` and `cpp:S5357` b/c we need to pass raw data from C libcanard api.
        auto* const buffer = static_cast<cetl::byte*>(transfer.payload.data);  // NOSONAR cpp:S5356 cpp:S5357
        TransportDelegate::CanardMemory canard_memory{delegate_,
                                                      transfer.payload.allocated_size,
                                                      buffer,
                                                      transfer.payload.size};

        const MessageRxMetadata meta{{{transfer_id, priority}, timestamp}, publisher_node_id};
        MessageRxTransfer       msg_rx_transfer{meta, ScatteredBuffer{std::move(canard_memory)}};
        if (on_receive_cb_fn_)
        {
            on_receive_cb_fn_(OnReceiveCallback::Arg{msg_rx_transfer});
            return;
        }
        (void) last_rx_transfer_.emplace(std::move(msg_rx_transfer));
    }

    // MARK: Data members:

    TransportDelegate&                delegate_;
    const MessageRxParams             params_;
    CanardRxSubscription              subscription_;
    cetl::optional<MessageRxTransfer> last_rx_transfer_;
    OnReceiveCallback::Function       on_receive_cb_fn_;

};  // MessageRxSession

}  // namespace detail
}  // namespace can
}  // namespace transport
}  // namespace libcyphal

#endif  // LIBCYPHAL_TRANSPORT_CAN_MSG_RX_SESSION_HPP_INCLUDED
