/// @copyright
/// Copyright (C) OpenCyphal Development Team  <opencyphal.org>
/// Copyright Amazon.com Inc. or its affiliates.
/// SPDX-License-Identifier: MIT

#ifndef LIBCYPHAL_TRANSPORT_UDP_MEDIA_MOCK_HPP_INCLUDED
#define LIBCYPHAL_TRANSPORT_UDP_MEDIA_MOCK_HPP_INCLUDED

#include <libcyphal/transport/udp/media.hpp>

#include <gmock/gmock.h>

namespace libcyphal
{
namespace transport
{
namespace udp
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

};  // MediaMock

}  // namespace udp
}  // namespace transport
}  // namespace libcyphal

#endif  // LIBCYPHAL_TRANSPORT_UDP_MEDIA_MOCK_HPP_INCLUDED
