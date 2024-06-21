/// @copyright
/// Copyright (C) OpenCyphal Development Team  <opencyphal.org>
/// Copyright Amazon.com Inc. or its affiliates.
/// SPDX-License-Identifier: MIT

#ifndef LIBCYPHAL_TRANSPORT_UDP_MSG_RX_SESSION_HPP_INCLUDED
#define LIBCYPHAL_TRANSPORT_UDP_MSG_RX_SESSION_HPP_INCLUDED

#include "delegate.hpp"

#include "libcyphal/runnable.hpp"
#include "libcyphal/transport/errors.hpp"
#include "libcyphal/transport/msg_sessions.hpp"
#include "libcyphal/transport/types.hpp"
#include "libcyphal/types.hpp"

#include <cetl/cetl.hpp>
#include <cetl/pf17/cetlpf.hpp>
#include <udpard.h>

#include <chrono>
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
/// namely: in destructor we have to unsubscribe, as well as let delegate to know this fact.
///
class MessageRxSession final : private IRxSessionDelegate, public IMessageRxSession  // NOSONAR cpp:S4963
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
    CETL_NODISCARD static Expected<UniquePtr<IMessageRxSession>, AnyError> make(TransportDelegate&     delegate,
                                                                                const MessageRxParams& params)
    {
        if (params.subject_id > UDPARD_SUBJECT_ID_MAX)
        {
            return ArgumentError{};
        }

        auto session =
            libcyphal::detail::makeUniquePtr<Spec>(delegate.memoryResources().general, Spec{}, delegate, params);
        if (session == nullptr)
        {
            return MemoryError{};
        }

        return session;
    }

    MessageRxSession(const Spec, TransportDelegate& delegate, const MessageRxParams& params)
        : delegate_{delegate}
        , params_{params}
    {
        // TODO: Implement!
        (void) delegate_;
    }

    MessageRxSession(const MessageRxSession&)                = delete;
    MessageRxSession(MessageRxSession&&) noexcept            = delete;
    MessageRxSession& operator=(const MessageRxSession&)     = delete;
    MessageRxSession& operator=(MessageRxSession&&) noexcept = delete;

    ~MessageRxSession()
    {
        // TODO: Implement!
        (void) 0;
    }

private:
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
            // TODO: Implement!
        }
    }

    // MARK: IRunnable

    IRunnable::MaybeError run(const TimePoint) override
    {
        // Nothing to do here currently.
        return {};
    }

    // MARK: IRxSessionDelegate

    void acceptRxTransfer(const UdpardRxTransfer&) override
    {
        // TODO: Implement!
    }

    // MARK: Data members:

    TransportDelegate&                delegate_;
    const MessageRxParams             params_;
    cetl::optional<MessageRxTransfer> last_rx_transfer_;

};  // MessageRxSession

}  // namespace detail
}  // namespace udp
}  // namespace transport
}  // namespace libcyphal

#endif  // LIBCYPHAL_TRANSPORT_UDP_MSG_RX_SESSION_HPP_INCLUDED
