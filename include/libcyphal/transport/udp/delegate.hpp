/// @copyright
/// Copyright (C) OpenCyphal Development Team  <opencyphal.org>
/// Copyright Amazon.com Inc. or its affiliates.
/// SPDX-License-Identifier: MIT

#ifndef LIBCYPHAL_TRANSPORT_UDP_DELEGATE_HPP_INCLUDED
#define LIBCYPHAL_TRANSPORT_UDP_DELEGATE_HPP_INCLUDED

#include "libcyphal/transport/errors.hpp"
#include "libcyphal/transport/scattered_buffer.hpp"
#include "libcyphal/transport/types.hpp"
#include "libcyphal/types.hpp"

#include <cetl/cetl.hpp>
#include <cetl/pf17/cetlpf.hpp>
#include <cetl/rtti.hpp>
#include <udpard.h>

#include <algorithm>
#include <cstddef>
#include <cstdint>
#include <cstring>
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

struct AnyUdpardTxMetadata
{
    struct Publish
    {
        UdpardMicrosecond deadline_us;
        UdpardPriority    priority;
        UdpardPortID      subject_id;
        UdpardTransferID  transfer_id;
    };
    struct Request
    {
        UdpardMicrosecond deadline_us;
        UdpardPriority    priority;
        UdpardPortID      service_id;
        UdpardNodeID      server_node_id;
        UdpardTransferID  transfer_id;
    };
    struct Respond
    {
        UdpardMicrosecond deadline_us;
        UdpardPriority    priority;
        UdpardPortID      service_id;
        UdpardNodeID      client_node_id;
        UdpardTransferID  transfer_id;
    };

    /// Defines variant of all possible transient error reports.
    ///
    using Variant = cetl::variant<Publish, Request, Respond>;

};  // AnyUdpardTxMetadata

/// This internal transport delegate class serves the following purposes:
/// 1. It provides memory management functions for the Udpard library.
/// 2. It provides a way to convert Udpard error codes to `AnyError` type.
/// 3. It provides an interface to access the transport from various session classes.
///
class TransportDelegate
{
    // FD977E25-CD36-48C5-B02E-B31420DCFF3B
    using UdpardMemoryTypeIdType = cetl::
        // NOLINTNEXTLINE(cppcoreguidelines-avoid-magic-numbers, readability-magic-numbers)
        type_id_type<0xFD, 0x97, 0x7E, 0x25, 0xCD, 0x36, 0x48, 0xC5, 0xB0, 0x2E, 0xB3, 0x14, 0x20, 0xDC, 0xFF, 0x3B>;

public:
    /// @brief RAII class to manage memory allocated by Udpard library.
    ///
    /// NOSONAR cpp:S4963 for below `class UdpardMemory` - we do directly handle resources here.
    ///
    class UdpardMemory final  // NOSONAR cpp:S4963
        : public cetl::rtti_helper<UdpardMemoryTypeIdType, ScatteredBuffer::IStorage>
    {
    public:
        UdpardMemory(TransportDelegate& delegate, UdpardRxTransfer& transfer)
            : delegate_{delegate}
            , payload_size_{std::exchange(transfer.payload_size, 0)}
            , payload_{std::exchange(transfer.payload, {})}
        {
        }
        UdpardMemory(UdpardMemory&& other) noexcept
            : delegate_{other.delegate_}
            , payload_size_{std::exchange(other.payload_size_, 0)}
            , payload_{std::exchange(other.payload_, {})}
        {
        }
        UdpardMemory(const UdpardMemory&) = delete;

        ~UdpardMemory() override
        {
            if ((payload_.origin.data != nullptr) || (payload_.next != nullptr))
            {
                ::udpardRxFragmentFree(payload_,
                                       delegate_.memoryResources().fragment,
                                       delegate_.memoryResources().payload);
            }
        }

        UdpardMemory& operator=(const UdpardMemory&)     = delete;
        UdpardMemory& operator=(UdpardMemory&&) noexcept = delete;

        // MARK: ScatteredBuffer::IStorage

        CETL_NODISCARD std::size_t size() const noexcept override
        {
            return payload_size_;
        }

        CETL_NODISCARD std::size_t copy(const std::size_t /* offset_bytes */,
                                        cetl::byte* const /* destination */,
                                        const std::size_t /* length_bytes */) const override
        {
            // TODO: Implement!
            return 0;
        }

    private:
        // MARK: Data members:

        TransportDelegate& delegate_;
        std::size_t        payload_size_;
        UdpardFragment     payload_;

    };  // UdpardMemory

    TransportDelegate(const TransportDelegate&)                = delete;
    TransportDelegate(TransportDelegate&&) noexcept            = delete;
    TransportDelegate& operator=(const TransportDelegate&)     = delete;
    TransportDelegate& operator=(TransportDelegate&&) noexcept = delete;

    CETL_NODISCARD NodeId node_id() const noexcept
    {
        return udpard_node_id_;
    }

    CETL_NODISCARD UdpardNodeID& udpard_node_id() noexcept
    {
        return udpard_node_id_;
    }

    static cetl::optional<AnyError> optAnyErrorFromUdpard(const std::int32_t result)
    {
        // Udpard error results are negative, so we need to negate them to get the error code.
        const std::int32_t udpard_error = -result;

        if (udpard_error == UDPARD_ERROR_ARGUMENT)
        {
            return ArgumentError{};
        }
        if (udpard_error == UDPARD_ERROR_MEMORY)
        {
            return MemoryError{};
        }
        if (udpard_error == UDPARD_ERROR_CAPACITY)
        {
            return CapacityError{};
        }
        if (udpard_error == UDPARD_ERROR_ANONYMOUS)
        {
            return AnonymousError{};
        }

        return cetl::nullopt;
    }

    /// Pops and frees Udpard TX queue item(s).
    ///
    /// @param tx_queue The TX queue from which the item should be popped.
    /// @param tx_item The TX queue item to be popped and freed.
    /// @param whole_transfer If `true` then whole transfer should be released from the queue.
    ///
    static void popAndFreeUdpardTxItem(UdpardTx* const tx_queue, const UdpardTxItem* tx_item, const bool whole_transfer)
    {
        while (UdpardTxItem* const mut_tx_item = ::udpardTxPop(tx_queue, tx_item))
        {
            tx_item = tx_item->next_in_transfer;

            ::udpardTxFree(tx_queue->memory, mut_tx_item);

            if (!whole_transfer)
            {
                break;
            }
        }
    }

    /// @brief Sends transfer to each media udpard TX queue of the transport.
    ///
    /// Internal method which is in use by TX session implementations to delegate actual sending to transport.
    ///
    CETL_NODISCARD virtual cetl::optional<AnyError> sendAnyTransfer(const AnyUdpardTxMetadata::Variant& tx_metadata_var,
                                                                    const PayloadFragments payload_fragments) = 0;

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

    explicit TransportDelegate(const MemoryResources& memory_resources)
        : udpard_node_id_{UDPARD_NODE_ID_UNSET}
        , memory_resources_{memory_resources}
    {
    }

    ~TransportDelegate() = default;

    CETL_NODISCARD const MemoryResources& memoryResources() const noexcept
    {
        return memory_resources_;
    }

    static UdpardMemoryResource makeUdpardMemoryResource(cetl::pmr::memory_resource* const custom,
                                                         cetl::pmr::memory_resource&       general)
    {
        // No Sonar `cpp:S5356` b/c the raw `user_reference` is part of libudpard api.
        void* const user_reference = (custom != nullptr) ? custom : &general;  // NOSONAR cpp:S5356
        return UdpardMemoryResource{user_reference, deallocateMemoryForUdpard, allocateMemoryForUdpard};
    }

    static UdpardMemoryDeleter makeUdpardMemoryDeleter(cetl::pmr::memory_resource* const custom,
                                                       cetl::pmr::memory_resource&       general)
    {
        // No Sonar `cpp:S5356` b/c the raw `user_reference` is part of libudpard api.
        void* const user_reference = (custom != nullptr) ? custom : &general;  // NOSONAR cpp:S5356
        return UdpardMemoryDeleter{user_reference, deallocateMemoryForUdpard};
    }

private:
    /// @brief Allocates memory for udpard.
    ///
    /// NOSONAR cpp:S5008 is unavoidable: this is integration with low-level C code of Udpard memory management.
    ///
    static void* allocateMemoryForUdpard(void* const user_reference, const size_t size)  // NOSONAR cpp:S5008
    {
        // No Sonar `cpp:S5356` and `cpp:S5357` b/c the raw `user_reference` is part of libudpard api,
        // and it was set by us at `makeUdpardMemoryResource` call.
        auto* const mr = static_cast<cetl::pmr::memory_resource*>(user_reference);  // NOSONAR cpp:S5356 cpp:S5357
        CETL_DEBUG_ASSERT(mr != nullptr, "Memory resource should not be null.");
        return mr->allocate(size);
    }

    /// @brief Releases memory allocated for udpard (by previous `allocateMemoryForUdpard` call).
    ///
    /// NOSONAR cpp:S5008 is unavoidable: this is integration with low-level C code of Udpard memory management.
    ///
    static void deallocateMemoryForUdpard(void* const  user_reference,  // NOSONAR cpp:S5008
                                          const size_t size,
                                          void* const  pointer)  // NOSONAR cpp:S5008
    {
        // No Sonar `cpp:S5356` and `cpp:S5357` b/c the raw `user_reference` is part of libudpard api,
        // and it was set by us at `makeUdpardMemoryResource` call.
        auto* const mr = static_cast<cetl::pmr::memory_resource*>(user_reference);  // NOSONAR cpp:S5356 cpp:S5357
        CETL_DEBUG_ASSERT(mr != nullptr, "Memory resource should not be null.");
        mr->deallocate(pointer, size);
    }

    // MARK: Data members:

    UdpardNodeID          udpard_node_id_;
    const MemoryResources memory_resources_;

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
    /// @param transfer The received transfer to be accepted. An implementation is expected to take ownership
    ///                 of the transfer payload, and to release it when it is no longer needed. On exit the original
    ///                 transfer's `payload_size` and `payload` fields are set to zero.
    ///
    virtual void acceptRxTransfer(UdpardRxTransfer& inout_transfer) = 0;

protected:
    IRxSessionDelegate()  = default;
    ~IRxSessionDelegate() = default;

};  // IRxSessionDelegate

}  // namespace detail
}  // namespace udp
}  // namespace transport
}  // namespace libcyphal

#endif  // LIBCYPHAL_TRANSPORT_UDP_DELEGATE_HPP_INCLUDED
