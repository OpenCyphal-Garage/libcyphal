/// @copyright
/// Copyright (C) OpenCyphal Development Team  <opencyphal.org>
/// Copyright Amazon.com Inc. or its affiliates.
/// SPDX-License-Identifier: MIT

#ifndef LIBCYPHAL_TRANSPORT_MSG_SESSIONS_HPP_INCLUDED
#define LIBCYPHAL_TRANSPORT_MSG_SESSIONS_HPP_INCLUDED

#include "errors.hpp"
#include "session.hpp"
#include "types.hpp"

#include "libcyphal/types.hpp"

#include <cetl/pf17/cetlpf.hpp>
#include <cetl/pmr/function.hpp>

#include <cstddef>

namespace libcyphal
{
namespace transport
{

struct MessageRxParams final
{
    std::size_t extent_bytes{};
    PortId      subject_id{};
};

struct MessageTxParams final
{
    PortId subject_id{};
};

/// @brief Defines an abstract interface of a transport layer receive session for message subscription.
///
/// Use transport's `makeMessageRxSession` factory function to create an instance of this interface.
///
/// @see IRxSession, ISession
///
class IMessageRxSession : public IRxSession
{
public:
    IMessageRxSession(const IMessageRxSession&)                = delete;
    IMessageRxSession(IMessageRxSession&&) noexcept            = delete;
    IMessageRxSession& operator=(const IMessageRxSession&)     = delete;
    IMessageRxSession& operator=(IMessageRxSession&&) noexcept = delete;

    /// @brief Returns the parameters of the message reception session.
    ///
    virtual MessageRxParams getParams() const noexcept = 0;

    /// @brief Receives a message from the transport layer.
    ///
    /// Method is not blocking, and will return immediately if no message is available.
    ///
    /// @return A message transfer if available; otherwise an empty optional.
    ///
    virtual cetl::optional<MessageRxTransfer> receive() = 0;

    /// @brief Umbrella type for data reception callback entities.
    ///
    struct OnReceiveCallback
    {
        /// @brief Defines standard arguments for data reception callback.
        ///
        struct Arg
        {
            /// Holds the received message transfer.
            ///
            /// It's made mutable to allow for the callback function to modify the transfer,
            /// f.e. to move its `ScatteredBuffer` payload to a different location.
            MessageRxTransfer& transfer;
        };

        /// @brief Defines signature of the data reception callback function.
        ///
        /// Size of the function is arbitrary (4 pointers), but should be enough for simple lambdas.
        ///
        using Function = cetl::pmr::function<void(const Arg&), sizeof(void*) * 4>;

    };  // OnReceiveCallback

    /// @brief Sets the data reception callback.
    ///
    /// @param function The callback function, which will be called on data reception.
    ///
    virtual void setOnReceiveCallback(OnReceiveCallback::Function&& function) = 0;

protected:
    IMessageRxSession()  = default;
    ~IMessageRxSession() = default;

};  // IMessageRxSession

/// @brief Defines an abstract interface of a transport layer transmit session for message publishing.
///
/// Use transport's `makeMessageTxSession` factory function to create an instance of this interface.
///
/// @see ITxSession, ISession
///
class IMessageTxSession : public ITxSession
{
public:
    IMessageTxSession(const IMessageTxSession&)                = delete;
    IMessageTxSession(IMessageTxSession&&) noexcept            = delete;
    IMessageTxSession& operator=(const IMessageTxSession&)     = delete;
    IMessageTxSession& operator=(IMessageTxSession&&) noexcept = delete;

    /// @brief Returns the parameters of the message transmission session.
    ///
    virtual MessageTxParams getParams() const noexcept = 0;

    /// @brief Sends a message to the transport layer.
    ///
    /// @param metadata Additional metadata associated with the message.
    /// @param payload_fragments Segments of the message payload.
    /// @return `nullopt` in case of success; otherwise a transport failure.
    ///
    virtual cetl::optional<AnyFailure> send(const TransferTxMetadata& metadata,
                                            const PayloadFragments    payload_fragments) = 0;

protected:
    IMessageTxSession()  = default;
    ~IMessageTxSession() = default;

};  // IMessageTxSession

}  // namespace transport
}  // namespace libcyphal

#endif  // LIBCYPHAL_TRANSPORT_MSG_SESSIONS_HPP_INCLUDED
