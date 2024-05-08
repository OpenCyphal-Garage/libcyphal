/// @copyright
/// Copyright (C) OpenCyphal Development Team  <opencyphal.org>
/// Copyright Amazon.com Inc. or its affiliates.
/// SPDX-License-Identifier: MIT

#ifndef LIBCYPHAL_TRANSPORT_SCATTERED_BUFFER_HPP_INCLUDED
#define LIBCYPHAL_TRANSPORT_SCATTERED_BUFFER_HPP_INCLUDED

#include <cetl/pf17/attribute.hpp>
#include <cetl/rtti.hpp>
#include <cetl/unbounded_variant.hpp>

#include <cstddef>
#include <type_traits>
#include <utility>

namespace libcyphal
{
namespace transport
{

/// @brief Represents a buffer that could be scattered across multiple memory regions of an abstract storage.
///
/// The buffer is movable but not copyable because copying the contents of a buffer is considered wasteful.
/// The buffer behaves as if it's empty if the underlying implementation is moved away.
///
class ScatteredBuffer final
{
    // 91C1B109-F90E-45BE-95CF-6ED02AC3FFAA
    using IStorageTypeIdType = cetl::
        // NOLINTNEXTLINE(cppcoreguidelines-avoid-magic-numbers, readability-magic-numbers)
        type_id_type<0x91, 0xC1, 0xB1, 0x09, 0xF9, 0x0E, 0x45, 0xBE, 0x95, 0xCF, 0x6E, 0xD0, 0x2A, 0xC3, 0xFF, 0xAA>;

public:
    /// @brief Defines maximum size (aka footprint) of the storage variant.
    ///
    static constexpr std::size_t StorageVariantFootprint = sizeof(void*) * 8;

    /// @brief Defines storage interface for the scattered buffer.
    ///
    /// @see ScatteredBuffer::ScatteredBuffer(AnyStorage&& any_storage)
    ///
    class IStorage : public cetl::rtti_helper<IStorageTypeIdType>
    {
    public:
        ~IStorage() override = default;

        // No copying, but move only!
        IStorage(const IStorage&)            = delete;
        IStorage& operator=(const IStorage&) = delete;

        /// @brief Gets the total number of bytes stored in the buffer.
        ///
        /// The storage could be possibly scattered, but this is hidden from the user.
        ///
        CETL_NODISCARD virtual std::size_t size() const noexcept = 0;

        /// @brief Copies a fragment of the specified size at the specified offset out of the storage.
        ///
        /// The request `[offset, offset+length)` range is truncated to prevent out-of-range memory access.
        /// The storage memory could be possibly scattered, but this is hidden from the user.
        ///
        /// @param offset_bytes The offset in bytes from the beginning of the storage.
        /// @param destination The pointer to the destination buffer. Should be at least `length_bytes` long.
        ///                    Could be `nullptr` if `length_bytes` is zero.
        /// @param length_bytes The number of bytes to copy.
        /// @return The number of bytes copied.
        ///
        /// NOSONAR is unavoidable: integration with low-level Lizard memory access.
        ///
        CETL_NODISCARD virtual std::size_t copy(const std::size_t offset_bytes,
                                                void* const       destination,  // NOSONAR
                                                const std::size_t length_bytes) const = 0;

    protected:
        IStorage()                               = default;
        IStorage(IStorage&&) noexcept            = default;
        IStorage& operator=(IStorage&&) noexcept = default;

    };  // IStorage

    /// @brief Default constructor of empty buffer with no storage attached.
    ///
    /// `copy()` method will do no operation, and returns zero (as `size()` does).
    ///
    ScatteredBuffer()
        : storage_{}
    {
    }

    // No copying, but move only!
    ScatteredBuffer(const ScatteredBuffer& other)            = delete;
    ScatteredBuffer& operator=(const ScatteredBuffer& other) = delete;

    /// @brief Moves other buffer into this new scattered buffer instance.
    ///
    /// The buffer is moved by moving the internal storage variant.
    ///
    /// @param other The other buffer to move into.
    ///
    ScatteredBuffer(ScatteredBuffer&& other) noexcept
        : storage_variant_(std::move(other.storage_variant_))
        , storage_{cetl::get_if<IStorage>(&storage_variant_)}
    {
        other.storage_ = nullptr;
    }

    /// @brief Constructs buffer by accepting a Lizard-specific implementation of `IStorage`
    ///        and moving it into the internal storage variant.
    ///
    /// @tparam AnyStorage The type of the storage implementation. Should fit into \ref StorageVariantFootprint.
    /// @param any_storage The storage to move into the buffer.
    ///
    template <typename AnyStorage, typename = std::enable_if_t<std::is_base_of<IStorage, AnyStorage>::value>>
    explicit ScatteredBuffer(AnyStorage&& any_storage) noexcept
        : storage_variant_(std::forward<AnyStorage>(any_storage))
        , storage_{cetl::get_if<IStorage>(&storage_variant_)}
    {
    }

    ~ScatteredBuffer()
    {
        reset();
    }

    /// @brief Assigns other scattered buffer by moving it into the this one.
    ///
    /// @param other The other buffer to move into.
    ///
    ScatteredBuffer& operator=(ScatteredBuffer&& other) noexcept
    {
        storage_variant_ = std::move(other.storage_variant_);

        other.storage_ = nullptr;
        storage_       = cetl::get_if<IStorage>(&storage_variant_);

        return *this;
    }

    /// @brief Resets the buffer by releasing its internal source.
    ///
    /// Has similar effect as if moved away. Has no effect if the buffer is moved away already.
    ///
    void reset() noexcept
    {
        storage_variant_.reset();
        storage_ = nullptr;
    }

    /// @brief Gets the number of bytes stored in the buffer (possibly scattered, but this is hidden from the user).
    ///
    /// @return Returns zero if the buffer is moved away.
    ///
    CETL_NODISCARD std::size_t size() const noexcept
    {
        return (storage_ != nullptr) ? storage_->size() : 0;
    }

    /// @brief Copies a fragment of the specified size at the specified offset out of the buffer.
    ///
    /// The request `[offset, offset+length)` range is truncated to prevent out-of-range memory access.
    /// Does nothing and returns zero if the instance has been moved away.
    ///
    /// @param offset_bytes The offset in bytes from the beginning of the buffer.
    /// @param destination The pointer to the destination buffer. Should be at least `length_bytes` long.
    ///                    Could be `nullptr` if `length_bytes` is zero.
    /// @param length_bytes The number of bytes to copy.
    /// @return The number of bytes copied.
    ///
    CETL_NODISCARD std::size_t copy(const std::size_t offset_bytes,
                                    void* const       destination,
                                    const std::size_t length_bytes) const
    {
        return (storage_ != nullptr) ? storage_->copy(offset_bytes, destination, length_bytes) : 0;
    }

private:
    cetl::unbounded_variant<StorageVariantFootprint, false, true> storage_variant_;
    const IStorage*                                               storage_;

};  // ScatteredBuffer

}  // namespace transport
}  // namespace libcyphal

#endif  // LIBCYPHAL_TRANSPORT_SCATTERED_BUFFER_HPP_INCLUDED
