/// @copyright
/// Copyright (C) OpenCyphal Development Team  <opencyphal.org>
/// Copyright Amazon.com Inc. or its affiliates.
/// SPDX-License-Identifier: MIT

#ifndef LIBCYPHAL_TRANSPORT_CAN_MEDIA_HPP_INCLUDED
#define LIBCYPHAL_TRANSPORT_CAN_MEDIA_HPP_INCLUDED

#include "libcyphal/transport/errors.hpp"
#include "libcyphal/types.hpp"

#include <cetl/cetl.hpp>
#include <cetl/pf17/cetlpf.hpp>
#include <cetl/pf20/cetlpf.hpp>
#include <cetl/rtti.hpp>

#include <cstddef>
#include <cstdint>

namespace libcyphal
{
namespace transport
{
namespace can
{

using CanId = std::uint32_t;

struct Filter final
{
    CanId id{};
    CanId mask{};
};
using Filters = cetl::span<const Filter>;

struct RxMetadata final
{
    TimePoint   timestamp;
    CanId       can_id{};
    std::size_t payload_size{};
};

/// @brief Defines interface to a custom CAN bus media implementation.
///
/// Implementation is supposed to be provided by an user of the library.
///
class IMedia
{
public:
    IMedia(const IMedia&)                = delete;
    IMedia(IMedia&&) noexcept            = delete;
    IMedia& operator=(const IMedia&)     = delete;
    IMedia& operator=(IMedia&&) noexcept = delete;

    /// @brief Get the maximum transmission unit (MTU) of the CAN bus.
    ///
    /// This value may change arbitrarily at runtime. The transport implementation will query it before every
    /// transmission on the port. This value has no effect on the reception pipeline as it can accept arbitrary MTU.
    ///
    virtual std::size_t getMtu() const noexcept = 0;

    /// @brief Set the filters for the CAN bus.
    ///
    /// If there are fewer hardware filters available than requested, the configuration will be coalesced as described
    /// in the Cyphal/CAN Specification. If zero filters are requested, all incoming traffic will be rejected.
    /// While reconfiguration is in progress, incoming frames may be lost and/or unwanted frames may be received.
    /// The lifetime of the filter array may end upon return (no references retained).
    ///
    /// @return Returns `nullopt` on success; otherwise some `MediaError` in case of a low-level error.
    ///
    virtual cetl::optional<MediaError> setFilters(const Filters filters) noexcept = 0;

    /// @brief Schedules the frame for transmission asynchronously and return immediately.
    ///
    /// @return Returns `true` if accepted or already timed out; `false` to try again later.
    ///
    virtual Expected<bool, MediaError> push(const TimePoint                    deadline,
                                            const CanId                        can_id,
                                            const cetl::span<const cetl::byte> payload) noexcept = 0;

    /// @brief Takes the next payload fragment (aka CAN frame) from the reception queue unless it's empty.
    ///
    /// @param payload_buffer The payload of the frame will be written into the mutable `payload_buffer` (aka span).
    /// @return Description of a received fragment if available; otherwise an empty optional is returned immediately.
    ///         `nodiscard` is used to prevent ignoring the return value, which contains not only possible media error,
    ///         but also important metadata (like `payload_size` field for further parsing of the result payload).
    ///
    CETL_NODISCARD virtual Expected<cetl::optional<RxMetadata>, MediaError> pop(
        const cetl::span<cetl::byte> payload_buffer) noexcept = 0;

protected:
    IMedia()  = default;
    ~IMedia() = default;

};  // IMedia

}  // namespace can
}  // namespace transport
}  // namespace libcyphal

namespace cetl
{

// 87CA7D3F-70A3-4BB5-9979-E44A56B350C8
template <>
constexpr type_id type_id_getter<libcyphal::transport::can::IMedia*>() noexcept
{
    // NOLINTNEXTLINE(cppcoreguidelines-avoid-magic-numbers, readability-magic-numbers)
    return {0x87, 0xCA, 0x7D, 0x3F, 0x70, 0xA3, 0x4B, 0xB5, 0x99, 0x79, 0xE4, 0x4A, 0x56, 0xB3, 0x50, 0xC8};
}

}  // namespace cetl

#endif  // LIBCYPHAL_TRANSPORT_CAN_MEDIA_HPP_INCLUDED
