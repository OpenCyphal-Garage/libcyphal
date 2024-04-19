/// @copyright
/// Copyright (C) OpenCyphal Development Team  <opencyphal.org>
/// Copyright Amazon.com Inc. or its affiliates.
/// SPDX-License-Identifier: MIT

#ifndef LIBCYPHAL_TRANSPORT_CAN_MSG_TX_SESSION_HPP_INCLUDED
#define LIBCYPHAL_TRANSPORT_CAN_MSG_TX_SESSION_HPP_INCLUDED

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
class MessageTxSession final : public IMessageTxSession
{
    // In use to disable public construction.
    // See https://seanmiddleditch.github.io/enabling-make-unique-with-private-constructors/
    struct Tag
    {
        explicit Tag()  = default;
        using Interface = IMessageTxSession;
        using Concrete  = MessageTxSession;
    };

public:
    CETL_NODISCARD static Expected<UniquePtr<IMessageTxSession>, AnyError> make(TransportDelegate&     delegate,
                                                                                const MessageTxParams& params)
    {
        auto session = libcyphal::detail::makeUniquePtr<Tag>(delegate.memory(), Tag{}, delegate, params);
        if (session == nullptr)
        {
            return MemoryError{};
        }

        return session;
    }

    MessageTxSession(Tag, TransportDelegate& delegate, const MessageTxParams& params)
        : delegate_{delegate}
        , params_{params}
    {
    }

private:
    // MARK: IMessageTxSession

    CETL_NODISCARD MessageTxParams getParams() const noexcept override
    {
        return params_;
    }

    CETL_NODISCARD cetl::optional<AnyError> send(const TransferMetadata& metadata,
                                                 const PayloadFragments  payload_fragments) override
    {
        (void) delegate_;
        (void) metadata;
        (void) payload_fragments;

        return NotImplementedError{};
    }

    // MARK: IRunnable

    void run(const TimePoint) override {}

    // MARK: Data members:

    TransportDelegate&    delegate_;
    const MessageTxParams params_;

};  // MessageTxSession

}  // namespace detail
}  // namespace can
}  // namespace transport
}  // namespace libcyphal

#endif  // LIBCYPHAL_TRANSPORT_CAN_MSG_TX_SESSION_HPP_INCLUDED
