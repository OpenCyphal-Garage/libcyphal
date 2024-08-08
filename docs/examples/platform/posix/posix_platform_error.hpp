/// @copyright
/// Copyright (C) OpenCyphal Development Team  <opencyphal.org>
/// Copyright Amazon.com Inc. or its affiliates.
/// SPDX-License-Identifier: MIT
///

#ifndef EXAMPLE_PLATFORM_POSIX_PLATFORM_ERROR_HPP_INCLUDED
#define EXAMPLE_PLATFORM_POSIX_PLATFORM_ERROR_HPP_INCLUDED

#include <cetl/cetl.hpp>
#include <libcyphal/transport/errors.hpp>

#include <cstdint>

namespace example
{
namespace platform
{
namespace posix
{

class PosixPlatformError final : public libcyphal::transport::IPlatformError
{
public:
    explicit PosixPlatformError(const int err)
        : code_{err}
    {
        CETL_DEBUG_ASSERT(err > 0, "");
    }
    virtual ~PosixPlatformError() noexcept                       = default;
    PosixPlatformError(const PosixPlatformError&)                = default;
    PosixPlatformError(PosixPlatformError&&) noexcept            = default;
    PosixPlatformError& operator=(const PosixPlatformError&)     = default;
    PosixPlatformError& operator=(PosixPlatformError&&) noexcept = default;

    // MARK: IPlatformError

    std::uint32_t code() const noexcept override
    {
        return static_cast<std::uint32_t>(code_);
    }

private:
    int code_;

};  // PosixPlatformError

}  // namespace posix
}  // namespace platform
}  // namespace example

namespace cetl
{

// Just random id: 5D29D78C-3785-43A2-8D15-7FAF2C3881BC
template <>
constexpr type_id type_id_getter<example::platform::posix::PosixPlatformError>() noexcept
{
    // NOLINTNEXTLINE(cppcoreguidelines-avoid-magic-numbers, readability-magic-numbers)
    return {0x5D, 0x29, 0xD7, 0x8C, 0x37, 0x85, 0x43, 0xA2, 0x8D, 0x15, 0x7F, 0xAF, 0x2C, 0x38, 0x81, 0xBC};
}

}  // namespace cetl

#endif  // EXAMPLE_PLATFORM_POSIX_PLATFORM_ERROR_HPP_INCLUDED
