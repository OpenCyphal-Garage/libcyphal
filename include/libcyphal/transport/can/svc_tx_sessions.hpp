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

/// @brief A class to represent a service request TX session.
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
        // libcanard currently does not support fragmented payloads (at `canardTxPush`).
        // so we need to concatenate them when there are more than one non-empty fragment.
        // See https://github.com/OpenCyphal/libcanard/issues/223
        //
        const transport::detail::ContiguousPayload contiguous_payload{delegate_.memory(), payload_fragments};
        if ((contiguous_payload.data() == nullptr) && (contiguous_payload.size() > 0))
        {
            return MemoryError{};
        }

        const TimePoint deadline = metadata.timestamp + send_timeout_;
        const auto deadline_us   = std::chrono::duration_cast<std::chrono::microseconds>(deadline.time_since_epoch());

        const auto canard_metadata = CanardTransferMetadata{static_cast<CanardPriority>(metadata.priority),
                                                            CanardTransferKindRequest,
                                                            static_cast<CanardPortID>(params_.service_id),
                                                            delegate_.canard_instance().node_id,
                                                            static_cast<CanardTransferID>(metadata.transfer_id)};

        return delegate_.sendTransfer(static_cast<CanardMicrosecond>(deadline_us.count()),
                                      canard_metadata,
                                      contiguous_payload.data(),
                                      contiguous_payload.size());
    }

    // MARK: IRunnable

    void run(const TimePoint) final
    {
        // Nothing to do here currently.
    }

    // MARK: Data members:

    TransportDelegate&    delegate_;
    const RequestTxParams params_;
    Duration              send_timeout_ = std::chrono::seconds{1};

};  // SvcRequestTxSession

/// @brief A class to represent a service response TX session.
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
        // libcanard currently does not support fragmented payloads (at `canardTxPush`).
        // so we need to concatenate them when there are more than one non-empty fragment.
        // See https://github.com/OpenCyphal/libcanard/issues/223
        //
        const transport::detail::ContiguousPayload contiguous_payload{delegate_.memory(), payload_fragments};
        if ((contiguous_payload.data() == nullptr) && (contiguous_payload.size() > 0))
        {
            return MemoryError{};
        }

        const TimePoint deadline = metadata.timestamp + send_timeout_;
        const auto deadline_us   = std::chrono::duration_cast<std::chrono::microseconds>(deadline.time_since_epoch());

        const auto canard_metadata = CanardTransferMetadata{static_cast<CanardPriority>(metadata.priority),
                                                            CanardTransferKindResponse,
                                                            static_cast<CanardPortID>(params_.service_id),
                                                            delegate_.canard_instance().node_id,
                                                            static_cast<CanardTransferID>(metadata.transfer_id)};

        return delegate_.sendTransfer(static_cast<CanardMicrosecond>(deadline_us.count()),
                                      canard_metadata,
                                      contiguous_payload.data(),
                                      contiguous_payload.size());
    }

    // MARK: IRunnable

    void run(const TimePoint) final
    {
        // Nothing to do here currently.
    }

    // MARK: Data members:

    TransportDelegate&     delegate_;
    const ResponseTxParams params_;
    Duration               send_timeout_ = std::chrono::seconds{1};

};  // SvcResponseTxSession

}  // namespace detail
}  // namespace can
}  // namespace transport
}  // namespace libcyphal

#endif  // LIBCYPHAL_TRANSPORT_CAN_SVC_TX_SESSIONS_HPP_INCLUDED
