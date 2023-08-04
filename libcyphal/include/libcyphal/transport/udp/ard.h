/// @file
/// Inevitable header file for translated to/from udpard.h and libcyphal.
///
/// @copyright
/// Copyright (C) OpenCyphal Development Team  <opencyphal.org>
/// Copyright Amazon.com Inc. or its affiliates.
/// SPDX-License-Identifier: MIT
///

#ifndef LIBCYPHAL_TRANSPORT_UDP_ARD_H_INCLUDED
#define LIBCYPHAL_TRANSPORT_UDP_ARD_H_INCLUDED

#include "udpard.h"

#include "libcyphal/libcyphal.hpp"
#include "libcyphal/janky.hpp"

#include "libcyphal/network/context.hpp"
#include "libcyphal/network/ip/address.hpp"
#include "libcyphal/transport/transfer.hpp"
#include "libcyphal/transport/session.hpp"

namespace libcyphal
{
namespace transport
{
namespace udp
{
static_assert(UDPARD_CYPHAL_HEADER_VERSION == 1,
              "We expected udpard.h to be UDPARD_CYPHAL_HEADER_VERSION==1. Please update this header to handle other "
              "versions and then change this static assert.");

constexpr NodeID AnonymousNodeID = UDPARD_NODE_ID_UNSET;

constexpr ResultCode fromUdpardResult(std::int32_t result)
{
    if (result >= 0)
    {
        return ResultCode::Success;
    }
    else
    {
        switch (result)
        {
        case -UDPARD_ERROR_INVALID_ARGUMENT:
            return ResultCode::InvalidArgumentError;
        case -UDPARD_ERROR_OUT_OF_MEMORY:
            return ResultCode::MemoryError;
        case -UDPARD_ERROR_OUT_OF_ORDER:
            return ResultCode::OutOfOrderError;
        case -UDPARD_SUCCESS:
            return ResultCode::Success;
        default:
            return ResultCode::UnknownError;
        }
    }
}

constexpr TransferPriority fromUdpardPriority(UdpardPriority priority)
{
    switch (priority)
    {
    case UdpardPriorityExceptional:
        return TransferPriority::PriorityExceptional;
    case UdpardPriorityImmediate:
        return TransferPriority::PriorityImmediate;
    case UdpardPriorityFast:
        return TransferPriority::PriorityFast;
    case UdpardPriorityHigh:
        return TransferPriority::PriorityHigh;
    case UdpardPriorityNominal:
        return TransferPriority::PriorityNominal;
    case UdpardPriorityLow:
        return TransferPriority::PriorityLow;
    case UdpardPrioritySlow:
        return TransferPriority::PrioritySlow;
    case UdpardPriorityOptional:
        return TransferPriority::PriorityOptional;
    default:
        return TransferPriority::PriorityNominal;
    }
}

constexpr UdpardPriority toUdpardPriority(TransferPriority priority)
{
    switch (priority)
    {
    case TransferPriority::PriorityExceptional:
        return UdpardPriorityExceptional;
    case TransferPriority::PriorityImmediate:
        return UdpardPriorityImmediate;
    case TransferPriority::PriorityFast:
        return UdpardPriorityFast;
    case TransferPriority::PriorityHigh:
        return UdpardPriorityHigh;
    case TransferPriority::PriorityNominal:
        return UdpardPriorityNominal;
    case TransferPriority::PriorityLow:
        return UdpardPriorityLow;
    case TransferPriority::PrioritySlow:
        return UdpardPrioritySlow;
    case TransferPriority::PriorityOptional:
        return UdpardPriorityOptional;
    default:
        return UdpardPriorityNominal;
    }
}

constexpr UdpardTransferKind toUdpTransferKind(const OutputSessionSpecifier& specifier)
{
    switch (specifier.getDataSpecifier().getRole())
    {
    case DataSpecifier::Role::Message:
        return UdpardTransferKindMessage;
    case DataSpecifier::Role::ServiceConsumer:
        return UdpardTransferKindRequest;
    case DataSpecifier::Role::ServiceProvider:
        return UdpardTransferKindResponse;
    default:
        return UdpardTransferKindMessage;
    }
}

constexpr UdpardTransferKind toUdpTransferKind(const InputSessionSpecifier& specifier)
{
    switch (specifier.getDataSpecifier().getRole())
    {
    case DataSpecifier::Role::Message:
        return UdpardTransferKindMessage;
    case DataSpecifier::Role::ServiceConsumer:
        return UdpardTransferKindResponse;
    case DataSpecifier::Role::ServiceProvider:
        return UdpardTransferKindRequest;
    default:
        return UdpardTransferKindMessage;
    }
}

UdpardMicrosecond toUdpardMicrosecond(std::chrono::microseconds us)
{
    return static_cast<UdpardMicrosecond>(us.count());
}

extern "C"
{
    inline void* udpardMemoryAllocateUsingMemoryResource(UdpardInstance* ins, size_t amount)
    {
        return reinterpret_cast<cetl::pf17::pmr::memory_resource*>(ins->user_reference)->allocate(amount);
    }

    inline void udpardMemoryFreeUsingMemoryResource(UdpardInstance* ins, void* p)
    {
        // TODO: update 'ards to provide the amount to free.
        reinterpret_cast<cetl::pf17::pmr::memory_resource*>(ins->user_reference)->deallocate(p, 0);
    }
}

/// DRYs out logic for going to/from C/C++ types when using UdpardInstance.
class UdpardContext
{
public:
    UdpardContext(NodeID nodeId, cetl::pf17::pmr::memory_resource* memory) noexcept
        : instance{udpardInit(udpardMemoryAllocateUsingMemoryResource, udpardMemoryFreeUsingMemoryResource)}
    {
        CETL_DEBUG_ASSERT(memory != nullptr, "memory must not be null");
        instance.user_reference = memory;
        instance.node_id        = nodeId;
    }

    cetl::pf17::pmr::memory_resource& getMemoryResource() const noexcept
    {
        CETL_DEBUG_ASSERT(instance.user_reference != nullptr, "UdpardInstance::user_reference has become null!");
        return *reinterpret_cast<cetl::pf17::pmr::memory_resource*>(instance.user_reference);
    }

    UdpardInstance instance;

protected:
    UdpardContext() = default;
};

/// DRYs out logic for going to/from C/C++ types when using UdpardTxQueue.
class UdpardTxContext final : public UdpardContext
{
public:
    explicit UdpardTxContext(network::ip::Address              interface_address,
                             std::uint32_t                     mtu_bytes,
                             NodeID                            nodeId,
                             cetl::pf17::pmr::memory_resource* memory) noexcept
        : UdpardContext(nodeId, memory)
        , interface_address{interface_address}
        , queue{
              udpardTxInit(cetl::pf17::pmr::deviant::memory_resource_traits<cetl::pf17::pmr::memory_resource>::max_size(
                               *memory),
                           mtu_bytes)}
    {
    }

    explicit UdpardTxContext(network::ip::Address interface_address,
                             std::uint32_t        mtu_bytes,
                             const UdpardContext& context) noexcept
        : UdpardTxContext(interface_address, mtu_bytes, context.instance.node_id, &context.getMemoryResource())
    {
    }

    network::ip::Address interface_address;
    UdpardTxQueue        queue;
};

}  // namespace udp
}  // namespace transport
}  // namespace libcyphal

#endif  // LIBCYPHAL_TRANSPORT_UDP_ARD_H_INCLUDED
