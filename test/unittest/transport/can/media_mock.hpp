/// @copyright
/// Copyright (C) OpenCyphal Development Team  <opencyphal.org>
/// Copyright Amazon.com Inc. or its affiliates.
/// SPDX-License-Identifier: MIT

#ifndef LIBCYPHAL_TRANSPORT_CAN_MEDIA_MOCK_HPP_INCLUDED
#define LIBCYPHAL_TRANSPORT_CAN_MEDIA_MOCK_HPP_INCLUDED

#include <cetl/pf17/cetlpf.hpp>
#include <cetl/pf20/cetlpf.hpp>
#include <libcyphal/executor.hpp>
#include <libcyphal/transport/can/media.hpp>
#include <libcyphal/transport/errors.hpp>
#include <libcyphal/types.hpp>

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

    MOCK_METHOD(cetl::optional<MediaFailure>, setFilters, (const Filters filters), (noexcept, override));

    MOCK_METHOD(PushResult::Type,
                push,
                (const TimePoint deadline, const CanId can_id, const cetl::span<const cetl::byte> payload),
                (noexcept, override));

    MOCK_METHOD(PopResult::Type, pop, (const cetl::span<cetl::byte> payload_buffer), (noexcept, override));

    MOCK_METHOD(IExecutor::Callback::Any,
                registerPushCallback,
                (IExecutor::Callback::Function && function),
                (override));

    MOCK_METHOD(IExecutor::Callback::Any,  //
                registerPopCallback,
                (IExecutor::Callback::Function && function),
                (override));

    MOCK_METHOD(cetl::pmr::memory_resource&, getTxMemoryResource, (), (override));

};  // MediaMock

}  // namespace can
}  // namespace transport
}  // namespace libcyphal

#endif  // LIBCYPHAL_TRANSPORT_CAN_MEDIA_MOCK_HPP_INCLUDED
