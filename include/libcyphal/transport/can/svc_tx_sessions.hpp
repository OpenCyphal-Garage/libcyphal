/// @copyright
/// Copyright (C) OpenCyphal Development Team  <opencyphal.org>
/// Copyright Amazon.com Inc. or its affiliates.
/// SPDX-License-Identifier: MIT

#ifndef LIBCYPHAL_TRANSPORT_CAN_SVC_TX_SESSIONS_HPP_INCLUDED
#define LIBCYPHAL_TRANSPORT_CAN_SVC_TX_SESSIONS_HPP_INCLUDED

#include "delegate.hpp"
#include "libcyphal/transport/svc_sessions.hpp"
#include "libcyphal/transport/contiguous_payload.hpp"

#include <canard.h>

#include <numeric>

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

/// @brief A class to represent a service request TX session (aka client side).
///
class SvcRequestTxSession final : public IRequestTxSession
{
    // In use to disable public construction.
    // See https://seanmiddleditch.github.io/enabling-make-unique-with-private-constructors/
    struct Tag
    {
        explicit Tag()  = default;
        using Interface = IRequestTxSession;
        using Concrete  = SvcRequestTxSession;
    };

public:
    CETL_NODISCARD static Expected<UniquePtr<IRequestTxSession>, AnyError> make(TransportDelegate&     delegate,
                                                                                const RequestTxParams& params)
    {
        if ((params.service_id > CANARD_SERVICE_ID_MAX) || (params.server_node_id > CANARD_NODE_ID_MAX))
        {
            return ArgumentError{};
        }

        auto session = libcyphal::detail::makeUniquePtr<Tag>(delegate.memory(), Tag{}, delegate, params);
        if (session == nullptr)
        {
            return MemoryError{};
        }

        return session;
    }

    SvcRequestTxSession(Tag, TransportDelegate& delegate, const RequestTxParams& params)
        : delegate_{delegate}
        , params_{params}
        , send_timeout_{std::chrono::seconds{1}}
    {
    }

private:
    // MARK: ITxSession

    void setSendTimeout(const Duration timeout) final
    {
        send_timeout_ = timeout;
    }

    // MARK: IRequestTxSession

    CETL_NODISCARD RequestTxParams getParams() const noexcept final
    {
        return params_;
    }

    CETL_NODISCARD cetl::optional<AnyError> send(const TransferMetadata& metadata,
                                                 const PayloadFragments  payload_fragments) final
    {
        // Before delegating to transport it makes sense to do some sanity checks.
        // Otherwise, transport may do some work (like possible payload allocation/copying,
        // media enumeration and pushing into their TX queues) doomed to fail with argument error.
        //
        const CanardNodeID local_node_id = delegate_.canard_instance().node_id;
        if (local_node_id > CANARD_NODE_ID_MAX)
        {
            return ArgumentError{};
        }

        const auto canard_metadata = CanardTransferMetadata{static_cast<CanardPriority>(metadata.priority),
                                                            CanardTransferKindRequest,
                                                            static_cast<CanardPortID>(params_.service_id),
                                                            static_cast<CanardNodeID>(params_.server_node_id),
                                                            static_cast<CanardTransferID>(metadata.transfer_id)};

        return delegate_.sendTransfer(metadata.timestamp + send_timeout_, canard_metadata, payload_fragments);
    }

    // MARK: IRunnable

    void run(const TimePoint) final
    {
        // Nothing to do here currently.
    }

    // MARK: Data members:

    TransportDelegate&    delegate_;
    const RequestTxParams params_;
    Duration              send_timeout_;

};  // SvcRequestTxSession

// MARK: -

/// @brief A class to represent a service response TX session (aka server side).
///
class SvcResponseTxSession final : public IResponseTxSession
{
    // In use to disable public construction.
    // See https://seanmiddleditch.github.io/enabling-make-unique-with-private-constructors/
    struct Tag
    {
        explicit Tag()  = default;
        using Interface = IResponseTxSession;
        using Concrete  = SvcResponseTxSession;
    };

public:
    CETL_NODISCARD static Expected<UniquePtr<IResponseTxSession>, AnyError> make(TransportDelegate&      delegate,
                                                                                 const ResponseTxParams& params)
    {
        if (params.service_id > CANARD_SERVICE_ID_MAX)
        {
            return ArgumentError{};
        }

        auto session = libcyphal::detail::makeUniquePtr<Tag>(delegate.memory(), Tag{}, delegate, params);
        if (session == nullptr)
        {
            return MemoryError{};
        }

        return session;
    }

    SvcResponseTxSession(Tag, TransportDelegate& delegate, const ResponseTxParams& params)
        : delegate_{delegate}
        , params_{params}
        , send_timeout_{std::chrono::seconds{1}}
    {
    }

private:
    // MARK: ITxSession

    void setSendTimeout(const Duration timeout) final
    {
        send_timeout_ = timeout;
    }

    // MARK: IResponseTxSession

    CETL_NODISCARD ResponseTxParams getParams() const noexcept final
    {
        return params_;
    }

    CETL_NODISCARD cetl::optional<AnyError> send(const ServiceTransferMetadata& metadata,
                                                 const PayloadFragments         payload_fragments) final
    {
        // Before delegating to transport it makes sense to do some sanity checks.
        // Otherwise, transport may do some work (like possible payload allocation/copying,
        // media enumeration and pushing into their TX queues) doomed to fail with argument error.
        //
        const CanardNodeID local_node_id = delegate_.canard_instance().node_id;
        if ((local_node_id > CANARD_NODE_ID_MAX) || (metadata.remote_node_id > CANARD_NODE_ID_MAX))
        {
            return ArgumentError{};
        }

        const auto canard_metadata = CanardTransferMetadata{static_cast<CanardPriority>(metadata.priority),
                                                            CanardTransferKindResponse,
                                                            static_cast<CanardPortID>(params_.service_id),
                                                            static_cast<CanardNodeID>(metadata.remote_node_id),
                                                            static_cast<CanardTransferID>(metadata.transfer_id)};

        return delegate_.sendTransfer(metadata.timestamp + send_timeout_, canard_metadata, payload_fragments);
    }

    // MARK: IRunnable

    void run(const TimePoint) final
    {
        // Nothing to do here currently.
    }

    // MARK: Data members:

    TransportDelegate&     delegate_;
    const ResponseTxParams params_;
    Duration               send_timeout_;

};  // SvcResponseTxSession

}  // namespace detail
}  // namespace can
}  // namespace transport
}  // namespace libcyphal

#endif  // LIBCYPHAL_TRANSPORT_CAN_SVC_TX_SESSIONS_HPP_INCLUDED
