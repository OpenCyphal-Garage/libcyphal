/// @copyright
/// Copyright (C) OpenCyphal Development Team  <opencyphal.org>
/// Copyright Amazon.com Inc. or its affiliates.
/// SPDX-License-Identifier: MIT
///

#ifndef EXAMPLE_PLATFORM_POSIX_UDP_SOCKETS_HPP_INCLUDED
#define EXAMPLE_PLATFORM_POSIX_UDP_SOCKETS_HPP_INCLUDED

#include "../posix_executor_extension.hpp"
#include "../posix_platform_error.hpp"
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

class UdpTxSocket final : public libcyphal::transport::udp::ITxSocket
{
public:
    CETL_NODISCARD static libcyphal::transport::udp::IMedia::MakeTxSocketResult::Type make(
        cetl::pmr::memory_resource& memory,
        libcyphal::IExecutor&       executor,
        const std::string&          iface_address)
    {
        UDPTxHandle handle{-1};
        const auto  result = ::udpTxInit(&handle, ::udpParseIfaceAddress(iface_address.c_str()));
        if (result < 0)
        {
            return libcyphal::transport::PlatformError{PosixPlatformError{-result}};
        }

        auto tx_socket = libcyphal::makeUniquePtr<ITxSocket, UdpTxSocket>(memory, executor, handle);
        if (tx_socket == nullptr)
        {
            ::udpTxClose(&handle);
            return libcyphal::transport::MemoryError{};
        }

        return tx_socket;
    }

    UdpTxSocket(libcyphal::IExecutor& executor, UDPTxHandle udp_handle)
        : udp_handle_{udp_handle}
        , executor_{executor}
    {
        CETL_DEBUG_ASSERT(udp_handle_.fd >= 0, "");
    }

    ~UdpTxSocket()
    {
        ::udpTxClose(&udp_handle_);
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
        CETL_DEBUG_ASSERT(udp_handle_.fd >= 0, "");
        CETL_DEBUG_ASSERT(payload_fragments.size() == 1, "");

        const std::int16_t result = ::udpTxSend(&udp_handle_,
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

    CETL_NODISCARD libcyphal::IExecutor::Callback::Any registerCallback(
        libcyphal::IExecutor::Callback::Function&& function) override
    {
        auto* const posix_executor_ext = cetl::rtti_cast<IPosixExecutorExtension*>(&executor_);
        if (nullptr == posix_executor_ext)
        {
            return {};
        }

        CETL_DEBUG_ASSERT(udp_handle_.fd >= 0, "");
        return posix_executor_ext->registerAwaitableCallback(std::move(function),
                                                             IPosixExecutorExtension::Trigger::Writable{
                                                                 udp_handle_.fd});
    }

    // MARK: Data members:

    UDPTxHandle           udp_handle_;
    libcyphal::IExecutor& executor_;

};  // UdpTxSocket

// MARK: -

class UdpRxSocket final : public libcyphal::transport::udp::IRxSocket
{
public:
    CETL_NODISCARD static libcyphal::transport::udp::IMedia::MakeRxSocketResult::Type make(
        cetl::pmr::memory_resource&                  memory,
        libcyphal::IExecutor&                        executor,
        const std::string&                           address,
        const libcyphal::transport::udp::IpEndpoint& endpoint)
    {
        UDPRxHandle handle{-1};
        const auto  result =
            ::udpRxInit(&handle, ::udpParseIfaceAddress(address.c_str()), endpoint.ip_address, endpoint.udp_port);
        if (result < 0)
        {
            return libcyphal::transport::PlatformError{PosixPlatformError{-result}};
        }

        auto rx_socket = libcyphal::makeUniquePtr<IRxSocket, UdpRxSocket>(memory, executor, handle, memory);
        if (rx_socket == nullptr)
        {
            ::udpRxClose(&handle);
            return libcyphal::transport::MemoryError{};
        }

        return rx_socket;
    }

    UdpRxSocket(libcyphal::IExecutor& executor, UDPRxHandle udp_handle, cetl::pmr::memory_resource& memory)
        : udp_handle_{udp_handle}
        , executor_{executor}
        , memory_{memory}
    {
        CETL_DEBUG_ASSERT(udp_handle_.fd >= 0, "");
    }

    ~UdpRxSocket()
    {
        ::udpRxClose(&udp_handle_);
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
        CETL_DEBUG_ASSERT(udp_handle_.fd >= 0, "");

        // Current Udpard api limitation is not allowing to pass bigger buffer than actual data size is.
        // Hence, we need temp buffer on stack, and then memory copying.
        // TODO: Eliminate tmp buffer and memmove when https://github.com/OpenCyphal/libudpard/issues/58 is resolved.
        //
        std::array<cetl::byte, BufferSize> buffer{};
        std::size_t                        inout_size = buffer.size();
        const std::int16_t                 result     = ::udpRxReceive(&udp_handle_, &inout_size, buffer.data());
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

        return ReceiveResult::Metadata{executor_.now(),
                                       {static_cast<cetl::byte*>(allocated_buffer),
                                        libcyphal::PmrRawBytesDeleter{inout_size, &memory_}}};
    }

    CETL_NODISCARD libcyphal::IExecutor::Callback::Any registerCallback(
        libcyphal::IExecutor::Callback::Function&& function) override
    {
        auto* const posix_executor_ext = cetl::rtti_cast<IPosixExecutorExtension*>(&executor_);
        if (nullptr == posix_executor_ext)
        {
            return {};
        }

        CETL_DEBUG_ASSERT(udp_handle_.fd >= 0, "");
        return posix_executor_ext->registerAwaitableCallback(std::move(function),
                                                             IPosixExecutorExtension::Trigger::Readable{
                                                                 udp_handle_.fd});
    }

    // MARK: Data members:

    UDPRxHandle                 udp_handle_;
    libcyphal::IExecutor&       executor_;
    cetl::pmr::memory_resource& memory_;

};  // UdpRxSocket

}  // namespace posix
}  // namespace platform
}  // namespace example

#endif  // EXAMPLE_PLATFORM_POSIX_UDP_SOCKETS_HPP_INCLUDED
