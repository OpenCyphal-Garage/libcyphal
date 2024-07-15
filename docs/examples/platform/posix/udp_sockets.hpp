/// @copyright
/// Copyright (C) OpenCyphal Development Team  <opencyphal.org>
/// Copyright Amazon.com Inc. or its affiliates.
/// SPDX-License-Identifier: MIT
///

#ifndef EXAMPLE_PLATFORM_POSIX_UDP_SOCKETS_HPP
#define EXAMPLE_PLATFORM_POSIX_UDP_SOCKETS_HPP

#include "udp.h"

#include <cetl/pf17/cetlpf.hpp>
#include <libcyphal/executor.hpp>
#include <libcyphal/transport/errors.hpp>
#include <libcyphal/transport/types.hpp>
#include <libcyphal/transport/udp/media.hpp>
#include <libcyphal/transport/udp/tx_rx_sockets.hpp>
#include <libcyphal/types.hpp>

#include <cstdint>
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
        const std::string&          address)
    {
        UDPTxHandle handle{-1};
        const auto  result = ::udpTxInit(&handle, ::udpParseIfaceAddress(address.c_str()));
        if (result < 0)
        {
            // TODO: make platform error
            return libcyphal::transport::MemoryError{};
        }

        auto tx_socket = libcyphal::makeUniquePtr<libcyphal::transport::udp::ITxSocket, UdpTxSocket>(memory, handle);
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
        const std::int16_t result = ::udpTxSend(&handle_,
                                                multicast_endpoint.ip_address,
                                                multicast_endpoint.udp_port,
                                                dscp,
                                                payload_fragments.size(),
                                                payload_fragments.data());
        if (result < 0)
        {
            // TODO: make platform error
            return SendResult::Failure{libcyphal::transport::ArgumentError{}};
        }

        return SendResult::Success{result == 1};
    }

    CETL_NODISCARD libcyphal::IExecutor::Callback::Handle registerCallback(
        libcyphal::IExecutor&                    executor,
        libcyphal::IExecutor::Callback::Function function) override
    {
        (void) executor;
        (void) function;

        return libcyphal::IExecutor::Callback::Handle{};
    }

    // MARK: Data members:

    UDPTxHandle handle_{-1};

};  // UdpTxSocket

// MARK: -

class UdpRxSocket final : public libcyphal::transport::udp::IRxSocket
{
public:
    CETL_NODISCARD static libcyphal::transport::udp::IMedia::MakeRxSocketResult::Type make(
        cetl::pmr::memory_resource&                  memory,
        const std::string&                           address,
        const libcyphal::transport::udp::IpEndpoint& endpoint)
    {
        UDPRxHandle handle{-1};
        const auto  result =
            ::udpRxInit(&handle, ::udpParseIfaceAddress(address.c_str()), endpoint.ip_address, endpoint.udp_port);
        if (result < 0)
        {
            // TODO: make platform error
            return libcyphal::transport::MemoryError{};
        }

        auto rx_socket = libcyphal::makeUniquePtr<libcyphal::transport::udp::IRxSocket, UdpRxSocket>(memory, handle);
        if (rx_socket == nullptr)
        {
            ::udpRxClose(&handle);
            return libcyphal::transport::MemoryError{};
        }

        return rx_socket;
    }

    explicit UdpRxSocket(UDPRxHandle handle)
        : handle_{handle}
    {
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
    // MARK: IRxSocket

    CETL_NODISCARD ReceiveResult::Type receive() override
    {
        return ReceiveResult::Failure{libcyphal::transport::MemoryError{}};
    }

    // MARK: Data members:

    UDPRxHandle handle_{-1};

};  // UdpRxSocket

}  // namespace posix
}  // namespace platform
}  // namespace example

#endif  // EXAMPLE_PLATFORM_POSIX_UDP_SOCKETS_HPP
