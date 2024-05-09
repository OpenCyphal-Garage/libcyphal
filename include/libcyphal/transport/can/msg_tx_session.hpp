/// @copyright
/// Copyright (C) OpenCyphal Development Team  <opencyphal.org>
/// Copyright Amazon.com Inc. or its affiliates.
/// SPDX-License-Identifier: MIT

#ifndef LIBCYPHAL_TRANSPORT_CAN_MSG_TX_SESSION_HPP_INCLUDED
#define LIBCYPHAL_TRANSPORT_CAN_MSG_TX_SESSION_HPP_INCLUDED

#include "delegate.hpp"

#include "libcyphal/transport/errors.hpp"
#include "libcyphal/transport/msg_sessions.hpp"
#include "libcyphal/transport/types.hpp"
#include "libcyphal/types.hpp"

#include <canard.h>
#include <cetl/pf17/attribute.hpp>
#include <cetl/pf17/cetlpf.hpp>

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

    MessageTxSession(Spec, TransportDelegate& delegate, const MessageTxParams& params)
        : delegate_{delegate}
        , params_{params}
        , send_timeout_{std::chrono::seconds{1}}  // NOSONAR : Sonar's cpp:S134 conflicts with AUTOSAR A12-1-2
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

    CETL_NODISCARD cetl::optional<AnyError> send(const TransferMetadata& metadata,
                                                 const PayloadFragments  payload_fragments) override
    {
        const auto canard_metadata = CanardTransferMetadata{static_cast<CanardPriority>(metadata.priority),
                                                            CanardTransferKindMessage,
                                                            params_.subject_id,
                                                            CANARD_NODE_ID_UNSET,
                                                            static_cast<CanardTransferID>(metadata.transfer_id)};

        return delegate_.sendTransfer(metadata.timestamp + send_timeout_, canard_metadata, payload_fragments);
    }

    // MARK: IRunnable

    void run(const TimePoint) override
    {
        // Nothing to do here currently.
    }

    // MARK: Data members:

    TransportDelegate&    delegate_;
    const MessageTxParams params_;
    Duration              send_timeout_;

};  // MessageTxSession

}  // namespace detail
}  // namespace can
}  // namespace transport
}  // namespace libcyphal

#endif  // LIBCYPHAL_TRANSPORT_CAN_MSG_TX_SESSION_HPP_INCLUDED
