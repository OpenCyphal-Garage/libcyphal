/// @copyright
/// Copyright (C) OpenCyphal Development Team  <opencyphal.org>
/// Copyright Amazon.com Inc. or its affiliates.
/// SPDX-License-Identifier: MIT

#ifndef LIBCYPHAL_TRANSPORT_SVC_SESSION_HPP_INCLUDED
#define LIBCYPHAL_TRANSPORT_SVC_SESSION_HPP_INCLUDED

#include "session.hpp"
#include "types.hpp"

#include <cetl/pf17/cetlpf.hpp>

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

class ISvcRxSession : public IRxSession
{
public:
    /// @brief Receives a service transfer (request or response) from the transport layer.
    ///
    /// Method is not blocking, and will return immediately if no transfer is available.
    ///
    /// @return A service transfer if available; otherwise an empty optional.
    ///
    virtual cetl::optional<ServiceRxTransfer> receive() = 0;

protected:
    ~ISvcRxSession() = default;
};

class IRequestRxSession : public ISvcRxSession
{
public:
    virtual RequestRxParams getParams() const noexcept = 0;

protected:
    ~IRequestRxSession() = default;
};

class IRequestTxSession : public ITxSession
{
public:
    virtual RequestTxParams getParams() const noexcept = 0;

    /// @brief Sends a service request to the transport layer.
    ///
    /// @param metadata Additional metadata associated with the request.
    /// @param payload_fragments Segments of the request payload.
    /// @return `nullopt` in case of success; otherwise a transport error.
    ///
    virtual cetl::optional<AnyError> send(const TransferMetadata& metadata,
                                          const PayloadFragments  payload_fragments) = 0;

protected:
    ~IRequestTxSession() = default;
};

class IResponseRxSession : public ISvcRxSession
{
public:
    virtual ResponseRxParams getParams() const noexcept = 0;

protected:
    ~IResponseRxSession() = default;
};

class IResponseTxSession : public ITxSession
{
public:
    virtual ResponseTxParams getParams() const noexcept = 0;

    /// @brief Sends a service response to the transport layer.
    ///
    /// @param metadata Additional metadata associated with the response.
    /// @param payload_fragments Segments of the response payload.
    /// @return `nullopt` in case of success; otherwise a transport error.
    ///
    virtual cetl::optional<AnyError> send(const ServiceTransferMetadata& metadata,
                                          const PayloadFragments         payload_fragments) = 0;

protected:
    ~IResponseTxSession() = default;
};

}  // namespace transport
}  // namespace libcyphal

#endif  // LIBCYPHAL_TRANSPORT_SVC_SESSION_HPP_INCLUDED
