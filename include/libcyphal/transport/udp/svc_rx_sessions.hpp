/// @copyright
/// Copyright (C) OpenCyphal Development Team  <opencyphal.org>
/// Copyright Amazon.com Inc. or its affiliates.
/// SPDX-License-Identifier: MIT

#ifndef LIBCYPHAL_TRANSPORT_UDP_SVC_RX_SESSIONS_HPP_INCLUDED
#define LIBCYPHAL_TRANSPORT_UDP_SVC_RX_SESSIONS_HPP_INCLUDED

#include "delegate.hpp"

#include "libcyphal/runnable.hpp"
#include "libcyphal/transport/errors.hpp"
#include "libcyphal/transport/svc_sessions.hpp"
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

/// @brief A template class to represent a service request/response RX session (both for server and client sides).
///
/// @tparam Interface_ Type of the session interface.
///                    Could be either `IRequestRxSession` or `IResponseRxSession`.
/// @tparam Params Type of the session parameters.
///                Could be either `RequestRxParams` or `ResponseRxParams`.
///
/// NOSONAR cpp:S4963 for below `class SvcRxSession` - we do directly handle resources here;
/// namely: in destructor we have to unsubscribe, as well as let delegate to know this fact.
///
template <typename Interface_, typename Params>
class SvcRxSession final : private IRxSessionDelegate, public Interface_  // NOSONAR cpp:S4963
{
    /// @brief Defines private specification for making interface unique ptr.
    ///
    struct Spec : libcyphal::detail::UniquePtrSpec<Interface_, SvcRxSession>
    {
        // `explicit` here is in use to disable public construction of derived private `Spec` structs.
        // See https://seanmiddleditch.github.io/enabling-make-unique-with-private-constructors/
        explicit Spec() = default;
    };

public:
    CETL_NODISCARD static Expected<UniquePtr<Interface_>, AnyError> make(cetl::pmr::memory_resource& memory,
                                                                         TransportDelegate&          delegate,
                                                                         const Params&               params)
    {
        if (params.service_id > UDPARD_SERVICE_ID_MAX)
        {
            return ArgumentError{};
        }

        auto session = libcyphal::detail::makeUniquePtr<Spec>(memory, Spec{}, delegate, params);
        if (session == nullptr)
        {
            return MemoryError{};
        }

        return session;
    }

    SvcRxSession(const Spec, TransportDelegate& delegate, const Params& params)
        : delegate_{delegate}
        , params_{params}
    {
        // TODO: Implement!
        (void) delegate_;
    }

    SvcRxSession(const SvcRxSession&)                = delete;
    SvcRxSession(SvcRxSession&&) noexcept            = delete;
    SvcRxSession& operator=(const SvcRxSession&)     = delete;
    SvcRxSession& operator=(SvcRxSession&&) noexcept = delete;

    ~SvcRxSession()
    {
        // TODO: Implement!
        (void) 0;
    }

private:
    // MARK: Interface

    CETL_NODISCARD Params getParams() const noexcept override
    {
        return params_;
    }

    CETL_NODISCARD cetl::optional<ServiceRxTransfer> receive() override
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
    const Params                      params_;
    cetl::optional<ServiceRxTransfer> last_rx_transfer_;

};  // SvcRxSession

// MARK: -

/// @brief A concrete class to represent a service request RX session (aka server side).
///
using SvcRequestRxSession = SvcRxSession<IRequestRxSession, RequestRxParams>;

/// @brief A concrete class to represent a service response RX session (aka client side).
///
using SvcResponseRxSession = SvcRxSession<IResponseRxSession, ResponseRxParams>;

}  // namespace detail
}  // namespace udp
}  // namespace transport
}  // namespace libcyphal

#endif  // LIBCYPHAL_TRANSPORT_UDP_SVC_RX_SESSIONS_HPP_INCLUDED
