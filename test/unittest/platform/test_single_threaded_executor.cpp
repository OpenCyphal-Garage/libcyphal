/// @copyright
/// Copyright (C) OpenCyphal Development Team  <opencyphal.org>
/// Copyright Amazon.com Inc. or its affiliates.
/// SPDX-License-Identifier: MIT

#include "../gtest_helpers.hpp"  // NOLINT(misc-include-cleaner) `PrintTo`-s are implicitly in use by gtest.
#include "../memory_resource_mock.hpp"
#include "../tracking_memory_resource.hpp"

#include <libcyphal/executor.hpp>
#include <libcyphal/platform/single_threaded_executor.hpp>
#include <libcyphal/types.hpp>

#include <gmock/gmock.h>
#include <gtest/gtest.h>

#include <chrono>
#include <thread>
#include <utility>

namespace
{

using Duration  = libcyphal::Duration;
using TimePoint = libcyphal::TimePoint;
using namespace libcyphal::platform;  // NOLINT This our main concern here in the unit tests.

using testing::_;
using testing::Ge;
using testing::Le;
using testing::AllOf;
using testing::Return;
using testing::IsEmpty;
using testing::StrictMock;

// https://github.com/llvm/llvm-project/issues/53444
// NOLINTBEGIN(misc-unused-using-decls, misc-include-cleaner)
using std::literals::chrono_literals::operator""ms;
using std::literals::chrono_literals::operator""us;
// NOLINTEND(misc-unused-using-decls, misc-include-cleaner)

// NOLINTBEGIN(cppcoreguidelines-avoid-magic-numbers, readability-magic-numbers)

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

// MARK: - Tests:

TEST_F(TestSingleThreadedExecutor, now)
{
    auto expected = TimePoint{std::chrono::duration_cast<Duration>(  //
        std::chrono::steady_clock::now().time_since_epoch())};

    const auto epsilon = 10us;

    const SingleThreadedExecutor executor{mr_};
    TimePoint                    actual = executor.now();

    EXPECT_THAT(actual, AllOf(Ge(expected), Le(expected + epsilon)));

    std::this_thread::sleep_for(20ms);

    expected = TimePoint{std::chrono::duration_cast<Duration>(  //
        std::chrono::steady_clock::now().time_since_epoch())};

    actual = executor.now();

    EXPECT_THAT(actual, AllOf(Ge(expected), Le(expected + epsilon)));
}

TEST_F(TestSingleThreadedExecutor, registerCallback)
{
    SingleThreadedExecutor executor{mr_};

    auto nop = [](const TimePoint) { return; };

    libcyphal::IExecutor::Callback::Handle handle1;
    EXPECT_FALSE(handle1);

    handle1 = executor.registerCallback(nop);
    EXPECT_TRUE(handle1);

    auto handle2a{executor.registerCallback(nop)};
    EXPECT_TRUE(handle2a);

    auto handle2b{std::move(handle2a)};
    EXPECT_TRUE(handle2b);

    handle2b = executor.registerCallback(nop);
    EXPECT_TRUE(handle2b);

    handle1 = {};
    EXPECT_FALSE(handle1);

    handle2b = std::move(handle1);
    EXPECT_FALSE(handle2b);
}

TEST_F(TestSingleThreadedExecutor, registerCallback_no_memory)
{
    StrictMock<MemoryResourceMock> mr_mock{};
    //mr_mock.redirectExpectedCallsTo(mr_);

    EXPECT_CALL(mr_mock, do_allocate(_, _)).WillOnce(Return(nullptr));

    SingleThreadedExecutor executor{mr_mock};

    auto handle = executor.registerCallback([](const TimePoint) { return; });
    EXPECT_FALSE(handle);
    handle.reset();
}

// NOLINTEND(cppcoreguidelines-avoid-magic-numbers, readability-magic-numbers)

}  // namespace
