/// @copyright
/// Copyright (C) OpenCyphal Development Team  <opencyphal.org>
/// Copyright Amazon.com Inc. or its affiliates.
/// SPDX-License-Identifier: MIT

#ifndef LIBCYPHAL_CONFIG_HPP_INCLUDED
#define LIBCYPHAL_CONFIG_HPP_INCLUDED

#include <cstddef>

namespace libcyphal
{
namespace config
{

/// Defines max footprint of a callback function in use by the executor.
/// Size is chosen arbitrary, but it should be enough to store any lambda or function pointer.
///
constexpr std::size_t IExecutor_Callback_FunctionMaxSize = sizeof(void*) * 8;

/// Defines footprint size reserved for a callback implementation.
/// The actual max footprint for the callback implementation is `sizeof(IExecutor::Function)` bigger,
/// and it depends on `Cfg_IExecutor_Callback_FunctionMaxSize`.
///
constexpr std::size_t IExecutor_Callback_ReserveSize = sizeof(void*) * 16;

namespace application
{
namespace node
{

/// Defines max footprint of a callback function in use by the heartbeat producer.
/// Size is chosen arbitrary, but it should be enough to store any lambda or function pointer.
///
constexpr std::size_t HeartbeatProducer_UpdateCallback_FunctionSize = sizeof(void*) * 4;

}  // namespace node
}  // namespace application

namespace presentation
{

/// Defines the size of a payload which is considered as a small one,
/// and therefore could be used with stack buffer. Any payload larger than this size will be PMR allocated.
///
constexpr std::size_t SmallPayloadSize = 256;

/// Defines max footprint of a callback function in use by the RPC client response promise.
/// Size is chosen arbitrary, but it should be enough to store any lambda or function pointer.
///
constexpr std::size_t ResponsePromiseBase_Callback_FunctionSize = sizeof(void*) * 4;

/// Defines max footprint of a callback function in use by the RPC server response continuation.
/// Size is chosen arbitrary, but it should be enough to store any lambda or function pointer.
///
constexpr std::size_t ServerBase_ContinuationImpl_FunctionMaxSize = sizeof(void*) * 5;

/// Defines max footprint of a callback function in use by the RPC server request notification.
/// Size is chosen arbitrary, but it should be enough to store any lambda or function pointer.
///
constexpr std::size_t ServerBase_OnRequestCallback_FunctionMaxSize = sizeof(void*) * 5;

/// Defines max footprint of a callback function in use by the message subscriber receive notification.
/// Size is chosen arbitrary, but it should be enough to store any lambda or function pointer.
///
constexpr std::size_t Subscriber_OnReceiveCallback_FunctionMaxSize = sizeof(void*) * 4;

}  // namespace presentation

namespace transport
{

/// Defines max footprint of a platform-specific error implementation.
///
constexpr std::size_t PlatformErrorMaxSize = sizeof(void*) * 3;

/// Defines max footprint of a callback function in use by the message RX session notification.
/// Size is chosen arbitrary, but it should be enough to store any lambda or function pointer.
///
constexpr std::size_t IMessageRxSession_OnReceiveCallback_FunctionMaxSize = sizeof(void*) * 4;

/// Defines max footprint of a callback function in use by the service RX session notification.
/// Size is chosen arbitrary, but it should be enough to store any lambda or function pointer.
///
constexpr std::size_t ISvcRxSession_OnReceiveCallback_FunctionMaxSize = sizeof(void*) * 4;

/// Defines max footprint of a storage implementation used by the scattered buffer.
///
constexpr std::size_t ScatteredBuffer_StorageVariantFootprint = sizeof(void*) * 8;

namespace can
{

/// Defines max footprint of a callback function in use by the CAN transport transient error handler.
/// Size is chosen arbitrary, but it should be enough to store any lambda or function pointer.
///
constexpr std::size_t ICanTransport_TransientErrorHandlerMaxSize = sizeof(void*) * 3;

}  // namespace can

namespace udp
{

/// Defines max footprint of a callback function in use by the UDP transport transient error handler.
/// Size is chosen arbitrary, but it should be enough to store any lambda or function pointer.
///
constexpr std::size_t IUdpTransport_TransientErrorHandlerMaxSize = sizeof(void*) * 3;

}  // namespace udp
}  // namespace transport

}  // namespace config
}  // namespace libcyphal

#endif  // LIBCYPHAL_CONFIG_HPP_INCLUDED
