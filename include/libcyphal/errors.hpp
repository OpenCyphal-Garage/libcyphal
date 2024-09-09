/// @copyright
/// Copyright (C) OpenCyphal Development Team  <opencyphal.org>
/// Copyright Amazon.com Inc. or its affiliates.
/// SPDX-License-Identifier: MIT

#ifndef LIBCYPHAL_ERRORS_HPP_INCLUDED
#define LIBCYPHAL_ERRORS_HPP_INCLUDED

namespace libcyphal
{

/// @brief Defines a generic error that is issued when a memory allocation fails.
///
struct MemoryError final
{};

/// @brief Defines a generic error that is issued when an argument is invalid.
///
struct ArgumentError final
{};

}  // namespace libcyphal

#endif  // LIBCYPHAL_ERRORS_HPP_INCLUDED
