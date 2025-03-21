/// @copyright
/// Copyright (C) OpenCyphal Development Team  <opencyphal.org>
/// Copyright Amazon.com Inc. or its affiliates.
/// SPDX-License-Identifier: MIT

#ifndef LIBCYPHAL_TRANSPORT_SCATTERED_BUFFER_HPP_INCLUDED
#define LIBCYPHAL_TRANSPORT_SCATTERED_BUFFER_HPP_INCLUDED

#include "libcyphal/config.hpp"
#include "types.hpp"

#include <cetl/pf17/cetlpf.hpp>
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
public:
    /// @brief Defines maximum size (aka footprint) of the storage variant.
    ///
    static constexpr std::size_t StorageVariantFootprint = config::Transport::ScatteredBuffer_StorageVariantFootprint();

    /// @brief Defines interface for observing internal fragments of the scattered buffer.
    ///
    class IFragmentsObserver
    {
    public:
        IFragmentsObserver(const IFragmentsObserver&)                = delete;
        IFragmentsObserver& operator=(const IFragmentsObserver&)     = delete;
        IFragmentsObserver& operator=(IFragmentsObserver&&) noexcept = delete;
        IFragmentsObserver(IFragmentsObserver&&) noexcept            = delete;

        /// @brief Notifies the observer about the next fragment of the scattered buffer.
        ///
        virtual void onNext(const PayloadFragment fragment) = 0;

    protected:
        IFragmentsObserver()  = default;
        ~IFragmentsObserver() = default;

    };  // IFragmentsObserver

    /// @brief Defines storage interface for the scattered buffer.
    ///
    /// @see ScatteredBuffer::ScatteredBuffer(AnyStorage&& any_storage)
    ///
    class IStorage : public cetl::rtti::rtti
    {
        // 91C1B109-F90E-45BE-95CF-6ED02AC3FFAA
        // clang-format off
        using TypeIdType = cetl::type_id_type<
            // NOLINTNEXTLINE(cppcoreguidelines-avoid-magic-numbers, readability-magic-numbers)
            0x91, 0xC1, 0xB1, 0x09, 0xF9, 0x0E, 0x45, 0xBE, 0x95, 0xCF, 0x6E, 0xD0, 0x2A, 0xC3, 0xFF, 0xAA>;
        // clang-format on

    public:
        // No copying, but move only!
        IStorage(const IStorage&)            = delete;
        IStorage& operator=(const IStorage&) = delete;

        /// @brief Gets the total number of bytes stored in the buffer.
        ///
        /// The storage could be possibly scattered, but this is hidden from the user.
        ///
        virtual std::size_t size() const noexcept = 0;

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
        virtual std::size_t copy(const std::size_t offset_bytes,
                                 cetl::byte* const destination,
                                 const std::size_t length_bytes) const = 0;

        /// @brief Reports the internal fragments of the storage to the specified observer.
        ///
        /// @param observer The observer will be called (by `onNext` method) for each fragment of the storage.
        ///
        virtual void observeFragments(IFragmentsObserver& observer) const = 0;

        // MARK: RTTI

        static constexpr cetl::type_id _get_type_id_() noexcept
        {
            return cetl::type_id_type_value<TypeIdType>();
        }

        // No Sonar `cpp:S5008` and `cpp:S5356` b/c they are unavoidable - RTTI integration.
        CETL_NODISCARD void* _cast_(const cetl::type_id& id) & noexcept override  // NOSONAR cpp:S5008
        {
            return (id == _get_type_id_()) ? this : nullptr;  // NOSONAR cpp:S5356
        }

        // No Sonar `cpp:S5008` and `cpp:S5356` b/c they are unavoidable - RTTI integration.
        CETL_NODISCARD const void* _cast_(const cetl::type_id& id) const& noexcept override  // NOSONAR cpp:S5008
        {
            return (id == _get_type_id_()) ? this : nullptr;  // NOSONAR cpp:S5356
        }

    protected:
        IStorage()                               = default;
        ~IStorage()                              = default;
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
    std::size_t size() const noexcept
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
    /// NOSONAR cpp:S5008 below currently unavoidable. Could be fixed if nunavut provides `cetl::byte*` support.
    ///
    std::size_t copy(const std::size_t offset_bytes,
                     void* const       destination,  // NOSONAR : cpp:S5008
                     const std::size_t length_bytes) const
    {
        if (storage_ == nullptr)
        {
            return 0;
        }

        return storage_->copy(offset_bytes, static_cast<cetl::byte*>(destination), length_bytes);
    }

    /// @brief Reports the internal fragments of the buffer to the specified observer.
    ///
    /// @param observer The observer will be called (by `onNext` method) for each fragment of the buffer.
    ///
    void observeFragments(IFragmentsObserver& observer) const
    {
        if (const auto* const storage = storage_)
        {
            storage->observeFragments(observer);
        }
    }

private:
    cetl::unbounded_variant<StorageVariantFootprint, false, true> storage_variant_;
    const IStorage*                                               storage_;

};  // ScatteredBuffer

}  // namespace transport
}  // namespace libcyphal

#endif  // LIBCYPHAL_TRANSPORT_SCATTERED_BUFFER_HPP_INCLUDED
