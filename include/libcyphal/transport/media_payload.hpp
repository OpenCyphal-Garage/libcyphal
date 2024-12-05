/// @copyright
/// Copyright (C) OpenCyphal Development Team  <opencyphal.org>
/// Copyright Amazon.com Inc. or its affiliates.
/// SPDX-License-Identifier: MIT

#ifndef LIBCYPHAL_TRANSPORT_MEDIA_PAYLOAD_HPP_INCLUDED
#define LIBCYPHAL_TRANSPORT_MEDIA_PAYLOAD_HPP_INCLUDED

#include <cetl/pf17/cetlpf.hpp>
#include <cetl/pf20/cetlpf.hpp>

#include <cstddef>
#include <tuple>
#include <utility>

namespace libcyphal
{
namespace transport
{

/// Defines a mutable media payload.
///
/// In use to pass payload data between the transport layer and its media.
/// It also manages memory ownership of the allocated payload buffer.
///
class MediaPayload final
{
public:
    /// Constructs a new empty payload.
    ///
    MediaPayload()
        : size_{0U}
        , data_{nullptr}
        , allocated_size_{0U}
        , mr_{nullptr}
    {
    }

    /// Constructs a new payload by owning the provided data buffer.
    ///
    /// @param size The size of the payload data in bytes. Must be less or equal to the allocated size.
    /// @param data The pointer to the payload data buffer.
    /// @param allocated_size The size of the allocated buffer. Must be greater or equal to the payload size.
    /// @param mr The PMR which was used to allocate the payload buffer. Will be used to deallocate it.
    ///
    MediaPayload(const std::size_t                 size,
                 cetl::byte* const                 data,
                 const std::size_t                 allocated_size,
                 cetl::pmr::memory_resource* const mr)
        : size_{size}
        , data_{data}
        , allocated_size_{allocated_size}
        , mr_{mr}
    {
        CETL_DEBUG_ASSERT(size_ <= allocated_size_, "");
        CETL_DEBUG_ASSERT((data_ == nullptr) || (mr_ != nullptr), "");
        CETL_DEBUG_ASSERT((data_ != nullptr) || ((size_ == 0) && (allocated_size_ == 0)), "");
    }

    /// Moves another payload into this new payload instance.
    ///
    /// @param other The other payload to move into.
    ///
    MediaPayload(MediaPayload&& other) noexcept
        : size_{std::exchange(other.size_, 0U)}
        , data_{std::exchange(other.data_, nullptr)}
        , allocated_size_{std::exchange(other.allocated_size_, 0U)}
        , mr_{std::exchange(other.mr_, nullptr)}
    {
        CETL_DEBUG_ASSERT(size_ <= allocated_size_, "");
        CETL_DEBUG_ASSERT((data_ == nullptr) || (mr_ != nullptr), "");
        CETL_DEBUG_ASSERT((data_ != nullptr) || ((size_ == 0) && (allocated_size_ == 0)), "");
    }

    /// @brief Assigns another payload by moving it into this one.
    ///
    /// @param other The other payload to move into.
    ///
    MediaPayload& operator=(MediaPayload&& other) noexcept
    {
        reset();

        size_           = std::exchange(other.size_, 0U);
        data_           = std::exchange(other.data_, nullptr);
        allocated_size_ = std::exchange(other.allocated_size_, 0U);
        mr_             = std::exchange(other.mr_, nullptr);

        return *this;
    }

    // No copying, but move only!
    MediaPayload(const MediaPayload& other)            = delete;
    MediaPayload& operator=(const MediaPayload& other) = delete;

    ~MediaPayload()
    {
        reset();
    }

    /// Gets the constant payload data as a span.
    ///
    /// Returns an empty (`{nullptr, 0}`) span if the payload is moved, released or reset.
    ///
    cetl::span<const cetl::byte> getSpan() const noexcept
    {
        return {data_, size_};
    }

    /// Gets size (in bytes) of allocated payload buffer.
    ///
    /// Returns zero if the payload is moved, released or reset.
    ///
    std::size_t getAllocatedSize() const noexcept
    {
        return allocated_size_;
    }

    /// Releases ownership of the payload by returning its data pointer and sizes.
    ///
    /// In use to return the payload to the lizard C API when media is not ready yet to accept the payload.
    /// After this call, corresponding internal fields will be nullified.
    ///
    /// @return Tuple with the payload size, pointer to the payload data, and the allocated size.
    ///         It's the caller's responsibility to deallocate the buffer with the correct memory resource,
    ///         or move it somewhere else with the same guarantee (like f.e. back to a lizard TX queue item).
    ///
    std::tuple<std::size_t, cetl::byte*, std::size_t> release() noexcept
    {
        mr_ = nullptr;
        return std::make_tuple(std::exchange(size_, 0U),
                               std::exchange(data_, nullptr),
                               std::exchange(allocated_size_, 0U));
    }

    /// Resets the payload by de-allocating its data buffer.
    ///
    /// Could be called multiple times.
    ///
    void reset() noexcept
    {
        if (data_ != nullptr)
        {
            CETL_DEBUG_ASSERT(mr_ != nullptr, "Memory resource should not be null.");

            // No Sonar `cpp:S5356` b/c we integrate here PMR.
            mr_->deallocate(data_, allocated_size_);  // NOSONAR cpp:S5356

            mr_             = nullptr;
            data_           = nullptr;
            size_           = 0;
            allocated_size_ = 0;
        }
    }

private:
    /// Size of the payload data in bytes.
    ///
    /// Could be less or equal to the allocated size.
    /// `0` when the payload is moved.
    ///
    std::size_t size_;

    /// Pointer to the payload buffer.
    ///
    /// `nullptr` when the payload is moved.
    ///
    cetl::byte* data_;

    /// Size of the allocated buffer.
    ///
    /// Could be greater or equal to the payload size.
    /// `0` when the payload is moved.
    ///
    std::size_t allocated_size_;

    /// Holds pointer to the PMR which was used to allocate the payload buffer. Will be used to deallocate it.
    ///
    cetl::pmr::memory_resource* mr_;

};  // MediaPayload

}  // namespace transport
}  // namespace libcyphal

#endif  // LIBCYPHAL_TRANSPORT_MEDIA_PAYLOAD_HPP_INCLUDED
