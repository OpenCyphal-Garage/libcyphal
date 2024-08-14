/// @copyright
/// Copyright (C) OpenCyphal Development Team  <opencyphal.org>
/// Copyright Amazon.com Inc. or its affiliates.
/// SPDX-License-Identifier: MIT

#ifndef LIBCYPHAL_TIME_PROVIDER_HPP_INCLUDED
#define LIBCYPHAL_TIME_PROVIDER_HPP_INCLUDED

#include "types.hpp"

namespace libcyphal
{

/// @brief Defines abstract interface of a time provider.
///
class ITimeProvider
{
public:
    ITimeProvider(const ITimeProvider&)                = delete;
    ITimeProvider(ITimeProvider&&) noexcept            = delete;
    ITimeProvider& operator=(const ITimeProvider&)     = delete;
    ITimeProvider& operator=(ITimeProvider&&) noexcept = delete;

    /// @brief Gets the current time point (aka now).
    ///
    virtual TimePoint now() const noexcept = 0;

protected:
    ITimeProvider()  = default;
    ~ITimeProvider() = default;

};  // ITimeProvider

}  // namespace libcyphal

#endif  // LIBCYPHAL_TIME_PROVIDER_HPP_INCLUDED
