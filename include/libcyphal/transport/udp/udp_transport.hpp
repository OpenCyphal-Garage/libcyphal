/// @copyright
/// Copyright (C) OpenCyphal Development Team  <opencyphal.org>
/// Copyright Amazon.com Inc. or its affiliates.
/// SPDX-License-Identifier: MIT

#ifndef LIBCYPHAL_TRANSPORT_UDP_TRANSPORT_HPP_INCLUDED
#define LIBCYPHAL_TRANSPORT_UDP_TRANSPORT_HPP_INCLUDED

#include "libcyphal/transport/transport.hpp"

#include <cetl/pf17/cetlpf.hpp>

namespace libcyphal
{
namespace transport
{
namespace udp
{

/// @brief Defines interface of UDP transport layer.
///
class IUdpTransport : public ITransport
{
public:
    IUdpTransport(const IUdpTransport&)                = delete;
    IUdpTransport(IUdpTransport&&) noexcept            = delete;
    IUdpTransport& operator=(const IUdpTransport&)     = delete;
    IUdpTransport& operator=(IUdpTransport&&) noexcept = delete;

protected:
    IUdpTransport()  = default;
    ~IUdpTransport() = default;

};  // IUdpTransport

/// @brief Specifies set of memory resources used by the UDP transport.
///
struct MemoryResourcesSpec
{
    /// The general purpose memory resource is used to provide memory for the libcyphal library.
    /// It is NOT used for any Udpard TX or RX transfers, payload (de)fragmentation or transient handles,
    /// but only for the libcyphal internal needs (like `make*[Rx|Tx]Session` factory calls).
    cetl::pmr::memory_resource& general;

    /// The session memory resource is used to provide memory for the Udpard session instances.
    /// Each instance is fixed-size, so a trivial zero-fragmentation block allocator is sufficient.
    /// If `nullptr` then the `.general` memory resource will be used instead.
    cetl::pmr::memory_resource* session{nullptr};

    /// The fragment handles are allocated per payload fragment; each handle contains a pointer to its fragment.
    /// Each instance is of a very small fixed size, so a trivial zero-fragmentation block allocator is sufficient.
    /// If `nullptr` then the `.general` memory resource will be used instead.
    cetl::pmr::memory_resource* fragment{nullptr};

    /// The library never allocates payload buffers itself, as they are handed over by the application via
    /// receive calls. Once a buffer is handed over, the library may choose to keep it if it is deemed to be
    /// necessary to complete a transfer reassembly, or to discard it if it is deemed to be unnecessary.
    /// Discarded payload buffers are freed using this memory resource.
    /// If `nullptr` then the `.general` memory resource will be used instead.
    cetl::pmr::memory_resource* payload{nullptr};

};  // MemoryResourcesSpec

}  // namespace udp
}  // namespace transport
}  // namespace libcyphal

#endif  // LIBCYPHAL_TRANSPORT_UDP_TRANSPORT_HPP_INCLUDED
