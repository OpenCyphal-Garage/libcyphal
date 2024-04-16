/// @copyright
/// Copyright (C) OpenCyphal Development Team  <opencyphal.org>
/// Copyright Amazon.com Inc. or its affiliates.
/// SPDX-License-Identifier: MIT

#ifndef LIBCYPHAL_TRANSPORT_CAN_MSG_RX_SESSION_HPP_INCLUDED
#define LIBCYPHAL_TRANSPORT_CAN_MSG_RX_SESSION_HPP_INCLUDED

#include "delegate.hpp"
#include "libcyphal/transport/msg_sessions.hpp"

#include <canard.h>

namespace libcyphal
{
namespace transport
{
namespace can
{
namespace detail
{
class MessageRxSession final : public IMessageRxSession
{
    // In use to disable public construction.
    // See https://seanmiddleditch.github.io/enabling-make-unique-with-private-constructors/
    struct Tag
    {
        explicit Tag()  = default;
        using Interface = IMessageRxSession;
        using Concrete  = MessageRxSession;
    };

public:
    CETL_NODISCARD static Expected<UniquePtr<IMessageRxSession>, AnyError> make(TransportDelegate&     delegate,
                                                                                const MessageRxParams& params)
    {
        cetl::optional<AnyError> any_error{};
        auto session = libcyphal::detail::makeUniquePtr<Tag>(delegate.memory_, Tag{}, delegate, params, any_error);
        if (any_error.has_value())
        {
            return any_error.value();
        }

        return session;
    }

    MessageRxSession(Tag,
                     TransportDelegate&        delegate,
                     const MessageRxParams&    params,
                     cetl::optional<AnyError>& out_error)
        : delegate_{delegate}
        , params_{params}
    {
        auto result = canardRxSubscribe(&delegate.canard_instance_,
                                        CanardTransferKindMessage,
                                        params_.subject_id,
                                        params_.extent_bytes,
                                        CANARD_DEFAULT_TRANSFER_ID_TIMEOUT_USEC,
                                        &subscription_);
        if (result < 0)
        {
            out_error = TransportDelegate::anyErrorFromCanard(result);
            return;
        }
        CETL_DEBUG_ASSERT(result > 0, "New subscription supposed to be made.");

        is_subscribed_ = true;
    }

    ~MessageRxSession() override
    {
        if (is_subscribed_)
        {
            canardRxUnsubscribe(&delegate_.canard_instance_, CanardTransferKindMessage, params_.subject_id);
        }
    }

private:
    // MARK: IMessageRxSession

    CETL_NODISCARD MessageRxParams getParams() const noexcept override
    {
        return params_;
    }

    CETL_NODISCARD cetl::optional<MessageRxTransfer> receive() override
    {
        return {};
    }

    // MARK: IRxSession

    void setTransferIdTimeout(const Duration timeout) override
    {
        CETL_DEBUG_ASSERT(timeout.count() > 0, "Timeout should be positive.");
        if (timeout.count() > 0)
        {
            subscription_.transfer_id_timeout_usec = static_cast<CanardMicrosecond>(timeout.count());
        }
    }

    // MARK: ISession

    // MARK: IRunnable

    void run(const TimePoint) override {}

private:
    TransportDelegate&    delegate_;
    const MessageRxParams params_;

    bool                 is_subscribed_{false};
    CanardRxSubscription subscription_{};

};  // MessageRxSession

}  // namespace detail
}  // namespace can
}  // namespace transport
}  // namespace libcyphal

#endif  // LIBCYPHAL_TRANSPORT_CAN_MSG_RX_SESSION_HPP_INCLUDED