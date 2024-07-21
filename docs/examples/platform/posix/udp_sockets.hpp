/// @copyright
/// Copyright (C) OpenCyphal Development Team  <opencyphal.org>
/// Copyright Amazon.com Inc. or its affiliates.
/// SPDX-License-Identifier: MIT
///

#ifndef EXAMPLE_PLATFORM_POSIX_UDP_SOCKETS_HPP_INCLUDED
#define EXAMPLE_PLATFORM_POSIX_UDP_SOCKETS_HPP_INCLUDED

#include "posix_executor_extension.hpp"
#include "posix_platform_error.hpp"
#include "udp.h"

#include <cetl/cetl.hpp>
#include <cetl/pf17/cetlpf.hpp>
#include <cetl/rtti.hpp>
#include <libcyphal/executor.hpp>
#include <libcyphal/transport/errors.hpp>
#include <libcyphal/transport/types.hpp>
#include <libcyphal/transport/udp/media.hpp>
#include <libcyphal/transport/udp/tx_rx_sockets.hpp>
#include <libcyphal/types.hpp>

#include <array>
#include <cstddef>
#include <cstdint>
#include <cstring>
#include <string>
#include <utility>

namespace example
{
namespace platform
{
namespace posix
{

class UdpSocketBase
{
protected:
    CETL_NODISCARD static libcyphal::IExecutor::Callback::Handle registerCallbackWithCondition(
        libcyphal::IExecutor&                                  executor,
        libcyphal::IExecutor::Callback::Function&&             function,
        const IPosixExecutorExtension::WhenCondition::Variant& condition)
    {
        auto* const posix_extension = cetl::rtti_cast<IPosixExecutorExtension*>(&executor);
        if (nullptr == posix_extension)
        {
            return {};
        }

        auto callback_handle = executor.registerCallback(std::move(function));
        posix_extension->scheduleCallbackWhen(callback_handle.id(), condition);

        return callback_handle;
    }

};  // UdpSocketBase

// MARK: -

class UdpTxSocket final : public UdpSocketBase, public libcyphal::transport::udp::ITxSocket
{
public:
    CETL_NODISCARD static libcyphal::transport::udp::IMedia::MakeTxSocketResult::Type make(
        cetl::pmr::memory_resource& memory,
        const std::string&          iface_address)
    {
        using ITxSocket = libcyphal::transport::udp::ITxSocket;

        UDPTxHandle handle{-1};
        const auto  result = ::udpTxInit(&handle, ::udpParseIfaceAddress(iface_address.c_str()));
        if (result < 0)
        {
            return libcyphal::transport::PlatformError{PosixPlatformError{-result}};
        }

        auto tx_socket = libcyphal::makeUniquePtr<ITxSocket, UdpTxSocket>(memory, handle);
        if (tx_socket == nullptr)
        {
            ::udpTxClose(&handle);
            return libcyphal::transport::MemoryError{};
        }

        return tx_socket;
    }

    explicit UdpTxSocket(UDPTxHandle handle)
        : handle_{handle}
    {
        CETL_DEBUG_ASSERT(handle_.fd >= 0, "");
    }

    ~UdpTxSocket()
    {
        ::udpTxClose(&handle_);
    }

    UdpTxSocket(const UdpTxSocket&)                = delete;
    UdpTxSocket(UdpTxSocket&&) noexcept            = delete;
    UdpTxSocket& operator=(const UdpTxSocket&)     = delete;
    UdpTxSocket& operator=(UdpTxSocket&&) noexcept = delete;

private:
    // MARK: ITxSocket

    SendResult::Type send(const libcyphal::TimePoint,
                          const libcyphal::transport::udp::IpEndpoint  multicast_endpoint,
                          const std::uint8_t                           dscp,
                          const libcyphal::transport::PayloadFragments payload_fragments) override
    {
        CETL_DEBUG_ASSERT(handle_.fd >= 0, "");
        CETL_DEBUG_ASSERT(payload_fragments.size() == 1, "");

        const std::int16_t result = ::udpTxSend(&handle_,
                                                multicast_endpoint.ip_address,
                                                multicast_endpoint.udp_port,
                                                dscp,
                                                payload_fragments[0].size(),
                                                payload_fragments[0].data());
        if (result < 0)
        {
            return libcyphal::transport::PlatformError{PosixPlatformError{-result}};
        }

        return SendResult::Success{result == 1};
    }

    CETL_NODISCARD libcyphal::IExecutor::Callback::Handle registerCallback(
        libcyphal::IExecutor&                      executor,
        libcyphal::IExecutor::Callback::Function&& function) override
    {
        using HandleWritable = IPosixExecutorExtension::WhenCondition::HandleWritable;

        CETL_DEBUG_ASSERT(handle_.fd >= 0, "");

        return registerCallbackWithCondition(executor, std::move(function), HandleWritable{handle_.fd});
    }

    // MARK: Data members:

    UDPTxHandle handle_;

};  // UdpTxSocket

// MARK: -

class UdpRxSocket final : public UdpSocketBase, public libcyphal::transport::udp::IRxSocket
{
public:
    CETL_NODISCARD static libcyphal::transport::udp::IMedia::MakeRxSocketResult::Type make(
        cetl::pmr::memory_resource&                  memory,
        libcyphal::IExecutor&                        executor,
        const std::string&                           address,
        const libcyphal::transport::udp::IpEndpoint& endpoint)
    {
        using IRxSocket = libcyphal::transport::udp::IRxSocket;

        UDPRxHandle handle{-1};
        const auto  result =
            ::udpRxInit(&handle, ::udpParseIfaceAddress(address.c_str()), endpoint.ip_address, endpoint.udp_port);
        if (result < 0)
        {
            return libcyphal::transport::PlatformError{PosixPlatformError{-result}};
        }

        auto rx_socket = libcyphal::makeUniquePtr<IRxSocket, UdpRxSocket>(memory, handle, executor, memory);
        if (rx_socket == nullptr)
        {
            ::udpRxClose(&handle);
            return libcyphal::transport::MemoryError{};
        }

        return rx_socket;
    }

    explicit UdpRxSocket(UDPRxHandle handle, libcyphal::IExecutor& executor, cetl::pmr::memory_resource& memory)
        : handle_{handle}
        , time_provider_{executor}
        , memory_{memory}
    {
        CETL_DEBUG_ASSERT(handle_.fd >= 0, "");
    }

    ~UdpRxSocket()
    {
        ::udpRxClose(&handle_);
    }

    UdpRxSocket(const UdpRxSocket&)                = delete;
    UdpRxSocket(UdpRxSocket&&) noexcept            = delete;
    UdpRxSocket& operator=(const UdpRxSocket&)     = delete;
    UdpRxSocket& operator=(UdpRxSocket&&) noexcept = delete;

private:
    static constexpr std::size_t BufferSize = 2000;

    // MARK: IRxSocket

    CETL_NODISCARD ReceiveResult::Type receive() override
    {
        CETL_DEBUG_ASSERT(handle_.fd >= 0, "");

        // Current Udpard api limitation is not allowing to pass bigger buffer than actual data size is.
        // Hence, we need temp buffer on stack, and then memory copying.
        // TODO: Eliminate tmp buffer and memmove when https://github.com/OpenCyphal/libudpard/issues/58 is resolved.
        //
        std::array<cetl::byte, BufferSize> buffer{};
        std::size_t                        inout_size = buffer.size();
        const std::int16_t                 result     = ::udpRxReceive(&handle_, &inout_size, buffer.data());
        if (result < 0)
        {
            return libcyphal::transport::PlatformError{PosixPlatformError{-result}};
        }
        if (result == 0)
        {
            return cetl::nullopt;
        }
        //
        auto* const allocated_buffer = memory_.allocate(inout_size);
        if (nullptr == allocated_buffer)
        {
            return libcyphal::transport::MemoryError{};
        }
        (void) std::memmove(allocated_buffer, buffer.data(), inout_size);

        return ReceiveResult::Metadata{time_provider_.now(),
                                       {static_cast<cetl::byte*>(allocated_buffer),
                                        libcyphal::PmrRawBytesDeleter{inout_size, &memory_}}};
    }

    CETL_NODISCARD libcyphal::IExecutor::Callback::Handle registerCallback(
        libcyphal::IExecutor&                      executor,
        libcyphal::IExecutor::Callback::Function&& function) override
    {
        using HandleReadable = IPosixExecutorExtension::WhenCondition::HandleReadable;

        CETL_DEBUG_ASSERT(handle_.fd >= 0, "");

        return registerCallbackWithCondition(executor, std::move(function), HandleReadable{handle_.fd});
    }

    // MARK: Data members:

    UDPRxHandle                 handle_;
    libcyphal::IExecutor&       time_provider_;
    cetl::pmr::memory_resource& memory_;

};  // UdpRxSocket

}  // namespace posix
}  // namespace platform
}  // namespace example

#endif  // EXAMPLE_PLATFORM_POSIX_UDP_SOCKETS_HPP_INCLUDED
