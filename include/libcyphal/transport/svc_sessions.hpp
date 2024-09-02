/// @copyright
/// Copyright (C) OpenCyphal Development Team  <opencyphal.org>
/// Copyright Amazon.com Inc. or its affiliates.
/// SPDX-License-Identifier: MIT

#ifndef LIBCYPHAL_TRANSPORT_SVC_SESSION_HPP_INCLUDED
#define LIBCYPHAL_TRANSPORT_SVC_SESSION_HPP_INCLUDED

#include "errors.hpp"
#include "session.hpp"
#include "types.hpp"

#include <cetl/pf17/cetlpf.hpp>
#include <cetl/pmr/function.hpp>

#include <cstddef>

namespace libcyphal
{
namespace transport
{

struct RequestRxParams final
{
    std::size_t extent_bytes{};
    PortId      service_id{};
};

struct RequestTxParams final
{
    PortId service_id{};
    NodeId server_node_id{};
};

struct ResponseRxParams final
{
    std::size_t extent_bytes{};
    PortId      service_id{};
    NodeId      server_node_id{};
};

struct ResponseTxParams final
{
    PortId service_id{};
};

/// @brief Defines an abstract interface of a transport layer receive session for service.
///
/// @see IRxSession, ISession
///
class ISvcRxSession : public IRxSession
{
public:
    ISvcRxSession(const ISvcRxSession&)                = delete;
    ISvcRxSession(ISvcRxSession&&) noexcept            = delete;
    ISvcRxSession& operator=(const ISvcRxSession&)     = delete;
    ISvcRxSession& operator=(ISvcRxSession&&) noexcept = delete;

    /// @brief Receives a service transfer (request or response) from the transport layer.
    ///
    /// Method is not blocking, and will return immediately if no transfer is available.
    ///
    /// @return A service transfer if available; otherwise an empty optional.
    ///
    virtual cetl::optional<ServiceRxTransfer> receive() = 0;

    // TODO: docs
    struct OnReceiveCallback
    {
        struct Arg
        {
            ServiceRxTransfer& transfer;
        };
        using Function = cetl::pmr::function<void(const Arg&), sizeof(void*) * 4>;
    };
    virtual void setOnReceiveCallback(OnReceiveCallback::Function&& function) = 0;

protected:
    ISvcRxSession()  = default;
    ~ISvcRxSession() = default;
};

/// @brief Defines an abstract interface of a transport layer receive session for service requests.
///
/// Use transport's `makeRequestRxSession` factory function to create an instance of this interface.
///
/// @see ISvcRxSession, IRxSession, ISession
///
class IRequestRxSession : public ISvcRxSession
{
public:
    IRequestRxSession(const IRequestRxSession&)                = delete;
    IRequestRxSession(IRequestRxSession&&) noexcept            = delete;
    IRequestRxSession& operator=(const IRequestRxSession&)     = delete;
    IRequestRxSession& operator=(IRequestRxSession&&) noexcept = delete;

    virtual RequestRxParams getParams() const noexcept = 0;

protected:
    IRequestRxSession()  = default;
    ~IRequestRxSession() = default;
};

/// @brief Defines an abstract interface of a transport layer transmit session for service requests.
///
/// Use transport's `makeRequestTxSession` factory function to create an instance of this interface.
///
/// @see ITxSession, ISession
///
class IRequestTxSession : public ITxSession
{
public:
    IRequestTxSession(const IRequestTxSession&)                = delete;
    IRequestTxSession(IRequestTxSession&&) noexcept            = delete;
    IRequestTxSession& operator=(const IRequestTxSession&)     = delete;
    IRequestTxSession& operator=(IRequestTxSession&&) noexcept = delete;

    virtual RequestTxParams getParams() const noexcept = 0;

    /// @brief Sends a service request to the transport layer.
    ///
    /// @param metadata Additional metadata associated with the request.
    /// @param payload_fragments Segments of the request payload.
    /// @return `nullopt` in case of success; otherwise a transport failure.
    ///
    virtual cetl::optional<AnyFailure> send(const TransferTxMetadata& metadata,
                                            const PayloadFragments    payload_fragments) = 0;

protected:
    IRequestTxSession()  = default;
    ~IRequestTxSession() = default;
};

/// @brief Defines an abstract interface of a transport layer receive session for service responses.
///
/// Use transport's `makeResponseRxSession` factory function to create an instance of this interface.
///
/// @see ISvcRxSession, IRxSession, ISession
///
class IResponseRxSession : public ISvcRxSession
{
public:
    IResponseRxSession(const IResponseRxSession&)                = delete;
    IResponseRxSession(IResponseRxSession&&) noexcept            = delete;
    IResponseRxSession& operator=(const IResponseRxSession&)     = delete;
    IResponseRxSession& operator=(IResponseRxSession&&) noexcept = delete;

    virtual ResponseRxParams getParams() const noexcept = 0;

protected:
    IResponseRxSession()  = default;
    ~IResponseRxSession() = default;
};

/// @brief Defines an abstract interface of a transport layer transmit session for service responses.
///
/// Use transport's `makeResponseTxSession` factory function to create an instance of this interface.
///
/// @see ITxSession, ISession
///
class IResponseTxSession : public ITxSession
{
public:
    IResponseTxSession(const IResponseTxSession&)                = delete;
    IResponseTxSession(IResponseTxSession&&) noexcept            = delete;
    IResponseTxSession& operator=(const IResponseTxSession&)     = delete;
    IResponseTxSession& operator=(IResponseTxSession&&) noexcept = delete;

    virtual ResponseTxParams getParams() const noexcept = 0;

    /// @brief Sends a service response to the transport layer.
    ///
    /// @param metadata Additional metadata associated with the response.
    /// @param payload_fragments Segments of the response payload.
    /// @return `nullopt` in case of success; otherwise a transport failure.
    ///
    virtual cetl::optional<AnyFailure> send(const ServiceTxMetadata& metadata,
                                            const PayloadFragments   payload_fragments) = 0;

protected:
    IResponseTxSession()  = default;
    ~IResponseTxSession() = default;
};

}  // namespace transport
}  // namespace libcyphal

#endif  // LIBCYPHAL_TRANSPORT_SVC_SESSION_HPP_INCLUDED
