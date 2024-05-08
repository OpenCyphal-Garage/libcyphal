/// @copyright
/// Copyright (C) OpenCyphal Development Team  <opencyphal.org>
/// Copyright Amazon.com Inc. or its affiliates.
/// SPDX-License-Identifier: MIT

#ifndef LIBCYPHAL_TRANSPORT_CAN_SVC_RX_SESSIONS_HPP_INCLUDED
#define LIBCYPHAL_TRANSPORT_CAN_SVC_RX_SESSIONS_HPP_INCLUDED

#include "delegate.hpp"
#include "libcyphal/transport/svc_sessions.hpp"

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

/// @brief A template class to represent a service request/response RX session (both for server and client sides).
///
/// @tparam Interface_ Type of the session interface.
///                    Could be either `IRequestRxSession` or `IResponseRxSession`.
/// @tparam Params Type of the session parameters.
///                Could be either `RequestRxParams` or `ResponseRxParams`.
/// @tparam TransferKind Kind of the service transfer.
///                      Could be either `CanardTransferKindRequest` or `CanardTransferKindResponse`.
///
template <typename Interface_, typename Params, CanardTransferKind TransferKind>
class SvcRxSession final : public Interface_, private IRxSessionDelegate
{
    /// @brief Defines specification for making interface unique ptr.
    ///
    struct Spec
    {
        using Interface = Interface_;
        using Concrete  = SvcRxSession;

        // In use to disable public construction.
        // See https://seanmiddleditch.github.io/enabling-make-unique-with-private-constructors/
        explicit Spec() = default;
    };

public:
    CETL_NODISCARD static Expected<UniquePtr<Interface_>, AnyError> make(TransportDelegate& delegate,
                                                                         const Params&      params)
    {
        if (params.service_id > CANARD_SERVICE_ID_MAX)
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

    SvcRxSession(Spec, TransportDelegate& delegate, const Params& params)
        : delegate_{delegate}
        , params_{params}
        , subscription_{}
        , last_rx_transfer_{}
    {
        const int8_t result = ::canardRxSubscribe(&delegate.canard_instance(),
                                                  TransferKind,
                                                  static_cast<CanardPortID>(params_.service_id),
                                                  static_cast<std::size_t>(params_.extent_bytes),
                                                  CANARD_DEFAULT_TRANSFER_ID_TIMEOUT_USEC,
                                                  &subscription_);
        (void) result;
        CETL_DEBUG_ASSERT(result >= 0, "There is no way currently to get an error here.");
        CETL_DEBUG_ASSERT(result > 0, "New subscription supposed to be made.");

        subscription_.user_reference = static_cast<IRxSessionDelegate*>(this);

        delegate_.triggerUpdateOfFilters(TransportDelegate::FiltersUpdateCondition::ServicePortAdded);
    }

    ~SvcRxSession() final
    {
        const int8_t result = ::canardRxUnsubscribe(&delegate_.canard_instance(),
                                                    TransferKind,
                                                    static_cast<CanardPortID>(params_.service_id));
        (void) result;
        CETL_DEBUG_ASSERT(result >= 0, "There is no way currently to get an error here.");
        CETL_DEBUG_ASSERT(result > 0, "Subscription supposed to be made at constructor.");

        delegate_.triggerUpdateOfFilters(TransportDelegate::FiltersUpdateCondition::ServicePortRemoved);
    }

private:
    // MARK: Interface

    CETL_NODISCARD Params getParams() const noexcept final
    {
        return params_;
    }

    CETL_NODISCARD cetl::optional<ServiceRxTransfer> receive() final
    {
        cetl::optional<ServiceRxTransfer> result{};
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

    // MARK: IRxSessionDelegate

    void acceptRxTransfer(const CanardRxTransfer& transfer) final
    {
        const auto priority       = static_cast<Priority>(transfer.metadata.priority);
        const auto remote_node_id = static_cast<NodeId>(transfer.metadata.remote_node_id);
        const auto transfer_id    = static_cast<TransferId>(transfer.metadata.transfer_id);
        const auto timestamp      = TimePoint{std::chrono::microseconds{transfer.timestamp_usec}};

        const ServiceTransferMetadata   meta{{transfer_id, timestamp, priority}, remote_node_id};
        TransportDelegate::CanardMemory canard_memory{delegate_, transfer.payload, transfer.payload_size};

        last_rx_transfer_.emplace(ServiceRxTransfer{meta, ScatteredBuffer{std::move(canard_memory)}});
    }

    // MARK: Data members:

    TransportDelegate&                delegate_;
    const Params                      params_;
    CanardRxSubscription              subscription_;
    cetl::optional<ServiceRxTransfer> last_rx_transfer_;

};  // SvcRxSession

// MARK: -

/// @brief A concrete class to represent a service request RX session (aka server side).
///
using SvcRequestRxSession = SvcRxSession<IRequestRxSession, RequestRxParams, CanardTransferKindRequest>;

/// @brief A concrete class to represent a service response RX session (aka client side).
///
using SvcResponseRxSession = SvcRxSession<IResponseRxSession, ResponseRxParams, CanardTransferKindResponse>;

}  // namespace detail
}  // namespace can
}  // namespace transport
}  // namespace libcyphal

#endif  // LIBCYPHAL_TRANSPORT_CAN_SVC_RX_SESSIONS_HPP_INCLUDED
