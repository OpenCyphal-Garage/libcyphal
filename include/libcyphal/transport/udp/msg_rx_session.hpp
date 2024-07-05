/// @copyright
/// Copyright (C) OpenCyphal Development Team  <opencyphal.org>
/// Copyright Amazon.com Inc. or its affiliates.
/// SPDX-License-Identifier: MIT

#ifndef LIBCYPHAL_TRANSPORT_UDP_MSG_RX_SESSION_HPP_INCLUDED
#define LIBCYPHAL_TRANSPORT_UDP_MSG_RX_SESSION_HPP_INCLUDED

#include "delegate.hpp"
#include "session_tree.hpp"

#include "libcyphal/runnable.hpp"
#include "libcyphal/transport/errors.hpp"
#include "libcyphal/transport/msg_sessions.hpp"
#include "libcyphal/transport/types.hpp"
#include "libcyphal/types.hpp"

#include <cetl/cetl.hpp>
#include <cetl/pf17/cetlpf.hpp>
#include <udpard.h>

#include <chrono>
#include <cstdint>
#include <utility>

namespace libcyphal
{
namespace transport
{
namespace udp
{

/// Internal implementation details of the UDP transport.
/// Not supposed to be used directly by the users of the library.
///
namespace detail
{

/// @brief A class to represent a message subscriber RX session.
///
/// NOSONAR cpp:S4963 for below `class MessageRxSession` - we do directly handle resources here;
/// namely: in destructor we have to unsubscribe, as well as let transport delegate to know this fact.
///
class MessageRxSession final : private IMsgRxSessionDelegate, public IMessageRxSession  // NOSONAR cpp:S4963
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
    CETL_NODISCARD static Expected<UniquePtr<IMessageRxSession>, AnyFailure> make(
        cetl::pmr::memory_resource& memory,
        TransportDelegate&          delegate,
        const MessageRxParams&      params,
        RxSessionTreeNode::Message& rx_session_node)
    {
        if (params.subject_id > UDPARD_SUBJECT_ID_MAX)
        {
            return ArgumentError{};
        }

        auto session = libcyphal::detail::makeUniquePtr<Spec>(memory, Spec{}, delegate, params, rx_session_node);
        if (session == nullptr)
        {
            return MemoryError{};
        }

        return session;
    }

    MessageRxSession(const Spec,
                     TransportDelegate&          delegate,
                     const MessageRxParams&      params,
                     RxSessionTreeNode::Message& rx_session_node)
        : delegate_{delegate}
        , params_{params}
        , subscription_{}
    {
        const std::int8_t result = ::udpardRxSubscriptionInit(&subscription_,
                                                              params.subject_id,
                                                              params.extent_bytes,
                                                              delegate.makeUdpardRxMemoryResources());
        (void) result;
        CETL_DEBUG_ASSERT(result == 0, "There is no way currently to get an error here.");

        rx_session_node.delegate() = this;
    }

    MessageRxSession(const MessageRxSession&)                = delete;
    MessageRxSession(MessageRxSession&&) noexcept            = delete;
    MessageRxSession& operator=(const MessageRxSession&)     = delete;
    MessageRxSession& operator=(MessageRxSession&&) noexcept = delete;

    ~MessageRxSession()
    {
        ::udpardRxSubscriptionFree(&subscription_);

        delegate_.onSessionEvent(SessionEvent::Destroyed{params_.subject_id});
    }

private:
    using SessionEvent = TransportDelegate::SessionEvent::Message;

    // MARK: IMessageRxSession

    CETL_NODISCARD MessageRxParams getParams() const noexcept override
    {
        return params_;
    }

    CETL_NODISCARD cetl::optional<MessageRxTransfer> receive() override
    {
        return std::exchange(last_rx_transfer_, cetl::nullopt);
    }

    // MARK: IRxSession

    void setTransferIdTimeout(const Duration timeout) override
    {
        const auto timeout_us = std::chrono::duration_cast<std::chrono::microseconds>(timeout);
        if (timeout_us.count() > 0)
        {
            subscription_.port.transfer_id_timeout_usec = static_cast<UdpardMicrosecond>(timeout_us.count());
        }
    }

    // MARK: IRunnable

    IRunnable::MaybeFailure run(const TimePoint) override
    {
        // Nothing to do here currently.
        return {};
    }

    // MARK: IRxSessionDelegate

    void acceptRxTransfer(UdpardRxTransfer& inout_transfer) override
    {
        const auto priority  = static_cast<Priority>(inout_transfer.priority);
        const auto timestamp = TimePoint{std::chrono::microseconds{inout_transfer.timestamp_usec}};

        const cetl::optional<NodeId> publisher_node_id =
            inout_transfer.source_node_id > UDPARD_NODE_ID_MAX
                ? cetl::nullopt
                : cetl::make_optional<NodeId>(inout_transfer.source_node_id);

        TransportDelegate::UdpardMemory udpard_memory{delegate_, inout_transfer};

        const MessageTransferMetadata meta{inout_transfer.transfer_id, timestamp, priority, publisher_node_id};
        (void) last_rx_transfer_.emplace(MessageRxTransfer{meta, ScatteredBuffer{std::move(udpard_memory)}});
    }

    // MARK: IMsgRxSessionDelegate

    UdpardRxSubscription& getSubscription() override
    {
        return subscription_;
    }

    // MARK: Data members:

    TransportDelegate&                delegate_;
    const MessageRxParams             params_;
    UdpardRxSubscription              subscription_;
    cetl::optional<MessageRxTransfer> last_rx_transfer_;

};  // MessageRxSession

}  // namespace detail
}  // namespace udp
}  // namespace transport
}  // namespace libcyphal

#endif  // LIBCYPHAL_TRANSPORT_UDP_MSG_RX_SESSION_HPP_INCLUDED
