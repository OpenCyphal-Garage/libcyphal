/// @copyright
/// Copyright (C) OpenCyphal Development Team  <opencyphal.org>
/// Copyright Amazon.com Inc. or its affiliates.
/// SPDX-License-Identifier: MIT

#ifndef LIBCYPHAL_TRANSPORT_COMMON_TOOLS_HPP_INCLUDED
#define LIBCYPHAL_TRANSPORT_COMMON_TOOLS_HPP_INCLUDED

#include "libcyphal/transport/errors.hpp"

#include <cetl/cetl.hpp>
#include <cetl/pf17/cetlpf.hpp>

namespace libcyphal
{
namespace transport
{
namespace common
{
namespace detail
{

/// @brief Converts a compatible (aka subset) failure variant to transport's `AnyFailure` one.
///
template <typename Failure>
CETL_NODISCARD static AnyFailure anyFailureFromVariant(Failure&& failure)
{
    return cetl::visit([](auto&& failure) -> AnyFailure { return std::forward<decltype(failure)>(failure); },
                       std::forward<Failure>(failure));
}

}  // namespace detail
}  // namespace common
}  // namespace transport
}  // namespace libcyphal

#endif  // LIBCYPHAL_TRANSPORT_COMMON_TOOLS_HPP_INCLUDED
