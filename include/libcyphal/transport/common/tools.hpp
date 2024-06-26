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

/// @brief Converts a compatible (aka subset) error variant to transport's `AnyError` one.
///
template <typename ErrorVariant>
CETL_NODISCARD static AnyError anyErrorFromVariant(ErrorVariant&& error_var)
{
    return cetl::visit([](auto&& error) -> AnyError { return std::forward<decltype(error)>(error); },
                       std::forward<ErrorVariant>(error_var));
}

}  // namespace detail
}  // namespace common
}  // namespace transport
}  // namespace libcyphal

#endif  // LIBCYPHAL_TRANSPORT_COMMON_TOOLS_HPP_INCLUDED
