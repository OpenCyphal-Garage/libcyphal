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

#endif  // EXAMPLE_PLATFORM_POSIX_PLATFORM_ERROR_HPP_INCLUDED
