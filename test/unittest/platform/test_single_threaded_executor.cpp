/// @copyright
/// Copyright (C) OpenCyphal Development Team  <opencyphal.org>
/// Copyright Amazon.com Inc. or its affiliates.
/// SPDX-License-Identifier: MIT

#include "../gtest_helpers.hpp" // NOLINT(misc-include-cleaner) `PrintTo`-s are implicitly in use by gtest.
#include "../tracking_memory_resource.hpp"

#include <libcyphal/platform/single_threaded_executor.hpp>
#include <libcyphal/types.hpp>

#include <gmock/gmock.h>
#include <gtest/gtest.h>

#include <chrono>

namespace
{

using Duration  = libcyphal::Duration;
using TimePoint = libcyphal::TimePoint;
using namespace libcyphal::platform;  // NOLINT This our main concern here in the unit tests.

using testing::Ge;
using testing::Le;
using testing::IsEmpty;

// https://github.com/llvm/llvm-project/issues/53444
// NOLINTBEGIN(misc-unused-using-decls, misc-include-cleaner)
using std::literals::chrono_literals::operator""us;
using std::literals::chrono_literals::operator""ns;
// NOLINTEND(misc-unused-using-decls, misc-include-cleaner)

class TestSingleThreadedExecutor : public testing::Test
{
protected:
    void TearDown() override
    {
        EXPECT_THAT(mr_.allocations, IsEmpty());
        EXPECT_THAT(mr_.total_allocated_bytes, mr_.total_deallocated_bytes);
    }

    // MARK: Data members:

    // NOLINTBEGIN
    TrackingMemoryResource mr_;
    // NOLINTEND
};

TEST_F(TestSingleThreadedExecutor, now)
{
    const auto expected = TimePoint{std::chrono::duration_cast<Duration>(  //
        std::chrono::steady_clock::now().time_since_epoch())};

    const SingleThreadedExecutor executor{mr_};
    const TimePoint              actual = executor.now();

    EXPECT_THAT(actual, Ge(expected));
    EXPECT_THAT(actual, Le(expected + 1us));
}

}  // namespace
