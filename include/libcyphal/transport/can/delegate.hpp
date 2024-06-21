/// @copyright
/// Copyright (C) OpenCyphal Development Team  <opencyphal.org>
/// Copyright Amazon.com Inc. or its affiliates.
/// SPDX-License-Identifier: MIT

#ifndef LIBCYPHAL_TRANSPORT_CAN_DELEGATE_HPP_INCLUDED
#define LIBCYPHAL_TRANSPORT_CAN_DELEGATE_HPP_INCLUDED

#include "libcyphal/transport/errors.hpp"
#include "libcyphal/transport/scattered_buffer.hpp"
#include "libcyphal/transport/types.hpp"
#include "libcyphal/types.hpp"

#include <canard.h>
#include <cetl/cetl.hpp>
#include <cetl/pf17/cetlpf.hpp>
#include <cetl/rtti.hpp>

#include <algorithm>
#include <cstddef>
#include <cstdint>
#include <cstring>
#include <utility>

namespace libcyphal
{
namespace transport
{
namespace can
{

/// Internal implementation details of the CAN transport.
/// Not supposed to be used directly by the users of the library.
///
namespace detail
{

/// This internal transport delegate class serves the following purposes:
/// 1. It provides memory management functions for the Canard library.
/// 2. It provides a way to convert Canard error codes to `AnyError` type.
/// 3. It provides an interface to access the transport from various session classes.
///
class TransportDelegate
{
    // 1141F5C0-2E61-44BF-9F0E-FA1C518CD517
    using CanardMemoryTypeIdType = cetl::
        // NOLINTNEXTLINE(cppcoreguidelines-avoid-magic-numbers, readability-magic-numbers)
        type_id_type<0x11, 0x41, 0xF5, 0xC0, 0x2E, 0x61, 0x44, 0xBF, 0x9F, 0x0E, 0xFA, 0x1C, 0x51, 0x8C, 0xD5, 0x17>;

public:
    /// @brief RAII class to manage memory allocated by Canard library.
    ///
    /// NOSONAR cpp:S4963 for below `class CanardMemory` - we do directly handle resources here.
    ///
    class CanardMemory final  // NOSONAR cpp:S4963
        : public cetl::rtti_helper<CanardMemoryTypeIdType, ScatteredBuffer::IStorage>
    {
    public:
        CanardMemory(TransportDelegate& delegate, cetl::byte* const buffer, const std::size_t payload_size)
            : delegate_{delegate}
            , buffer_{buffer}
            , payload_size_{payload_size}
        {
        }
        CanardMemory(CanardMemory&& other) noexcept
            : delegate_{other.delegate_}
            , buffer_{std::exchange(other.buffer_, nullptr)}
            , payload_size_{std::exchange(other.payload_size_, 0)}
        {
        }
        CanardMemory(const CanardMemory&) = delete;

        ~CanardMemory() override
        {
            if (buffer_ != nullptr)
            {
                delegate_.freeCanardMemory(buffer_);
            }
        }

        CanardMemory& operator=(const CanardMemory&)     = delete;
        CanardMemory& operator=(CanardMemory&&) noexcept = delete;

        // MARK: ScatteredBuffer::IStorage

        CETL_NODISCARD std::size_t size() const noexcept override
        {
            return payload_size_;
        }

        CETL_NODISCARD std::size_t copy(const std::size_t offset_bytes,
                                        cetl::byte* const destination,
                                        const std::size_t length_bytes) const override
        {
            CETL_DEBUG_ASSERT((destination != nullptr) || (length_bytes == 0),
                              "Destination could be null only with zero bytes ask.");

            if ((destination == nullptr) || (buffer_ == nullptr) || (payload_size_ <= offset_bytes))
            {
                return 0;
            }

            const std::size_t bytes_to_copy = std::min(length_bytes, payload_size_ - offset_bytes);
            // Next nolint is unavoidable: we need offset from the beginning of the buffer.
            // NOLINTNEXTLINE(cppcoreguidelines-pro-bounds-pointer-arithmetic)
            (void) std::memmove(destination, buffer_ + offset_bytes, bytes_to_copy);
            return bytes_to_copy;
        }

    private:
        // MARK: Data members:

        TransportDelegate& delegate_;
        cetl::byte*        buffer_;
        std::size_t        payload_size_;

    };  // CanardMemory

    /// @brief Utility type with various helpers related to Canard AVL trees.
    ///
    /// @tparam Node Type of a concrete AVL tree node.
    ///
    template <typename Node>
    struct CanardConcreteTree
    {
        /// @brief Recursively visits each node of the AVL tree, counting total number of nodes visited.
        ///
        /// Recursion goes first to the left child, then to the current node, and finally to the right child.
        /// B/c AVL tree is balanced, the total complexity is `O(n)` and call stack depth should not be deeper than
        /// ~O(log(N)) (`ceil(1.44 * log2(N + 2) - 0.328)` to be precise, or 19 in case of 8192 ports),
        /// where `N` is the total number of tree nodes. Hence, the `NOLINTNEXTLINE(misc-no-recursion)`
        /// and `NOSONAR cpp:S925` exceptions.
        ///
        /// @tparam Visitor Type of the visitor callable.
        /// @param node (sub-)root node of the AVL tree. Could be `nullptr`.
        /// @param visitor The callable to be invoked for each non-null node.
        /// @return Total count of visited nodes (including the `node` one). `0` if `node` is `nullptr`.
        ///
        template <typename Visitor>
        // NOLINTNEXTLINE(misc-no-recursion)
        static std::size_t visitCounting(CanardTreeNode* const node, const Visitor& visitor)
        {
            if (node == nullptr)
            {
                return 0;
            }

            // Initial `1` is for the current node.
            std::size_t count = 1;

            count += visitCounting(node->lr[0], visitor);  // NOSONAR cpp:S925

            // Next nolint & NOSONAR are unavoidable: this is integration with low-level C code of Canard AVL trees.
            // NOLINTNEXTLINE(cppcoreguidelines-pro-type-reinterpret-cast)
            visitor(*reinterpret_cast<Node*>(node));  // NOSONAR cpp:S3630

            count += visitCounting(node->lr[1], visitor);  // NOSONAR cpp:S925

            return count;
        }

    };  // CanardConcreteTree

    struct FiltersUpdate
    {
        struct SubjectPort
        {
            bool is_added;
        };
        struct ServicePort
        {
            bool is_added;
        };

        using Variant = cetl::variant<SubjectPort, ServicePort>;

    };  // FiltersUpdate

    explicit TransportDelegate(cetl::pmr::memory_resource& memory)
        : memory_{memory}
        , canard_instance_{::canardInit(allocateMemoryForCanard, freeCanardMemory)}
    {
        canard_instance().user_reference = this;
    }

    TransportDelegate(const TransportDelegate&)                = delete;
    TransportDelegate(TransportDelegate&&) noexcept            = delete;
    TransportDelegate& operator=(const TransportDelegate&)     = delete;
    TransportDelegate& operator=(TransportDelegate&&) noexcept = delete;

    CETL_NODISCARD NodeId node_id() const noexcept
    {
        return canard_instance_.node_id;
    }

    CETL_NODISCARD CanardNodeID& canard_node_id() noexcept
    {
        return canard_instance_.node_id;
    }

    CETL_NODISCARD CanardInstance& canard_instance() noexcept
    {
        return canard_instance_;
    }

    CETL_NODISCARD cetl::pmr::memory_resource& memory() const noexcept
    {
        return memory_;
    }

    static cetl::optional<AnyError> optAnyErrorFromCanard(const std::int32_t result)
    {
        // Canard error results are negative, so we need to negate them to get the error code.
        const std::int32_t canard_error = -result;

        if (canard_error == CANARD_ERROR_INVALID_ARGUMENT)
        {
            return ArgumentError{};
        }
        if (canard_error == CANARD_ERROR_OUT_OF_MEMORY)
        {
            return MemoryError{};
        }

        return cetl::nullopt;
    }

    /// @brief Releases memory allocated for canard (by previous `allocateMemoryForCanard` call).
    ///
    /// NOSONAR cpp:S5008 b/s it is unavoidable: this is integration with low-level C code of Canard memory management.
    ///
    void freeCanardMemory(void* const pointer) const  // NOSONAR cpp:S5008
    {
        if (pointer == nullptr)
        {
            return;
        }

        auto* memory_header = static_cast<CanardMemoryHeader*>(pointer);
        // Next nolint is unavoidable: this is integration with C code of Canard memory management.
        // NOLINTNEXTLINE(cppcoreguidelines-pro-bounds-pointer-arithmetic)
        --memory_header;

        memory_.deallocate(memory_header, memory_header->size);
    }

    /// @brief Sends transfer to each media canard TX queue of the transport.
    ///
    /// Internal method which is in use by TX session implementations to delegate actual sending to transport.
    ///
    CETL_NODISCARD virtual cetl::optional<AnyError> sendTransfer(const TimePoint               deadline,
                                                                 const CanardTransferMetadata& metadata,
                                                                 const PayloadFragments        payload_fragments) = 0;

    /// @brief Triggers update of media filters due to a change of RX ports.
    ///
    /// Actual update will be done on next `run` of transport.
    ///
    /// @param update_var Describes variant of which the RX ports update has happened.
    ///                   Allows to distinguish between subject and service ports.
    ///
    virtual void triggerUpdateOfFilters(const FiltersUpdate::Variant& update_var) = 0;

protected:
    ~TransportDelegate() = default;

private:
    // Until "canardMemFree must provide size" issue #216 is resolved,
    // we need to store the size of the memory allocated.
    // TODO: Remove this workaround when the issue is resolved.
    // see https://github.com/OpenCyphal/libcanard/issues/216
    //
    struct CanardMemoryHeader final
    {
        alignas(std::max_align_t) std::size_t size;
    };

    /// @brief Converts Canard instance to the transport delegate.
    ///
    /// In use to bridge two worlds: canard library and transport entities.
    ///
    CETL_NODISCARD static TransportDelegate& getSelfFrom(const CanardInstance* const ins)
    {
        CETL_DEBUG_ASSERT(ins != nullptr, "Expected canard instance.");
        CETL_DEBUG_ASSERT(ins->user_reference != nullptr, "Expected `this` transport as user reference.");

        // NOSONAR cpp:S5357 b/c the raw `user_reference` is part of libcanard api,
        // and it was set by us at this delegate constructor (see `TransportDelegate` ctor).
        return *static_cast<TransportDelegate*>(ins->user_reference);
    }

    /// @brief Allocates memory for canard instance.
    ///
    /// Implicitly stores the size of the allocated memory in the prepended `CanardMemoryHeader` struct,
    /// so that it will possible later (at `freeCanardMemory`) restore the original size.
    ///
    /// NOSONAR cpp:S995 is unavoidable: this is integration with low-level C code
    /// of Canard memory management (@see ::canardInit).
    ///
    CETL_NODISCARD static void* allocateMemoryForCanard(CanardInstance* ins, std::size_t amount)  // NOSONAR cpp:S995
    {
        TransportDelegate& self = getSelfFrom(ins);

        const std::size_t memory_size   = sizeof(CanardMemoryHeader) + amount;
        auto*             memory_header = static_cast<CanardMemoryHeader*>(self.memory_.allocate(memory_size));
        if (memory_header == nullptr)
        {
            return nullptr;
        }

        // Return the memory after the `CanardMemoryHeader` struct (containing its size).
        // The size is used in `canardMemoryFree` to deallocate the memory.
        //
        memory_header->size = memory_size;
        // Next nolint is unavoidable: this is integration with C code of Canard memory management.
        // NOLINTNEXTLINE(cppcoreguidelines-pro-bounds-pointer-arithmetic)
        return ++memory_header;
    }

    /// @brief Releases memory allocated for canard instance (by previous `allocateMemoryForCanard` call).
    ///
    /// NOSONAR cpp:S995 & cpp:S5008 are unavoidable: this is integration with low-level C code
    /// of Canard memory management (@see ::canardInit).
    ///
    static void freeCanardMemory(CanardInstance* ins,  // NOSONAR cpp:S995
                                 void*           pointer)        // NOSONAR cpp:S5008
    {
        const TransportDelegate& self = getSelfFrom(ins);
        self.freeCanardMemory(pointer);
    }

    // MARK: Data members:

    cetl::pmr::memory_resource& memory_;
    CanardInstance              canard_instance_;

};  // TransportDelegate

// MARK: -

/// This internal session delegate class serves the following purpose: it provides an interface (aka gateway)
/// to access RX session from transport (by casting canard's `user_reference` member to this class).
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
    virtual void acceptRxTransfer(const CanardRxTransfer& transfer) = 0;

protected:
    IRxSessionDelegate()  = default;
    ~IRxSessionDelegate() = default;

};  // IRxSessionDelegate

}  // namespace detail
}  // namespace can
}  // namespace transport
}  // namespace libcyphal

#endif  // LIBCYPHAL_TRANSPORT_CAN_DELEGATE_HPP_INCLUDED
