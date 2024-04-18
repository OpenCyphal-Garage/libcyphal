/// @file
/// libcyphal common header.
///
/// @copyright
/// Copyright (C) OpenCyphal Development Team  <opencyphal.org>
/// Copyright Amazon.com Inc. or its affiliates.
/// SPDX-License-Identifier: MIT

#ifndef LIBCYPHAL_GTEST_HELPERS_HPP
#define LIBCYPHAL_GTEST_HELPERS_HPP

#include <libcyphal/types.hpp>
#include <libcyphal/transport/defines.hpp>

#include <ostream>

namespace libcyphal
{

inline void PrintTo(const Duration duration, std::ostream* os)
{
    auto locale = os->imbue(std::locale("en_US"));
    *os << std::chrono::duration_cast<std::chrono::microseconds>(duration).count() << "_us";
    os->imbue(locale);
}

inline void PrintTo(const TimePoint time_point, std::ostream* os)
{
    PrintTo(time_point.time_since_epoch(), os);
}

namespace transport
{

inline void PrintTo(const Priority priority, std::ostream* os)
{
    static constexpr std::array<const char*, 8> names{
        "Exceptional (0)",
        "Immediate (1)",
        "Fast (2)",
        "High (3)",
        "Nominal (4)",
        "Low (5)",
        "Slow (6)",
        "Optional  (7)",
    };
    *os << names[static_cast<std::size_t>(priority)];
}

}  // namespace transport
}  // namespace libcyphal

#endif  // LIBCYPHAL_GTEST_HELPERS_HPP
