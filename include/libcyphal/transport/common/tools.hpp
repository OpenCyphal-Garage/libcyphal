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

/// Internal implementation details of the Transport layer.
/// Not supposed to be used directly by the users of the library.
///
namespace detail
{

/// @brief Converts a compatible (aka subset) failure variant to transport's `AnyFailure` one.
///
template <typename FailureVar>
CETL_NODISCARD static AnyFailure anyFailureFromVariant(FailureVar&& failure_var)
{
    return cetl::visit([](auto&& failure) -> AnyFailure { return std::forward<decltype(failure)>(failure); },
                       std::forward<FailureVar>(failure_var));
}

}  // namespace detail
}  // namespace common
}  // namespace transport
}  // namespace libcyphal

#endif  // LIBCYPHAL_TRANSPORT_COMMON_TOOLS_HPP_INCLUDED
