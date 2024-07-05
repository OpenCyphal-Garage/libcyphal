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

/// @brief A template class to represent a service request/response RX session (both for server and client sides).
///
/// @tparam Interface_ Type of the session interface.
///                    Could be either `IRequestRxSession` or `IResponseRxSession`.
/// @tparam Params Type of the session parameters.
///                Could be either `RequestRxParams` or `ResponseRxParams`.
///
/// NOSONAR cpp:S4963 for below `class SvcRxSession` - we do directly handle resources here;
/// namely: in destructor we have to unsubscribe, as well as let transport delegate to know this fact.
///
template <typename Interface_, typename Params, typename SessionEvent, bool IsRequest>
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
    CETL_NODISCARD static Expected<UniquePtr<Interface_>, AnyFailure> make(cetl::pmr::memory_resource& memory,
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
        , rpc_port_{}
    {
        const std::int8_t result = ::udpardRxRPCDispatcherListen(&delegate.getUdpardRpcDispatcher(),
                                                                 &rpc_port_,
                                                                 params.service_id,
                                                                 IsRequest,
                                                                 params.extent_bytes);
        (void) result;
        CETL_DEBUG_ASSERT(result >= 0, "There is no way currently to get an error here.");
        CETL_DEBUG_ASSERT(result == 1, "A new registration has been expected to be created.");

        // No Sonar `cpp:S5356` b/c we integrate here with C libudpard API.
        rpc_port_.user_reference = static_cast<IRxSessionDelegate*>(this);  // NOSONAR cpp:S5356
    }

    SvcRxSession(const SvcRxSession&)                = delete;
    SvcRxSession(SvcRxSession&&) noexcept            = delete;
    SvcRxSession& operator=(const SvcRxSession&)     = delete;
    SvcRxSession& operator=(SvcRxSession&&) noexcept = delete;

    ~SvcRxSession()
    {
        const std::int8_t result =
            ::udpardRxRPCDispatcherCancel(&delegate_.getUdpardRpcDispatcher(), params_.service_id, IsRequest);
        (void) result;
        CETL_DEBUG_ASSERT(result >= 0, "There is no way currently to get an error here.");
        CETL_DEBUG_ASSERT(result == 1, "Existing registration has been expected to be cancelled.");

        delegate_.onSessionEvent(typename SessionEvent::Destroyed{params_.service_id});
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
            rpc_port_.port.transfer_id_timeout_usec = static_cast<UdpardMicrosecond>(timeout_us.count());
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

        TransportDelegate::UdpardMemory udpard_memory{delegate_, inout_transfer};

        const ServiceTransferMetadata meta{inout_transfer.transfer_id,
                                           timestamp,
                                           priority,
                                           inout_transfer.source_node_id};
        (void) last_rx_transfer_.emplace(ServiceRxTransfer{meta, ScatteredBuffer{std::move(udpard_memory)}});
    }

    // MARK: Data members:

    TransportDelegate&                delegate_;
    const Params                      params_;
    UdpardRxRPCPort                   rpc_port_;
    cetl::optional<ServiceRxTransfer> last_rx_transfer_;

};  // SvcRxSession

// MARK: -

/// @brief A concrete class to represent a service request RX session (aka server side).
///
using SvcRequestRxSession =
    SvcRxSession<IRequestRxSession, RequestRxParams, TransportDelegate::SessionEvent::Request, true /*IsRequest*/>;

/// @brief A concrete class to represent a service response RX session (aka client side).
///
using SvcResponseRxSession =
    SvcRxSession<IResponseRxSession, ResponseRxParams, TransportDelegate::SessionEvent::Response, false /*IsRequest*/>;

}  // namespace detail
}  // namespace udp
}  // namespace transport
}  // namespace libcyphal

#endif  // LIBCYPHAL_TRANSPORT_UDP_SVC_RX_SESSIONS_HPP_INCLUDED
