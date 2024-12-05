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
/// 2. It provides a way to convert Canard error codes to `AnyFailure` type.
/// 3. It provides an interface to access the transport from various session classes.
///
class TransportDelegate
{
public:
    /// @brief RAII class to manage memory allocated by Canard library.
    ///
    /// NOSONAR cpp:S4963 for below `class CanardMemory` - we do directly handle resources here.
    ///
    class CanardMemory final : public ScatteredBuffer::IStorage  // NOSONAR cpp:S4963
    {
    public:
        CanardMemory(TransportDelegate& delegate,
                     const std::size_t  allocated_size,
                     cetl::byte* const  buffer,
                     const std::size_t  payload_size)
            : delegate_{delegate}
            , allocated_size_{allocated_size}
            , buffer_{buffer}
            , payload_size_{payload_size}
        {
        }
        CanardMemory(CanardMemory&& other) noexcept
            : delegate_{other.delegate_}
            , allocated_size_{std::exchange(other.allocated_size_, 0)}
            , buffer_{std::exchange(other.buffer_, nullptr)}
            , payload_size_{std::exchange(other.payload_size_, 0)}
        {
        }
        CanardMemory(const CanardMemory&) = delete;

        ~CanardMemory()
        {
            if (buffer_ != nullptr)
            {
                // No Sonar `cpp:S5356` b/c we integrate here with C libcanard memory management.
                delegate_.freeCanardMemory(buffer_, allocated_size_);  // NOSONAR cpp:S5356
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
            // No Sonar `cpp:S5356` b/c we integrate here with libcanard raw C buffers.
            // NOLINTNEXTLINE(cppcoreguidelines-pro-bounds-pointer-arithmetic)
            (void) std::memmove(destination, buffer_ + offset_bytes, bytes_to_copy);  // NOSONAR cpp:S5356
            return bytes_to_copy;
        }

    private:
        // MARK: Data members:

        TransportDelegate& delegate_;
        std::size_t        allocated_size_;
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
        /// @brief Visits in-order each node of the AVL tree, counting total number of nodes visited.
        ///
        /// @tparam Visitor Type of the visitor callable.
        /// @param root The root node of the AVL tree. Could be `nullptr`.
        /// @param visitor The callable to be invoked for each non-null node.
        /// @return Total count of visited nodes (including the `root` one). `0` if `root` is `nullptr`.
        ///
        /// No Sonar cpp:S134 b/c this is usual in-order traversal - maintainability is not a concern here.
        ///
        template <typename Visitor>
        static std::size_t visitCounting(CanardTreeNode* const root, const Visitor& visitor)
        {
            std::size_t           count = 0;
            CanardTreeNode*       node  = root;
            const CanardTreeNode* prev  = nullptr;

            while (nullptr != node)
            {
                CanardTreeNode* next = node->up;

                if (prev == node->up)
                {
                    // We came down to this node from `prev`.

                    if (auto* const left = node->lr[0])
                    {
                        next = left;
                    }
                    else
                    {
                        ++count;
                        visitor(down(*node));

                        if (auto* const right = node->lr[1])  // NOSONAR cpp:S134
                        {
                            next = right;
                        }
                    }
                }
                else if (prev == node->lr[0])
                {
                    // We came up to this node from the left child.

                    ++count;
                    visitor(down(*node));

                    if (auto* const right = node->lr[1])
                    {
                        next = right;
                    }
                }
                else
                {
                    // next has already been set to the parent node.
                }

                prev = std::exchange(node, next);
            }

            return count;
        }

    private:
        static Node& down(CanardTreeNode& node)
        {
            // Next nolint & NOSONAR are unavoidable: this is integration with Canard C AVL trees.
            // NOLINTNEXTLINE(cppcoreguidelines-pro-type-reinterpret-cast)
            return reinterpret_cast<Node&>(node);  // NOSONAR cpp:S3630
        }

    };  // CanardConcreteTree

    struct SessionEvent
    {
        struct MsgRxLifetime
        {
            bool is_added;
        };
        struct SvcRxLifetime
        {
            bool is_added;
        };

        using Variant = cetl::variant<MsgRxLifetime, SvcRxLifetime>;

    };  // SessionEvent

    TransportDelegate(const TransportDelegate&)                = delete;
    TransportDelegate(TransportDelegate&&) noexcept            = delete;
    TransportDelegate& operator=(const TransportDelegate&)     = delete;
    TransportDelegate& operator=(TransportDelegate&&) noexcept = delete;

    CETL_NODISCARD NodeId getNodeId() const noexcept
    {
        return canard_instance_.node_id;
    }

    void setNodeId(const NodeId node_id) noexcept
    {
        canard_instance_.node_id = static_cast<CanardNodeID>(node_id);
    }

    CETL_NODISCARD CanardInstance& canardInstance() noexcept
    {
        return canard_instance_;
    }

    CETL_NODISCARD const CanardInstance& canardInstance() const noexcept
    {
        return canard_instance_;
    }

    CETL_NODISCARD cetl::pmr::memory_resource& memory() const noexcept
    {
        return memory_;
    }

    CETL_NODISCARD static cetl::optional<AnyFailure> optAnyFailureFromCanard(const std::int32_t result)
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
    /// No Sonar `cpp:S5008` and `cpp:S5356` b/c they are unavoidable -
    /// this is integration with low-level Canard C memory management.
    ///
    void freeCanardMemory(void* const pointer, const std::size_t amount) const  // NOSONAR cpp:S5008
    {
        if (pointer == nullptr)
        {
            return;
        }

        memory_.deallocate(pointer, amount);
    }

    /// Pops and frees Canard TX queue item(s).
    ///
    /// @param tx_queue The TX queue from which the item should be popped.
    /// @param canard_instance The Canard instance to be used for the item deallocation.
    /// @param tx_item The TX queue item to be popped and freed.
    /// @param whole_transfer If `true` then whole transfer should be released from the queue.
    ///
    static void popAndFreeCanardTxQueueItem(CanardTxQueue&        tx_queue,
                                            const CanardInstance& canard_instance,
                                            CanardTxQueueItem*    tx_item,
                                            const bool            whole_transfer)
    {
        while (CanardTxQueueItem* const mut_tx_item = ::canardTxPop(&tx_queue, tx_item))
        {
            tx_item = tx_item->next_in_transfer;
            ::canardTxFree(&tx_queue, &canard_instance, mut_tx_item);

            if (!whole_transfer)
            {
                break;
            }
        }
    }

    /// @brief Sends transfer to each media canard TX queue of the transport.
    ///
    /// Internal method which is in use by TX session implementations to delegate actual sending to transport.
    ///
    CETL_NODISCARD virtual cetl::optional<AnyFailure> sendTransfer(const TimePoint               deadline,
                                                                   const CanardTransferMetadata& metadata,
                                                                   const PayloadFragments        payload_fragments) = 0;

    /// @brief Called on a session event.
    ///
    /// @param event_var Describes variant of the session even has happened.
    ///
    virtual void onSessionEvent(const SessionEvent::Variant& event_var) = 0;

protected:
    explicit TransportDelegate(cetl::pmr::memory_resource& memory)
        : memory_{memory}
        , canard_instance_{::canardInit(makeCanardMemoryResource())}
    {
        // No Sonar `cpp:S5356` b/c we integrate here with C libcanard API.
        canardInstance().user_reference = this;  // NOSONAR cpp:S5356
    }

    ~TransportDelegate() = default;

private:
    /// @brief Converts Canard instance to the transport delegate.
    ///
    /// In use to bridge two worlds: canard library and transport entities.
    /// NOSONAR  cpp:S5008 is unavoidable: this is integration with low-level Canard C memory management.
    ///
    CETL_NODISCARD static TransportDelegate& getSelfFrom(void* const user_reference)  // NOSONAR cpp:S5008
    {
        CETL_DEBUG_ASSERT(user_reference != nullptr, "Expected `this` transport as user reference.");

        // No Sonar `cpp:S5357` b/c the raw `user_reference` is part of libcanard api,
        // and it was set by us at this delegate constructor (see `TransportDelegate` ctor).
        return *static_cast<TransportDelegate*>(user_reference);  // NOSONAR cpp:S5357
    }

    /// @brief Allocates memory for canard instance.
    ///
    /// NOSONAR  cpp:S5008 is unavoidable: this is integration with low-level Canard C memory management.
    ///
    CETL_NODISCARD static void* allocateMemoryForCanard(void* const       user_reference,  // NOSONAR cpp:S5008
                                                        const std::size_t amount)
    {
        const TransportDelegate& self = getSelfFrom(user_reference);
        return self.memory_.allocate(amount);
    }

    /// @brief Releases memory allocated for canard instance (by previous `allocateMemoryForCanard` call).
    ///
    /// NOSONAR  cpp:S5008 is unavoidable: this is integration with low-level Canard C memory management.
    ///
    static void freeCanardMemory(void* const       user_reference,  // NOSONAR cpp:S5008
                                 const std::size_t amount,
                                 void* const       pointer)  // NOSONAR cpp:S5008
    {
        const TransportDelegate& self = getSelfFrom(user_reference);
        self.freeCanardMemory(pointer, amount);
    }

    CETL_NODISCARD CanardMemoryResource makeCanardMemoryResource()
    {
        // No Sonar `cpp:S5356` b/c we integrate here with C libcanard memory management.
        return {this, freeCanardMemory, allocateMemoryForCanard};  // NOSONAR cpp:S5356
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
