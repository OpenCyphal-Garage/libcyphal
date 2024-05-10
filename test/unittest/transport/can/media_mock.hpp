/// @copyright
/// Copyright (C) OpenCyphal Development Team  <opencyphal.org>
/// Copyright Amazon.com Inc. or its affiliates.
/// SPDX-License-Identifier: MIT

#ifndef LIBCYPHAL_TRANSPORT_CAN_MEDIA_MOCK_HPP_INCLUDED
#define LIBCYPHAL_TRANSPORT_CAN_MEDIA_MOCK_HPP_INCLUDED

#include <cetl/pf17/cetlpf.hpp>
#include <cetl/pf20/cetlpf.hpp>
#include <libcyphal/transport/can/media.hpp>
#include <libcyphal/transport/errors.hpp>

#include <gmock/gmock.h>

#include <cstddef>

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
    MOCK_METHOD(std::size_t, getMtu, (), (const, noexcept, override));
    MOCK_METHOD(cetl::optional<MediaError>, setFilters, (const Filters filters), (noexcept, override));
    MOCK_METHOD((Expected<bool, MediaError>),
                push,
                (const TimePoint deadline, const CanId can_id, const cetl::span<const cetl::byte> payload),
                (noexcept, override));
    MOCK_METHOD((Expected<cetl::optional<RxMetadata>, MediaError>),
                pop,
                (const cetl::span<cetl::byte> payload_buffer),
                (noexcept, override));

};  // MediaMock

}  // namespace can
}  // namespace transport
}  // namespace libcyphal

#endif  // LIBCYPHAL_TRANSPORT_CAN_MEDIA_MOCK_HPP_INCLUDED
