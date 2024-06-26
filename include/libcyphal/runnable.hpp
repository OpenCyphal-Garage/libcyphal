/// @copyright
/// Copyright (C) OpenCyphal Development Team  <opencyphal.org>
/// Copyright Amazon.com Inc. or its affiliates.
/// SPDX-License-Identifier: MIT

#ifndef LIBCYPHAL_RUNNABLE_HPP_INCLUDED
#define LIBCYPHAL_RUNNABLE_HPP_INCLUDED

#include "types.hpp"

#include <cetl/unbounded_variant.hpp>

#include <cstddef>

namespace libcyphal
{

class IRunnable
{
    static constexpr std::size_t MaxErrorSize = sizeof(void*) * 14;

public:
    /// @brief Defines possible error for the runnable entity.
    ///
    /// Empty unbounded_variant represents success (see `unbounded_variant::has_value`).
    ///
    using MaybeError = cetl::unbounded_variant<MaxErrorSize>;

    IRunnable(const IRunnable&)                = delete;
    IRunnable(IRunnable&&) noexcept            = delete;
    IRunnable& operator=(const IRunnable&)     = delete;
    IRunnable& operator=(IRunnable&&) noexcept = delete;

    virtual MaybeError run(const TimePoint now) = 0;

protected:
    IRunnable()  = default;
    ~IRunnable() = default;
};

}  // namespace libcyphal

#endif  // LIBCYPHAL_RUNNABLE_HPP_INCLUDED
