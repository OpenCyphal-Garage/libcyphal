/// @file
/// Abstract type for an object that can poll a system to see if a collection of networking resources have pending
/// events.
///
/// @copyright
/// Copyright (C) OpenCyphal Development Team  <opencyphal.org>
/// Copyright Amazon.com Inc. or its affiliates.
/// SPDX-License-Identifier: MIT
///

#ifndef LIBCYPHAL_NETWORK_POLL_HPP_INCLUDED
#define LIBCYPHAL_NETWORK_POLL_HPP_INCLUDED

#include <chrono>

#include "libcyphal/libcyphal.hpp"
#include "libcyphal/janky.hpp"

#include "cetl/pf17/memory_resource.hpp"
#include "cetl/variable_length_array.hpp"

namespace libcyphal
{
namespace network
{

class ISocket;  //< forward declaration.

class IPoll
{
public:
    using SocketEventList = cetl::VariableLengthArray<ISocket*, cetl::pf17::pmr::polymorphic_allocator<ISocket*>>;

    virtual Status reserveRegistrarCapacity(std::size_t socketCount) = 0;
    virtual Status registerSocket(ISocket* s)                        = 0;
    virtual Status unregisterSocket(ISocket* s) noexcept             = 0;
    virtual Status clear() noexcept                                  = 0;

    virtual Status poll(SocketEventList&          out_event_list,
                        std::chrono::microseconds wait_for = std::chrono::microseconds{0}) = 0;

protected:
    /// Only DarkPointers can delete this object through the interface.
    friend class cetl::pmr::PolymorphicDeleter<cetl::pf17::pmr::polymorphic_allocator<IPoll>>;
    virtual ~IPoll() = default;
};

}  // namespace network
}  // namespace libcyphal

#endif  // LIBCYPHAL_NETWORK_POLL_HPP_INCLUDED
