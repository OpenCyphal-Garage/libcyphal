/// @file
/// Context implementation for POSIX systems.
///
/// @copyright
/// Copyright (C) OpenCyphal Development Team  <opencyphal.org>
/// Copyright Amazon.com Inc. or its affiliates.
/// SPDX-License-Identifier: MIT
///

#ifndef LIBCYPHAL_NETWORK_POSIX_CONTEXT_HPP_INCLUDED
#define LIBCYPHAL_NETWORK_POSIX_CONTEXT_HPP_INCLUDED

#include <initializer_list>

#include "libcyphal/network/posix/posix.hpp"
#include "libcyphal/network/context.hpp"
#include "libcyphal/network/posix/sockets.hpp"
#include "libcyphal/janky.hpp"

namespace libcyphal
{
namespace network
{
namespace posix
{

/// Static vector where maxRedundantInterfaces InterfaceIdType's are allocated statically.
template <typename InterfaceIdType>
using RedundantInterfaceContainer =
    libcyphal::janky::UnsynchronizedStaticVector<InterfaceIdType, maxRedundantInterfaces>;

class CANInterfaceEnumerator : public InterfaceEnumerator<std::uint32_t>
{
public:
    CANInterfaceEnumerator(std::initializer_list<std::uint32_t> addresses) noexcept
        : addresses_{addresses}
    {
    }

    virtual ~CANInterfaceEnumerator() = default;

    size_type count() const noexcept override
    {
        return addresses_.size();
    }

    const_iterator begin() const noexcept override
    {
        return addresses_.begin();
    }

    const_iterator end() const noexcept override
    {
        return addresses_.end();
    }

private:
    RedundantInterfaceContainer<std::uint32_t> addresses_;
};

class IPInterfaceEnumerator : public InterfaceEnumerator<network::ip::Address>
{
public:
    IPInterfaceEnumerator(std::initializer_list<network::ip::Address> addresses) noexcept
        : addresses_{addresses}
    {
    }

    virtual ~IPInterfaceEnumerator() = default;

    size_type count() const noexcept override
    {
        return addresses_.size();
    }

    const_iterator begin() const noexcept override
    {
        return addresses_.begin();
    }

    const_iterator end() const noexcept override
    {
        return addresses_.end();
    }

private:
    RedundantInterfaceContainer<network::ip::Address> addresses_;
};

struct NetworkMemoryResources
{
    cetl::pf17::pmr::memory_resource* ip_socket_memory;
    cetl::pf17::pmr::memory_resource* can_socket_memory;
    cetl::pf17::pmr::memory_resource* poll_memory;
};

/// You probably only want to make one of these. That's all libcyphal needs anyway so if you make more then, what are
/// you going to do with the others? You can't eat them and they have no monetary value so...?
class Context : public IContext
{
public:
    Context(NetworkMemoryResources&                     memory_resources,
            std::initializer_list<std::uint32_t>        can_interfaces,
            std::initializer_list<network::ip::Address> ip_interfaces) noexcept;

    virtual ~Context() = default;

    Context()                          = delete;
    Context(const Context&)            = delete;
    Context(Context&&)                 = delete;
    Context& operator=(const Context&) = delete;
    Context& operator=(Context&&)      = delete;

    // +-----------------------------------------------------------------------+
    // | IContext
    // +-----------------------------------------------------------------------+
    janky::expected<PollPointer> makeReadPoll() override;

    janky::expected<SocketPointer<network::ip::Socket>> makeBoundUdpMulticastOutputSocket(
        network::ip::Address interface) override;

    janky::expected<SocketPointer<network::ip::Socket>> makeBoundUdpMulticastInputSocket(
        network::ip::Address interface,
        network::ip::Address multicast_address,
        std::uint16_t        multicast_port) override;

    janky::expected<SocketPointer<std::uint32_t>> makeCANSocket(std::uint32_t) override;

    InterfaceEnumerator<network::ip::Address>& enumerateIPInterfaces() noexcept override;
    InterfaceEnumerator<std::uint32_t>&        enumerateCANInterfaces() noexcept override;

private:
    cetl::pf17::pmr::polymorphic_allocator<network::posix::ip::UDPSocket> ip_socket_allocator_;
    IPInterfaceEnumerator                                                 ip_interface_enumerator_;

    cetl::pf17::pmr::polymorphic_allocator<std::uint32_t> can_socket_allocator_;
    CANInterfaceEnumerator                                can_interface_enumerator_;

    cetl::pf17::pmr::memory_resource* poll_memory_;
};

}  // namespace posix
}  // namespace network
}  // namespace libcyphal

#endif  // LIBCYPHAL_NETWORK_POSIX_CONTEXT_HPP_INCLUDED
