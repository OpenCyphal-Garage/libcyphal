/// @file
/// Defines the Service Session interfaces for Transport Layer implementations.
///
/// @copyright
/// Copyright (C) OpenCyphal Development Team  <opencyphal.org>
/// Copyright Amazon.com Inc. or its affiliates.
/// SPDX-License-Identifier: MIT

#ifndef LIBCYPHAL_TRANSPORT_SESSION_SVC_SESSION_HPP_INCLUDED
#define LIBCYPHAL_TRANSPORT_SESSION_SVC_SESSION_HPP_INCLUDED

#include "session.hpp"

namespace libcyphal
{
namespace transport
{
inline namespace session
{

/// @brief Declares service request RX session parameters.
///
struct RequestRxParams final
{
    PortId      service_id;
    std::size_t extent_bytes;

};  // RequestRxParams

/// @brief Declares service request TX session parameters.
///
struct RequestTxParams final
{
    PortId service_id;
    NodeId server_node_id;

};  // RequestTxParams

/// @brief Declares service response RX session parameters.
///
struct ResponseRxParams final
{
    PortId      service_id;
    NodeId      server_node_id;
    std::size_t extent_bytes;

};  // ResponseRxParams

/// @brief Declares service response TX session parameters.
///
struct ResponseTxParams final
{
    PortId service_id;

};  // ResponseTxParams

/// @brief Declares an abstract Cyphal transport request RX session interface.
///
class IRequestRxSession : public ISession
{
public:
    CETL_NODISCARD virtual RequestRxParams getParams() const noexcept             = 0;
    virtual void                           setTransferIdTimeout(Duration timeout) = 0;

};  // IRequestRxSession

/// @brief Declares an abstract Cyphal transport request TX session interface.
///
class IRequestTxSession : public ISession
{
public:
    CETL_NODISCARD virtual RequestTxParams getParams() const noexcept = 0;

};  // IRequestTxSession

/// @brief Declares an abstract Cyphal transport response RX session interface.
///
class IResponseRxSession : public ISession
{
public:
    CETL_NODISCARD virtual ResponseRxParams getParams() const noexcept             = 0;
    virtual void                            setTransferIdTimeout(Duration timeout) = 0;

};  // IResponseRxSession

/// @brief Declares an abstract Cyphal transport response TX session interface.
///
class IResponseTxSession : public ISession
{
public:
    CETL_NODISCARD virtual ResponseTxParams getParams() const noexcept = 0;

};  // IResponseTxSession

}  // namespace session
}  // namespace transport
}  // namespace libcyphal

#endif  // LIBCYPHAL_TRANSPORT_SESSION_SVC_SESSION_HPP_INCLUDED
