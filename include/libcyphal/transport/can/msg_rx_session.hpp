/// @copyright
/// Copyright (C) OpenCyphal Development Team  <opencyphal.org>
/// Copyright Amazon.com Inc. or its affiliates.
/// SPDX-License-Identifier: MIT

#ifndef LIBCYPHAL_TRANSPORT_CAN_MSG_RX_SESSION_HPP_INCLUDED
#define LIBCYPHAL_TRANSPORT_CAN_MSG_RX_SESSION_HPP_INCLUDED

#include "delegate.hpp"

#include "libcyphal/transport/errors.hpp"
#include "libcyphal/transport/msg_sessions.hpp"
#include "libcyphal/transport/types.hpp"
#include "libcyphal/types.hpp"

#include <canard.h>
#include <cetl/cetl.hpp>
#include <cetl/pf17/attribute.hpp>
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
/// namely: in destructor we have to unsubscribe, as well as let delegate to know this fact.
///
class MessageRxSession final : public IMessageRxSession, private IRxSessionDelegate  // NOSONAR cpp:S4963
{
    /// @brief Defines specification for making interface unique ptr.
    ///
    struct Spec
    {
        using Interface = IMessageRxSession;
        using Concrete  = MessageRxSession;

        // In use to disable public construction.
        // See https://seanmiddleditch.github.io/enabling-make-unique-with-private-constructors/
        explicit Spec() = default;
    };

public:
    CETL_NODISCARD static Expected<UniquePtr<IMessageRxSession>, AnyError> make(TransportDelegate&     delegate,
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

    MessageRxSession(Spec, TransportDelegate& delegate, const MessageRxParams& params)
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

        subscription_.user_reference = static_cast<IRxSessionDelegate*>(this);

        delegate_.triggerUpdateOfFilters(TransportDelegate::FiltersUpdateCondition::SubjectPortAdded);
    }

    MessageRxSession(const MessageRxSession&)                = delete;
    MessageRxSession(MessageRxSession&&) noexcept            = delete;
    MessageRxSession& operator=(const MessageRxSession&)     = delete;
    MessageRxSession& operator=(MessageRxSession&&) noexcept = delete;

    ~MessageRxSession() override
    {
        const int8_t result =
            ::canardRxUnsubscribe(&delegate_.canard_instance(), CanardTransferKindMessage, params_.subject_id);
        (void) result;
        CETL_DEBUG_ASSERT(result >= 0, "There is no way currently to get an error here.");
        CETL_DEBUG_ASSERT(result > 0, "Subscription supposed to be made at constructor.");

        delegate_.triggerUpdateOfFilters(TransportDelegate::FiltersUpdateCondition::SubjectPortRemoved);
    }

private:
    // MARK: IMessageRxSession

    CETL_NODISCARD MessageRxParams getParams() const noexcept override
    {
        return params_;
    }

    CETL_NODISCARD cetl::optional<MessageRxTransfer> receive() override
    {
        cetl::optional<MessageRxTransfer> result{};
        result.swap(last_rx_transfer_);
        return result;
    }

    // MARK: IRxSession

    void setTransferIdTimeout(const Duration timeout) override
    {
        const auto timeout_us = std::chrono::duration_cast<std::chrono::microseconds>(timeout);
        if (timeout_us.count() > 0)
        {
            subscription_.transfer_id_timeout_usec = static_cast<CanardMicrosecond>(timeout_us.count());
        }
    }

    // MARK: IRunnable

    void run(const TimePoint) override
    {
        // Nothing to do here currently.
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

        const MessageTransferMetadata   meta{transfer_id, timestamp, priority, publisher_node_id};
        TransportDelegate::CanardMemory canard_memory{delegate_,
                                                      static_cast<cetl::byte*>(transfer.payload),
                                                      transfer.payload_size};

        (void) last_rx_transfer_.emplace(MessageRxTransfer{meta, ScatteredBuffer{std::move(canard_memory)}});
    }

    // MARK: Data members:

    TransportDelegate&                delegate_;
    const MessageRxParams             params_;
    CanardRxSubscription              subscription_;
    cetl::optional<MessageRxTransfer> last_rx_transfer_;

};  // MessageRxSession

}  // namespace detail
}  // namespace can
}  // namespace transport
}  // namespace libcyphal

#endif  // LIBCYPHAL_TRANSPORT_CAN_MSG_RX_SESSION_HPP_INCLUDED
