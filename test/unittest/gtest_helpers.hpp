/// @copyright
/// Copyright (C) OpenCyphal Development Team  <opencyphal.org>
/// Copyright Amazon.com Inc. or its affiliates.
/// SPDX-License-Identifier: MIT

#ifndef LIBCYPHAL_GTEST_HELPERS_HPP_INCLUDED
#define LIBCYPHAL_GTEST_HELPERS_HPP_INCLUDED

#include <libcyphal/transport/types.hpp>
#include <libcyphal/types.hpp>

#include <gmock/gmock.h>
#include <gtest/gtest-matchers.h>
#include <gtest/gtest-printers.h>

#include <chrono>
#include <cstddef>
#include <cstdint>
#include <iomanip>
#include <ios>
#include <ostream>

// NOLINTBEGIN(cppcoreguidelines-avoid-magic-numbers, readability-magic-numbers)

// MARK: - GTest Printers:

namespace libcyphal
{

inline void PrintTo(const Duration duration, std::ostream* os)
{
    auto locale = os->imbue(std::locale(""));
    *os << std::chrono::duration_cast<std::chrono::microseconds>(duration).count() << " us";
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
    switch (priority)
    {
    case Priority::Exceptional:
        *os << "Exceptional (0)";
        break;
    case Priority::Immediate:
        *os << "Immediate (1)";
        break;
    case Priority::Fast:
        *os << "Fast (2)";
        break;
    case Priority::High:
        *os << "High (3)";
        break;
    case Priority::Nominal:
        *os << "Nominal (4)";
        break;
    case Priority::Low:
        *os << "Low (5)";
        break;
    case Priority::Slow:
        *os << "Slow (6)";
        break;
    case Priority::Optional:
        *os << "Optional (7)";
        break;
    }
}

}  // namespace transport
}  // namespace libcyphal

// NOLINTEND(cppcoreguidelines-avoid-magic-numbers, readability-magic-numbers)

#endif  // LIBCYPHAL_GTEST_HELPERS_HPP_INCLUDED
