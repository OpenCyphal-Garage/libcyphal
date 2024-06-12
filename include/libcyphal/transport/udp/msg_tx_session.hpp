/// @copyright
/// Copyright (C) OpenCyphal Development Team  <opencyphal.org>
/// Copyright Amazon.com Inc. or its affiliates.
/// SPDX-License-Identifier: MIT

#ifndef LIBCYPHAL_TRANSPORT_UDP_MSG_TX_SESSION_HPP_INCLUDED
#define LIBCYPHAL_TRANSPORT_UDP_MSG_TX_SESSION_HPP_INCLUDED

#include "delegate.hpp"

#include "libcyphal/runnable.hpp"
#include "libcyphal/transport/errors.hpp"
#include "libcyphal/transport/msg_sessions.hpp"
#include "libcyphal/transport/types.hpp"
#include "libcyphal/types.hpp"

#include <cetl/cetl.hpp>
#include <cetl/pf17/cetlpf.hpp>
#include <udpard.h>

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

class MessageTxSession final : public IMessageTxSession
{
    /// @brief Defines specification for making interface unique ptr.
    ///
    struct Spec
    {
        using Interface = IMessageTxSession;
        using Concrete  = MessageTxSession;

        // In use to disable public construction.
        // See https://seanmiddleditch.github.io/enabling-make-unique-with-private-constructors/
        explicit Spec() = default;
    };

public:
    CETL_NODISCARD static Expected<UniquePtr<IMessageTxSession>, AnyError> make(TransportDelegate&     delegate,
                                                                                const MessageTxParams& params)
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

    MessageTxSession(Spec, TransportDelegate& delegate, const MessageTxParams& params)
        : delegate_{delegate}
        , params_{params}
        , send_timeout_{std::chrono::seconds{1}}
    {
    }

private:
    // MARK: ITxSession

    void setSendTimeout(const Duration timeout) override
    {
        send_timeout_ = timeout;
    }

    // MARK: IMessageTxSession

    CETL_NODISCARD MessageTxParams getParams() const noexcept override
    {
        return params_;
    }

    CETL_NODISCARD cetl::optional<AnyError> send(const TransferMetadata&, const PayloadFragments) override
    {
        // TODO: Implement!
        (void) delegate_;
        return NotImplementedError{};
    }

    // MARK: IRunnable

    MaybeError run(const TimePoint) override
    {
        // Nothing to do here currently.
        return {};
    }

    // MARK: Data members:

    TransportDelegate&    delegate_;
    const MessageTxParams params_;
    Duration              send_timeout_;

};  // MessageTxSession

}  // namespace detail
}  // namespace udp
}  // namespace transport
}  // namespace libcyphal

#endif  // LIBCYPHAL_TRANSPORT_UDP_MSG_TX_SESSION_HPP_INCLUDED
