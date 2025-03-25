/// @copyright
/// Copyright (C) OpenCyphal Development Team  <opencyphal.org>
/// Copyright Amazon.com Inc. or its affiliates.
/// SPDX-License-Identifier: MIT

#ifndef LIBCYPHAL_TRANSPORT_MEDIA_PAYLOAD_HPP_INCLUDED
#define LIBCYPHAL_TRANSPORT_MEDIA_PAYLOAD_HPP_INCLUDED

#include "types.hpp"

#include <cetl/pf17/cetlpf.hpp>
#include <cetl/pf20/cetlpf.hpp>

#include <cstddef>
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
    /// Structure with the payload size, pointer to the payload data, and the allocated size.
    ///
    /// NB! This structure (in contrast to the parent `MediaPayload` type) is intended for raw (unmanaged) and
    /// explicit transfer of payload ownership out of the `MediaPayload` instance (see `release` method).
    /// It's the caller's responsibility to deallocate the buffer with the correct memory resource,
    /// or move it somewhere else with the same guarantee (like f.e. back to a lizard TX queue item).
    /// If you just need to access the payload data (without owning it), use `getSpan` method instead.
    ///
    struct Ownership
    {
        /// Size of the payload data in bytes.
        ///
        /// Could be less or equal to the allocated size.
        /// `0` when the payload is moved.
        ///
        std::size_t size;

        /// Pointer to the payload buffer.
        ///
        /// `nullptr` when the payload is moved.
        ///
        cetl::byte* data;

        /// Size of the allocated buffer.
        ///
        /// Could be greater or equal to the payload size.
        /// `0` when the payload is moved.
        ///
        std::size_t allocated_size;

    };  // Ownership

    /// Constructs a new empty payload.
    ///
    MediaPayload()
        : ownership_{0U, nullptr, 0U}
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
        : ownership_{size, data, allocated_size}
        , mr_{mr}
    {
        CETL_DEBUG_ASSERT(size <= allocated_size, "");
        CETL_DEBUG_ASSERT((data == nullptr) || (mr_ != nullptr), "");
        CETL_DEBUG_ASSERT((data != nullptr) || ((size == 0) && (allocated_size == 0)), "");
    }

    /// Moves another payload into this new payload instance.
    ///
    /// @param other The other payload to move into.
    ///
    MediaPayload(MediaPayload&& other) noexcept
        : ownership_{std::exchange(other.ownership_, {0U, nullptr, 0U})}
        , mr_{std::exchange(other.mr_, nullptr)}
    {
        CETL_DEBUG_ASSERT(ownership_.size <= ownership_.allocated_size, "");
        CETL_DEBUG_ASSERT((ownership_.data == nullptr) || (mr_ != nullptr), "");
        CETL_DEBUG_ASSERT((ownership_.data != nullptr) || ((ownership_.size == 0) && (ownership_.allocated_size == 0)),
                          "");
    }

    /// @brief Assigns another payload by moving it into this one.
    ///
    /// @param other The other payload to move into.
    ///
    MediaPayload& operator=(MediaPayload&& other) noexcept
    {
        reset();

        ownership_ = std::exchange(other.ownership_, {0U, nullptr, 0U});
        mr_        = std::exchange(other.mr_, nullptr);

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
    PayloadFragment getSpan() const noexcept
    {
        return {ownership_.data, ownership_.size};
    }

    /// Gets size (in bytes) of allocated payload buffer.
    ///
    /// Returns zero if the payload is moved, released or reset.
    ///
    std::size_t getAllocatedSize() const noexcept
    {
        return ownership_.allocated_size;
    }

    /// Releases ownership of the payload by returning its data pointer and sizes.
    ///
    /// In use to return the payload to the lizard C API when media is not ready yet to accept the payload.
    /// After this call, corresponding internal fields will be nullified.
    /// If you just need to access the payload data (without owning it), use `getSpan` method instead.
    ///
    /// @return Tuple with the payload size, pointer to the payload data, and the allocated size.
    ///         It's the caller's responsibility to deallocate the buffer with the correct memory resource,
    ///         or move it somewhere else with the same guarantee (like f.e. back to a lizard TX queue item).
    ///
    Ownership release() noexcept
    {
        mr_ = nullptr;
        return std::exchange(ownership_, {0U, nullptr, 0U});
    }

    /// Resets the payload by de-allocating its data buffer.
    ///
    /// Could be called multiple times.
    ///
    void reset() noexcept
    {
        if (ownership_.data != nullptr)
        {
            CETL_DEBUG_ASSERT(mr_ != nullptr, "Memory resource should not be null.");

            // No Sonar `cpp:S5356` b/c we integrate here PMR.
            mr_->deallocate(ownership_.data, ownership_.allocated_size);  // NOSONAR cpp:S5356

            mr_        = nullptr;
            ownership_ = {0U, nullptr, 0U};
        }
    }

private:
    Ownership ownership_;

    /// Holds pointer to the PMR which was used to allocate the payload buffer. Will be used to deallocate it.
    ///
    cetl::pmr::memory_resource* mr_;

};  // MediaPayload

}  // namespace transport
}  // namespace libcyphal

#endif  // LIBCYPHAL_TRANSPORT_MEDIA_PAYLOAD_HPP_INCLUDED
