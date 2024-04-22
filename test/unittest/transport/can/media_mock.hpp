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
