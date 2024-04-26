/// @copyright
/// Copyright (C) OpenCyphal Development Team  <opencyphal.org>
/// Copyright Amazon.com Inc. or its affiliates.
/// SPDX-License-Identifier: MIT

#ifndef LIBCYPHAL_TRANSPORT_CAN_DELEGATE_HPP_INCLUDED
#define LIBCYPHAL_TRANSPORT_CAN_DELEGATE_HPP_INCLUDED

#include <libcyphal/transport/errors.hpp>
#include <libcyphal/transport/scattered_buffer.hpp>

#include <cetl/rtti.hpp>

#include <canard.h>

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
/// 3. It provides interface to access the transport from various session classes.
///
class TransportDelegate
{
    // 1141F5C0-2E61-44BF-9F0E-FA1C518CD517
    using CanardMemoryTypeIdType = cetl::
        type_id_type<0x11, 0x41, 0xF5, 0xC0, 0x2E, 0x61, 0x44, 0xBF, 0x9F, 0x0E, 0xFA, 0x1C, 0x51, 0x8C, 0xD5, 0x17>;

public:
    /// @brief RAII class to manage memory allocated by Canard library.
    ///
    class CanardMemory final : public cetl::rtti_helper<CanardMemoryTypeIdType, ScatteredBuffer::Interface>
    {
    public:
        CanardMemory(TransportDelegate& delegate, void* const buffer, const std::size_t payload_size)
            : delegate_{delegate}
            , buffer_{buffer}
            , payload_size_{payload_size}
        {
        }
        CanardMemory(CanardMemory&& other) noexcept
            : delegate_{other.delegate_}
            , buffer_{other.buffer_}
            , payload_size_{other.payload_size_}
        {
            other.buffer_       = nullptr;
            other.payload_size_ = 0;
        }
        CanardMemory(const CanardMemory&) = delete;

        ~CanardMemory() final
        {
            if (buffer_ != nullptr)
            {
                delegate_.freeCanardMemory(buffer_);
            }
        }

        CanardMemory& operator=(const CanardMemory&)     = delete;
        CanardMemory& operator=(CanardMemory&&) noexcept = delete;

        // MARK: ScatteredBuffer::Interface

        CETL_NODISCARD std::size_t size() const noexcept final
        {
            return payload_size_;
        }

        CETL_NODISCARD std::size_t copy(const std::size_t offset_bytes,
                                        void* const       destination,
                                        const std::size_t length_bytes) const final
        {
            CETL_DEBUG_ASSERT((destination != nullptr) || (length_bytes == 0),
                              "Destination could be null only with zero bytes ask.");

            if ((destination == nullptr) || (buffer_ == nullptr) || (payload_size_ <= offset_bytes))
            {
                return 0;
            }

            const auto bytes_to_copy = std::min(length_bytes, payload_size_ - offset_bytes);
            std::memmove(destination, static_cast<cetl::byte*>(buffer_) + offset_bytes, bytes_to_copy);
            return bytes_to_copy;
        }

    private:
        // MARK: Data members:

        TransportDelegate& delegate_;
        void*              buffer_;
        std::size_t        payload_size_;

    };  // CanardMemory

    explicit TransportDelegate(cetl::pmr::memory_resource& memory)
        : memory_{memory}
        , canard_instance_(::canardInit(allocateMemoryForCanard, freeCanardMemory))
    {
        canard_instance().user_reference = this;
    }

    CETL_NODISCARD CanardInstance& canard_instance() noexcept
    {
        return canard_instance_;
    }

    CETL_NODISCARD const CanardInstance& canard_instance() const noexcept
    {
        return canard_instance_;
    }

    CETL_NODISCARD cetl::pmr::memory_resource& memory() const noexcept
    {
        return memory_;
    }

    static cetl::optional<AnyError> anyErrorFromCanard(const std::int32_t result)
    {
        // Canard error results are negative, so we need to negate them to get the error code.
        const auto canard_error = static_cast<std::int32_t>(-result);

        if (canard_error == CANARD_ERROR_INVALID_ARGUMENT)
        {
            return ArgumentError{};
        }
        if (canard_error == CANARD_ERROR_OUT_OF_MEMORY)
        {
            return MemoryError{};
        }

        return {};
    }

    /// @brief Releases memory allocated for canard (by previous `allocateMemoryForCanard` call).
    ///
    void freeCanardMemory(void* const pointer)
    {
        if (pointer == nullptr)
        {
            return;
        }

        auto memory_header = static_cast<CanardMemoryHeader*>(pointer);
        --memory_header;

        memory_.deallocate(memory_header, memory_header->size);
    }

    /// @brief Sends transfer to each media canard TX queue of the transport.
    ///
    /// Internal method which is in use by TX session implementations to delegate actual sending to transport.
    ///
    CETL_NODISCARD virtual cetl::optional<AnyError> sendTransfer(const CanardMicrosecond       deadline,
                                                                 const CanardTransferMetadata& metadata,
                                                                 const void* const             payload,
                                                                 const std::size_t             payload_size) = 0;

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

        return *static_cast<TransportDelegate*>(ins->user_reference);
    }

    /// @brief Allocates memory for canard instance.
    ///
    /// Implicitly stores the size of the allocated memory in the prepended `CanardMemoryHeader` struct,
    /// so that it will possible later (at `freeCanardMemory`) restore the original size.
    ///
    CETL_NODISCARD static void* allocateMemoryForCanard(CanardInstance* ins, std::size_t amount)
    {
        auto& self = getSelfFrom(ins);

        const std::size_t memory_size   = sizeof(CanardMemoryHeader) + amount;
        auto              memory_header = static_cast<CanardMemoryHeader*>(self.memory_.allocate(memory_size));
        if (memory_header == nullptr)
        {
            return nullptr;
        }

        // Return the memory after the `CanardMemoryHeader` struct (containing its size).
        // The size is used in `canardMemoryFree` to deallocate the memory.
        //
        memory_header->size = memory_size;
        return ++memory_header;
    }

    /// @brief Releases memory allocated for canard instance (by previous `allocateMemoryForCanard` call).
    ///
    static void freeCanardMemory(CanardInstance* ins, void* pointer)
    {
        auto& self = getSelfFrom(ins);
        self.freeCanardMemory(pointer);
    }

    // MARK: Data members:

    cetl::pmr::memory_resource& memory_;
    CanardInstance              canard_instance_;

};  // TransportDelegate

/// This internal session delegate class serves the following purpose: it provides interface
/// to access a session from transport (by casting canard's `user_reference` member to this class).
///
class SessionDelegate
{
public:
    SessionDelegate(const SessionDelegate&)                = delete;
    SessionDelegate(SessionDelegate&&) noexcept            = delete;
    SessionDelegate& operator=(const SessionDelegate&)     = delete;
    SessionDelegate& operator=(SessionDelegate&&) noexcept = delete;

    /// @brief Accepts a received transfer from the transport dedicated to this RX session.
    ///
    virtual void acceptRxTransfer(const CanardRxTransfer& transfer) = 0;

protected:
    SessionDelegate()  = default;
    ~SessionDelegate() = default;

};  // SessionDelegate

}  // namespace detail
}  // namespace can
}  // namespace transport
}  // namespace libcyphal

#endif  // LIBCYPHAL_TRANSPORT_CAN_DELEGATE_HPP_INCLUDED
