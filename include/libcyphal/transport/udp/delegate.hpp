/// @copyright
/// Copyright (C) OpenCyphal Development Team  <opencyphal.org>
/// Copyright Amazon.com Inc. or its affiliates.
/// SPDX-License-Identifier: MIT

#ifndef LIBCYPHAL_TRANSPORT_UDP_DELEGATE_HPP_INCLUDED
#define LIBCYPHAL_TRANSPORT_UDP_DELEGATE_HPP_INCLUDED

#include "libcyphal/transport/errors.hpp"
#include "libcyphal/transport/types.hpp"
#include "libcyphal/types.hpp"

#include <cetl/cetl.hpp>
#include <cetl/pf17/cetlpf.hpp>
#include <udpard.h>

#include <cstddef>
#include <cstdint>
#include <utility>

namespace libcyphal
{
namespace transport
{
namespace udp
{

/// Internal implementation details of the UDP transport.
/// Not supposed to be used directly by the users of the library.
///
namespace detail
{

/// This internal transport delegate class serves the following purposes:
/// 1. It provides memory management functions for the Udpard library.
/// 2. It provides a way to convert Udpard error codes to `AnyError` type.
/// 3. It provides an interface to access the transport from various session classes.
///
class TransportDelegate
{
protected:
    /// @brief Defines internal set of memory resources used by the UDP transport.
    ///
    struct MemoryResources
    {
        /// The general purpose memory resource is used to provide memory for the libcyphal library.
        /// It is NOT used for any Udpard TX or RX transfers, payload (de)fragmentation or transient handles,
        /// but only for the libcyphal internal needs (like `make*[Rx|Tx]Session` factory calls).
        cetl::pmr::memory_resource& general;

        /// The session memory resource is used to provide memory for the Udpard session instances.
        /// Each instance is fixed-size, so a trivial zero-fragmentation block allocator is sufficient.
        UdpardMemoryResource session;

        /// The fragment handles are allocated per payload fragment; each handle contains a pointer to its fragment.
        /// Each instance is of a very small fixed size, so a trivial zero-fragmentation block allocator is sufficient.
        UdpardMemoryResource fragment;

        /// The library never allocates payload buffers itself, as they are handed over by the application via
        /// receive calls. Once a buffer is handed over, the library may choose to keep it if it is deemed to be
        /// necessary to complete a transfer reassembly, or to discard it if it is deemed to be unnecessary.
        /// Discarded payload buffers are freed using this memory resource.
        UdpardMemoryDeleter payload;
    };

public:
    explicit TransportDelegate(MemoryResources memory_resources)
        : memory_resources_{memory_resources}
    {
    }

    TransportDelegate(const TransportDelegate&)                = delete;
    TransportDelegate(TransportDelegate&&) noexcept            = delete;
    TransportDelegate& operator=(const TransportDelegate&)     = delete;
    TransportDelegate& operator=(TransportDelegate&&) noexcept = delete;

    CETL_NODISCARD MemoryResources& memoryResources() noexcept
    {
        return memory_resources_;
    }

    static cetl::optional<AnyError> optAnyErrorFromUdpard(const std::int32_t result)
    {
        // Udpard error results are negative, so we need to negate them to get the error code.
        const std::int32_t canard_error = -result;

        if (canard_error == UDPARD_ERROR_ARGUMENT)
        {
            return ArgumentError{};
        }
        if (canard_error == UDPARD_ERROR_MEMORY)
        {
            return MemoryError{};
        }
        if (canard_error == UDPARD_ERROR_CAPACITY)
        {
            return CapacityError{};
        }
        if (canard_error == UDPARD_ERROR_ANONYMOUS)
        {
            return AnonymousError{};
        }

        return cetl::nullopt;
    }

protected:
    ~TransportDelegate() = default;

    static UdpardMemoryResource makeUdpardMemoryResource(cetl::pmr::memory_resource* const custom,
                                                         cetl::pmr::memory_resource&       general)
    {
        return UdpardMemoryResource{(custom != nullptr) ? custom : &general,
                                    udpardMemoryDeallocate,
                                    udpardMemoryAllocate};
    }

    static UdpardMemoryDeleter makeUdpardMemoryDeleter(cetl::pmr::memory_resource* const custom,
                                                       cetl::pmr::memory_resource&       general)
    {
        return UdpardMemoryDeleter{(custom != nullptr) ? custom : &general, udpardMemoryDeallocate};
    }

private:
    /// @brief Converts type erased user pointer to the transport delegate.
    ///
    /// In use to bridge two worlds: Udpard library and transport entities.
    ///
    CETL_NODISCARD static TransportDelegate& getSelfFrom(void* const self)
    {
        CETL_DEBUG_ASSERT(self != nullptr, "Expected non-null pointer.");

        return *static_cast<TransportDelegate*>(self);
    }

    static void* udpardMemoryAllocate(void* const user_reference, const size_t size)
    {
        auto* mr = static_cast<cetl::pmr::memory_resource*>(user_reference);
        CETL_DEBUG_ASSERT(mr != nullptr, "Memory resource should not be null.");
        return mr->allocate(size);
    }

    static void udpardMemoryDeallocate(void* const user_reference, const size_t size, void* const pointer)
    {
        auto* mr = static_cast<cetl::pmr::memory_resource*>(user_reference);
        CETL_DEBUG_ASSERT(mr != nullptr, "Memory resource should not be null.");
        mr->deallocate(pointer, size);
    }

    // MARK: Data members:

    MemoryResources memory_resources_;

};  // TransportDelegate

// MARK: -

/// This internal session delegate class serves the following purpose: it provides an interface (aka gateway)
/// to access RX session from transport (by casting udpard `user_reference` member to this class).
///
class IRxSessionDelegate
{
public:
    IRxSessionDelegate(const IRxSessionDelegate&)                = delete;
    IRxSessionDelegate(IRxSessionDelegate&&) noexcept            = delete;
    IRxSessionDelegate& operator=(const IRxSessionDelegate&)     = delete;
    IRxSessionDelegate& operator=(IRxSessionDelegate&&) noexcept = delete;

    /// @brief Accepts a received transfer from the transport dedicated to this RX session.
    ///
    virtual void acceptRxTransfer(const UdpardRxTransfer& transfer) = 0;

protected:
    IRxSessionDelegate()  = default;
    ~IRxSessionDelegate() = default;

};  // IRxSessionDelegate

}  // namespace detail
}  // namespace udp
}  // namespace transport
}  // namespace libcyphal

#endif  // LIBCYPHAL_TRANSPORT_UDP_DELEGATE_HPP_INCLUDED
