/// @copyright
/// Copyright (C) OpenCyphal Development Team  <opencyphal.org>
/// Copyright Amazon.com Inc. or its affiliates.
/// SPDX-License-Identifier: MIT

#ifndef LIBCYPHAL_PLATFORM_STORAGE_KEY_VALUE_MOCK_HPP_INCLUDED
#define LIBCYPHAL_PLATFORM_STORAGE_KEY_VALUE_MOCK_HPP_INCLUDED

#include <cetl/pf17/cetlpf.hpp>
#include <libcyphal/platform/storage.hpp>

#include <gmock/gmock.h>

#include <cstddef>
#include <cstdint>

namespace libcyphal
{
namespace platform
{
namespace storage
{

class KeyValueMock : public IKeyValue
{
public:
    KeyValueMock()          = default;
    virtual ~KeyValueMock() = default;

    KeyValueMock(const KeyValueMock&)                = delete;
    KeyValueMock(KeyValueMock&&) noexcept            = delete;
    KeyValueMock& operator=(const KeyValueMock&)     = delete;
    KeyValueMock& operator=(KeyValueMock&&) noexcept = delete;

    // MARK: IKeyValue

    MOCK_METHOD((Expected<std::size_t, Error>),
                get,
                (const cetl::string_view key, const cetl::span<std::uint8_t> data),
                (const, override));
    MOCK_METHOD(cetl::optional<Error>,
                put,
                (const cetl::string_view key, const cetl::span<const std::uint8_t> data),
                (override));
    MOCK_METHOD(cetl::optional<Error>, drop, (const cetl::string_view key), (override));

};  // KeyValueMock

}  // namespace storage
}  // namespace platform
}  // namespace libcyphal

#endif  // LIBCYPHAL_PLATFORM_STORAGE_KEY_VALUE_MOCK_HPP_INCLUDED
