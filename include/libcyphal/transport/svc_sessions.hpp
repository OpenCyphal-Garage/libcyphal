/// @copyright
/// Copyright (C) OpenCyphal Development Team  <opencyphal.org>
/// Copyright Amazon.com Inc. or its affiliates.
/// SPDX-License-Identifier: MIT

#ifndef LIBCYPHAL_TRANSPORT_SVC_SESSION_HPP_INCLUDED
#define LIBCYPHAL_TRANSPORT_SVC_SESSION_HPP_INCLUDED

#include "session.hpp"

namespace libcyphal
{
namespace transport
{
namespace session
{

struct RequestRxParams final
{
    std::size_t extent_bytes;
    PortId      service_id;
};

struct RequestTxParams final
{
    PortId service_id;
    NodeId server_node_id;
};

struct ResponseRxParams final
{
    std::size_t extent_bytes;
    PortId      service_id;
    NodeId      server_node_id;
};

struct ResponseTxParams final
{
    PortId service_id;
};

class ISvcRxSession : public IRxSession
{
public:
    /// @brief Receives a service transfer (request or response) from the transport layer.
    ///
    /// Method is not blocking, and will return immediately if no transfer is available.
    ///
    /// @return A service transfer if available; otherwise an empty optional.
    ///
    CETL_NODISCARD virtual cetl::optional<ServiceRxTransfer> receive() = 0;
};

class IRequestRxSession : public ISvcRxSession
{
public:
    CETL_NODISCARD virtual RequestRxParams getParams() const noexcept = 0;
};

class IRequestTxSession : public ISession
{
public:
    CETL_NODISCARD virtual RequestTxParams getParams() const noexcept = 0;

    /// @brief Sends a service request to the transport layer.
    ///
    /// @param metadata Additional metadata associated with the request.
    /// @param payload_fragments Segments of the request payload.
    /// @return `void` in case of success; otherwise an error.
    ///
    CETL_NODISCARD virtual Expected<void, AnyError> send(const TransferMetadata& metadata,
                                                         const PayloadFragments  payload_fragments) = 0;
};

class IResponseRxSession : public ISvcRxSession
{
public:
    CETL_NODISCARD virtual ResponseRxParams getParams() const noexcept = 0;
};

class IResponseTxSession : public ISession
{
public:
    CETL_NODISCARD virtual ResponseTxParams getParams() const noexcept = 0;

    /// @brief Sends a service response to the transport layer.
    ///
    /// @param metadata Additional metadata associated with the response.
    /// @param payload_fragments Segments of the response payload.
    /// @return `void` in case of success; otherwise an error.
    ///
    CETL_NODISCARD virtual Expected<void, AnyError> send(const ServiceTransferMetadata& metadata,
                                                         const PayloadFragments         payload_fragments) = 0;
};

}  // namespace session
}  // namespace transport
}  // namespace libcyphal

#endif  // LIBCYPHAL_TRANSPORT_SVC_SESSION_HPP_INCLUDED
