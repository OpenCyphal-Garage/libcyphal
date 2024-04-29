/// @copyright
/// Copyright (C) OpenCyphal Development Team  <opencyphal.org>
/// Copyright Amazon.com Inc. or its affiliates.
/// SPDX-License-Identifier: MIT

#ifndef LIBCYPHAL_TRANSPORT_SCATTERED_BUFFER_HPP_INCLUDED
#define LIBCYPHAL_TRANSPORT_SCATTERED_BUFFER_HPP_INCLUDED

#include <cetl/unbounded_variant.hpp>

#include <cstdint>

namespace libcyphal
{
namespace transport
{

/// The buffer is movable but not copyable because copying the contents of a buffer is considered wasteful.
/// The buffer behaves as if it's empty if the underlying implementation is moved away.
///
class ScatteredBuffer final
{
    // 91C1B109-F90E-45BE-95CF-6ED02AC3FFAA
    using InterfaceTypeIdType = cetl::
        type_id_type<0x91, 0xC1, 0xB1, 0x09, 0xF9, 0x0E, 0x45, 0xBE, 0x95, 0xCF, 0x6E, 0xD0, 0x2A, 0xC3, 0xFF, 0xAA>;

public:
    static constexpr std::size_t ImplementationFootprint = sizeof(void*) * 8;

    class Interface : public cetl::rtti_helper<InterfaceTypeIdType>
    {
    public:
        Interface(const Interface&)            = delete;
        Interface& operator=(const Interface&) = delete;

        CETL_NODISCARD virtual std::size_t size() const noexcept                      = 0;
        CETL_NODISCARD virtual std::size_t copy(const std::size_t offset_bytes,
                                                void* const       destination,
                                                const std::size_t length_bytes) const = 0;

    protected:
        Interface()                                = default;
        ~Interface() override                      = default;
        Interface(Interface&&) noexcept            = default;
        Interface& operator=(Interface&&) noexcept = default;

    };  // Interface

    ScatteredBuffer()                             = default;
    ScatteredBuffer(const ScatteredBuffer& other) = delete;
    ScatteredBuffer(ScatteredBuffer&& other) noexcept
    {
        storage_ = std::move(other.storage_);

        other.interface_ = nullptr;
        interface_       = cetl::get_if<Interface>(&storage_);
    }

    /// @brief Accepts a Lizard-specific implementation of `Interface` and moves it into the internal storage.
    ///
    template <typename T, typename = std::enable_if_t<std::is_base_of<Interface, T>::value>>
    explicit ScatteredBuffer(T&& source) noexcept
        : storage_(std::forward<T>(source))
        , interface_{cetl::get_if<Interface>(&storage_)}
    {
    }

    ~ScatteredBuffer()
    {
        reset();
    }

    ScatteredBuffer& operator=(const ScatteredBuffer& other) = delete;
    ScatteredBuffer& operator=(ScatteredBuffer&& other) noexcept
    {
        storage_ = std::move(other.storage_);

        other.interface_ = nullptr;
        interface_       = cetl::get_if<Interface>(&storage_);

        return *this;
    }

    void reset() noexcept
    {
        storage_.reset();
        interface_ = nullptr;
    }

    /// @brief Gets the number of bytes stored in the buffer (possibly scattered, but this is hidden from the user).
    ///
    /// @return Returns zero if the buffer is moved away.
    ///
    CETL_NODISCARD std::size_t size() const noexcept
    {
        return interface_ ? interface_->size() : 0;
    }

    /// @brief Copies a fragment of the specified size at the specified offset out of the buffer.
    ///
    /// The request is truncated to prevent out-of-range memory access.
    /// Returns the number of bytes copied.
    /// Does nothing and returns zero if the instance has been moved away.
    ///
    CETL_NODISCARD std::size_t copy(const std::size_t offset_bytes,
                                    void* const       destination,
                                    const std::size_t length_bytes) const
    {
        return interface_ ? interface_->copy(offset_bytes, destination, length_bytes) : 0;
    }

private:
    cetl::unbounded_variant<ImplementationFootprint, false, true> storage_;
    const Interface*                                              interface_ = nullptr;

};  // ScatteredBuffer

}  // namespace transport
}  // namespace libcyphal

#endif  // LIBCYPHAL_TRANSPORT_SCATTERED_BUFFER_HPP_INCLUDED
