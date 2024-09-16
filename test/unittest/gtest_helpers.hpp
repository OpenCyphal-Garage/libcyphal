/// @copyright
/// Copyright (C) OpenCyphal Development Team  <opencyphal.org>
/// Copyright Amazon.com Inc. or its affiliates.
/// SPDX-License-Identifier: MIT

#ifndef LIBCYPHAL_GTEST_HELPERS_HPP_INCLUDED
#define LIBCYPHAL_GTEST_HELPERS_HPP_INCLUDED

#include <libcyphal/types.hpp>

#include <chrono>
#include <ostream>

// NOLINTBEGIN(cppcoreguidelines-avoid-magic-numbers, readability-magic-numbers)

// MARK: - GTest Printers:

namespace libcyphal
{

inline void PrintTo(const Duration duration, std::ostream* os)
{
    *os << std::chrono::duration_cast<std::chrono::microseconds>(duration).count() << "us";
}

inline void PrintTo(const TimePoint time_point, std::ostream* os)
{
    PrintTo(time_point.time_since_epoch(), os);
}

}  // namespace libcyphal

// NOLINTEND(cppcoreguidelines-avoid-magic-numbers, readability-magic-numbers)

#endif  // LIBCYPHAL_GTEST_HELPERS_HPP_INCLUDED
