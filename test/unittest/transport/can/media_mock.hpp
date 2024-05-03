/// @copyright
/// Copyright (C) OpenCyphal Development Team  <opencyphal.org>
/// Copyright Amazon.com Inc. or its affiliates.
/// SPDX-License-Identifier: MIT

#ifndef LIBCYPHAL_TRANSPORT_CAN_MEDIA_MOCK_HPP_INCLUDED
#define LIBCYPHAL_TRANSPORT_CAN_MEDIA_MOCK_HPP_INCLUDED

#include <libcyphal/transport/can/media.hpp>

#include <gmock/gmock.h>

namespace libcyphal
{
namespace transport
{
namespace can
{

class MediaMock : public IMedia
{
public:
    MediaMock()          = default;
    virtual ~MediaMock() = default;

    MediaMock(const MediaMock&)                = delete;
    MediaMock(MediaMock&&) noexcept            = delete;
    MediaMock& operator=(const MediaMock&)     = delete;
    MediaMock& operator=(MediaMock&&) noexcept = delete;

    // NOLINTNEXTLINE(bugprone-exception-escape)
    MOCK_METHOD(std::size_t, getMtu, (), (const, noexcept, final));
    MOCK_METHOD(cetl::optional<MediaError>, setFilters, (const Filters filters), (noexcept, final));
    MOCK_METHOD((Expected<bool, MediaError>),
                push,
                (const TimePoint deadline, const CanId can_id, const cetl::span<const cetl::byte> payload),
                (noexcept, final));
    MOCK_METHOD((Expected<cetl::optional<RxMetadata>, MediaError>),
                pop,
                (const cetl::span<cetl::byte> payload_buffer),
                (noexcept, final));

};  // MediaMock

}  // namespace can
}  // namespace transport
}  // namespace libcyphal

#endif  // LIBCYPHAL_TRANSPORT_CAN_MEDIA_MOCK_HPP_INCLUDED
