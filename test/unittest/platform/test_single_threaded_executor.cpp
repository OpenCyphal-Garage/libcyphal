/// @copyright
/// Copyright (C) OpenCyphal Development Team  <opencyphal.org>
/// Copyright Amazon.com Inc. or its affiliates.
/// SPDX-License-Identifier: MIT

#include "../gtest_helpers.hpp"  // NOLINT(misc-include-cleaner) `PrintTo`-s are implicitly in use by gtest.
#include "../memory_resource_mock.hpp"
#include "../tracking_memory_resource.hpp"

#include <cetl/pf17/cetlpf.hpp>
#include <libcyphal/executor.hpp>
#include <libcyphal/platform/single_threaded_executor.hpp>
#include <libcyphal/types.hpp>

#include <gmock/gmock.h>
#include <gtest/gtest.h>

#include <chrono>
#include <string>
#include <thread>
#include <tuple>
#include <utility>
#include <vector>

namespace
{

using Duration  = libcyphal::Duration;
using TimePoint = libcyphal::TimePoint;
using Callback  = libcyphal::IExecutor::Callback;
using Schedule  = libcyphal::IExecutor::Callback::Schedule;
using namespace libcyphal::platform;  // NOLINT This our main concern here in the unit tests.

using testing::_;
using testing::Eq;
using testing::Ge;
using testing::Le;
using testing::AllOf;
using testing::Return;
using testing::IsEmpty;
using testing::Optional;
using testing::StrictMock;
using testing::ElementsAre;

// https://github.com/llvm/llvm-project/issues/53444
// NOLINTBEGIN(misc-unused-using-decls, misc-include-cleaner)
using std::literals::chrono_literals::operator""ms;
using std::literals::chrono_literals::operator""us;
// NOLINTEND(misc-unused-using-decls, misc-include-cleaner)

// NOLINTBEGIN(cppcoreguidelines-avoid-magic-numbers, readability-magic-numbers)

class TestSingleThreadedExecutor : public testing::Test
{
protected:
    struct MySingleThreadedExecutor final : public SingleThreadedExecutor
    {
        class NowMock
        {
        public:
            MOCK_METHOD(TimePoint, now, (), (const, noexcept));  // NOLINT(bugprone-exception-escape)
        };

        using SingleThreadedExecutor::SingleThreadedExecutor;
        using SingleThreadedExecutor::releaseAllCallbackNodes;

        TimePoint now() const noexcept override
        {
            return now_mock_.now();
        }

        // NOLINTBEGIN
        StrictMock<NowMock> now_mock_;
        // NOLINTEND

    };  // MySingleThreadedExecutor

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

    const auto epsilon = 1ms;

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
    MySingleThreadedExecutor executor{mr_};

    auto nop = [](const TimePoint) { return; };

    Callback::Handle handle1;
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

    EXPECT_CALL(mr_mock, do_allocate(_, _)).WillOnce(Return(nullptr));

    MySingleThreadedExecutor executor{mr_mock};

    auto handle = executor.registerCallback([](auto) { return; });
    EXPECT_FALSE(handle);
    handle.reset();
}

TEST_F(TestSingleThreadedExecutor, scheduleAt_no_spin)
{
    MySingleThreadedExecutor executor{mr_};

    auto virtual_now = TimePoint{};

    bool was_called = false;
    auto handle     = executor.registerCallback([&](auto) { was_called = true; });
    EXPECT_FALSE(was_called);

    EXPECT_TRUE(handle.scheduleAt(virtual_now, Schedule::Once{}));
    EXPECT_FALSE(was_called);

    EXPECT_TRUE(handle.scheduleAt(virtual_now + 1ms, Schedule::Once{}));
    EXPECT_FALSE(was_called);

    handle.reset();
    EXPECT_FALSE(was_called);

    // b/c already reset
    EXPECT_FALSE(handle.scheduleAt(virtual_now, Schedule::Once{}));
}

TEST_F(TestSingleThreadedExecutor, spinOnce_no_callbacks)
{
    MySingleThreadedExecutor executor{mr_};

    const auto spin_result = executor.spinOnce();
    EXPECT_THAT(spin_result.next_exec_time, Eq(cetl::nullopt));
    EXPECT_THAT(spin_result.worst_lateness, Duration::zero());
}

TEST_F(TestSingleThreadedExecutor, spinOnce)
{
    MySingleThreadedExecutor executor{mr_};

    int  called = 0;
    auto handle = executor.registerCallback([&](auto) { ++called; });

    // Registered but not scheduled yet.
    //
    auto virtual_now = TimePoint{};
    EXPECT_CALL(executor.now_mock_, now()).WillRepeatedly(Return(virtual_now));
    auto spin_result = executor.spinOnce();
    EXPECT_THAT(called, 0);
    EXPECT_THAT(spin_result.next_exec_time, Eq(cetl::nullopt));
    EXPECT_THAT(spin_result.worst_lateness, Duration::zero());

    EXPECT_TRUE(handle.scheduleAt(virtual_now, Schedule::Once{}));
    EXPECT_TRUE(handle.scheduleAt(virtual_now + 4ms, Schedule::Once{}));

    const auto deadline = virtual_now + 10ms;

    while (virtual_now < deadline)
    {
        spin_result = executor.spinOnce();
        EXPECT_THAT(spin_result.worst_lateness, Duration::zero());

        virtual_now += 1ms;
        EXPECT_CALL(executor.now_mock_, now()).WillRepeatedly(Return(virtual_now));
    }

    EXPECT_THAT(called, 1);
}

TEST_F(TestSingleThreadedExecutor, spinOnce_auto_remove)
{
    MySingleThreadedExecutor executor{mr_};

    cetl::optional<TimePoint> called;
    auto                      handle = executor.registerCallback([&](auto now) { called = now; });

    auto virtual_now = TimePoint{};
    EXPECT_TRUE(handle.scheduleAt(virtual_now, Schedule::Once{}));
    EXPECT_TRUE(handle.scheduleAt(virtual_now + 4ms, Schedule::Once{true /* is_auto_remove */}));

    const auto deadline = virtual_now + 10ms;
    EXPECT_CALL(executor.now_mock_, now()).WillRepeatedly(Return(virtual_now));

    while (virtual_now < deadline)
    {
        const auto spin_result = executor.spinOnce();
        EXPECT_THAT(spin_result.worst_lateness, Duration::zero());

        virtual_now += 1ms;
        EXPECT_CALL(executor.now_mock_, now()).WillRepeatedly(Return(virtual_now));
    }
    EXPECT_THAT(called, Optional(TimePoint{4ms}));

    // release handle of auto-removed callback
    EXPECT_FALSE(handle.scheduleAt(virtual_now, Schedule::Once{}));
    handle.reset();
}

TEST_F(TestSingleThreadedExecutor, schedule_once_multiple)
{
    MySingleThreadedExecutor executor{mr_};

    std::vector<std::tuple<std::string, TimePoint>> calls;
    auto handle1 = executor.registerCallback([&](auto now) { calls.emplace_back(std::make_tuple("1", now)); });
    auto handle2 = executor.registerCallback([&](auto now) { calls.emplace_back(std::make_tuple("2", now)); });
    auto handle3 = executor.registerCallback([&](auto now) { calls.emplace_back(std::make_tuple("3", now)); });

    auto virtual_now = TimePoint{};
    EXPECT_TRUE(handle1.scheduleAt(virtual_now + 8ms, Schedule::Once{}));
    EXPECT_TRUE(handle2.scheduleAt(virtual_now + 3ms, Schedule::Once{}));
    EXPECT_TRUE(handle3.scheduleAt(virtual_now + 5ms, Schedule::Once{}));

    const auto deadline = virtual_now + 10ms;
    EXPECT_CALL(executor.now_mock_, now()).WillRepeatedly(Return(virtual_now));

    while (virtual_now < deadline)
    {
        const auto spin_result = executor.spinOnce();
        EXPECT_THAT(spin_result.worst_lateness, Duration::zero());

        virtual_now += 1ms;
        EXPECT_CALL(executor.now_mock_, now()).WillRepeatedly(Return(virtual_now));
    }
    EXPECT_THAT(calls,
                ElementsAre(std::make_tuple("2", TimePoint{3ms}),
                            std::make_tuple("3", TimePoint{5ms}),
                            std::make_tuple("1", TimePoint{8ms})));
}

TEST_F(TestSingleThreadedExecutor, schedule_once_multiple_with_the_same_exec_time)
{
    MySingleThreadedExecutor executor{mr_};

    std::vector<std::tuple<std::string, TimePoint>> calls;
    auto handle1 = executor.registerCallback([&](auto now) { calls.emplace_back(std::make_tuple("1", now)); });
    auto handle2 = executor.registerCallback([&](auto now) { calls.emplace_back(std::make_tuple("2", now)); });
    auto handle3 = executor.registerCallback([&](auto now) { calls.emplace_back(std::make_tuple("3", now)); });

    auto       virtual_now = TimePoint{};
    const auto exec_time   = virtual_now + 5ms;
    EXPECT_TRUE(handle2.scheduleAt(exec_time, Schedule::Once{}));
    EXPECT_TRUE(handle1.scheduleAt(exec_time, Schedule::Once{}));
    EXPECT_TRUE(handle3.scheduleAt(exec_time, Schedule::Once{}));

    const auto deadline = virtual_now + 10ms;
    EXPECT_CALL(executor.now_mock_, now()).WillRepeatedly(Return(virtual_now));

    while (virtual_now < deadline)
    {
        const auto spin_result = executor.spinOnce();
        EXPECT_THAT(spin_result.worst_lateness, Duration::zero());

        virtual_now += 1ms;
        EXPECT_CALL(executor.now_mock_, now()).WillRepeatedly(Return(virtual_now));
    }
    EXPECT_THAT(calls,
                ElementsAre(std::make_tuple("2", TimePoint{5ms}),
                            std::make_tuple("1", TimePoint{5ms}),
                            std::make_tuple("3", TimePoint{5ms})));
}

TEST_F(TestSingleThreadedExecutor, schedule_once_callback_recursively)
{
    MySingleThreadedExecutor executor{mr_};

    std::vector<std::tuple<int, TimePoint>> calls;

    int              counter = 0;
    Callback::Handle handle;
    handle = executor.registerCallback([&](auto now) {
        //
        ++counter;
        calls.emplace_back(std::make_tuple(counter, now));

        EXPECT_TRUE(handle.scheduleAt(now + 2ms, Schedule::Once{}));
    });

    auto virtual_now = TimePoint{};
    EXPECT_TRUE(handle.scheduleAt(virtual_now + 5ms, Schedule::Once{}));

    const auto deadline = virtual_now + 10ms;
    EXPECT_CALL(executor.now_mock_, now()).WillRepeatedly(Return(virtual_now));

    while (virtual_now < deadline)
    {
        const auto spin_result = executor.spinOnce();
        EXPECT_THAT(spin_result.worst_lateness, Duration::zero());

        virtual_now += 1ms;
        EXPECT_CALL(executor.now_mock_, now()).WillRepeatedly(Return(virtual_now));
    }
    EXPECT_THAT(calls,
                ElementsAre(std::make_tuple(1, TimePoint{5ms}),
                            std::make_tuple(2, TimePoint{7ms}),
                            std::make_tuple(3, TimePoint{9ms})));
}

TEST_F(TestSingleThreadedExecutor, reset_once_scheduling_from_callback)
{
    MySingleThreadedExecutor executor{mr_};

    std::vector<std::tuple<int, TimePoint>> calls;

    int              counter = 0;
    Callback::Handle handle;
    handle = executor.registerCallback([&](auto now) {
        //
        ++counter;
        calls.emplace_back(std::make_tuple(counter, now));

        handle.reset();
    });

    auto virtual_now = TimePoint{};
    EXPECT_TRUE(handle.scheduleAt(virtual_now + 5ms, Schedule::Once{}));

    const auto deadline = virtual_now + 10ms;
    EXPECT_CALL(executor.now_mock_, now()).WillRepeatedly(Return(virtual_now));

    while (virtual_now < deadline)
    {
        const auto spin_result = executor.spinOnce();
        EXPECT_THAT(spin_result.worst_lateness, Duration::zero());

        virtual_now += 1ms;
        EXPECT_CALL(executor.now_mock_, now()).WillRepeatedly(Return(virtual_now));
    }

    EXPECT_THAT(calls, ElementsAre(std::make_tuple(1, TimePoint{5ms})));
}

TEST_F(TestSingleThreadedExecutor, reset_once_scheduling_from_auto_remove_callback)
{
    MySingleThreadedExecutor executor{mr_};

    std::vector<std::tuple<int, TimePoint>> calls;

    int              counter = 0;
    Callback::Handle handle;
    handle = executor.registerCallback([&](auto now) {
        //
        ++counter;
        calls.emplace_back(std::make_tuple(counter, now));

        handle.reset();
    });

    auto virtual_now = TimePoint{};
    EXPECT_TRUE(handle.scheduleAt(virtual_now + 5ms, Schedule::Once{true /* is_auto_remove */}));

    const auto deadline = virtual_now + 10ms;
    EXPECT_CALL(executor.now_mock_, now()).WillRepeatedly(Return(virtual_now));

    while (virtual_now < deadline)
    {
        const auto spin_result = executor.spinOnce();
        EXPECT_THAT(spin_result.worst_lateness, Duration::zero());

        virtual_now += 1ms;
        EXPECT_CALL(executor.now_mock_, now()).WillRepeatedly(Return(virtual_now));
    }

    EXPECT_THAT(calls, ElementsAre(std::make_tuple(1, TimePoint{5ms})));
}

TEST_F(TestSingleThreadedExecutor, reset_repeat_scheduling_from_callback)
{
    MySingleThreadedExecutor executor{mr_};

    std::vector<std::tuple<int, TimePoint>> calls;

    int              counter = 0;
    Callback::Handle handle  = executor.registerCallback([&](auto now) {
        //
        ++counter;
        calls.emplace_back(std::make_tuple(counter, now));

        if (counter == 3)
        {
            handle.reset();
        }
    });

    auto virtual_now = TimePoint{};
    EXPECT_TRUE(handle.scheduleAt(virtual_now + 20ms, Schedule::Repeat{5ms}));

    const auto deadline = virtual_now + 100ms;
    EXPECT_CALL(executor.now_mock_, now()).WillRepeatedly(Return(virtual_now));

    while (virtual_now < deadline)
    {
        const auto spin_result = executor.spinOnce();
        EXPECT_THAT(spin_result.worst_lateness, Duration::zero());

        virtual_now = spin_result.next_exec_time.value_or(virtual_now + 1ms);
        EXPECT_CALL(executor.now_mock_, now()).WillRepeatedly(Return(virtual_now));
    }

    EXPECT_THAT(calls,
                ElementsAre(std::make_tuple(1, TimePoint{20ms}),
                            std::make_tuple(2, TimePoint{25ms}),
                            std::make_tuple(3, TimePoint{30ms})));
}

TEST_F(TestSingleThreadedExecutor, spinOnce_worsth_lateness)
{
    MySingleThreadedExecutor executor{mr_};

    std::vector<std::tuple<int, TimePoint>> calls;

    auto handle1 = executor.registerCallback([&](auto now) {
        //
        calls.emplace_back(std::make_tuple(1, now));
    });
    auto handle2 = executor.registerCallback([&](auto now) {
        //
        calls.emplace_back(std::make_tuple(2, now));
    });

    auto virtual_now = TimePoint{};
    EXPECT_TRUE(handle1.scheduleAt(virtual_now + 7ms, Schedule::Once{}));
    EXPECT_TRUE(handle2.scheduleAt(virtual_now + 4ms, Schedule::Once{}));

    // Emulate lateness by spinning at +10ms
    virtual_now += 10ms;
    EXPECT_CALL(executor.now_mock_, now()).WillRepeatedly(Return(virtual_now));

    const auto spin_result = executor.spinOnce();
    EXPECT_THAT(spin_result.next_exec_time, Eq(cetl::nullopt));
    EXPECT_THAT(spin_result.worst_lateness, 6ms);

    EXPECT_THAT(calls, ElementsAre(std::make_tuple(2, TimePoint{10ms}), std::make_tuple(1, TimePoint{10ms})));
}

TEST_F(TestSingleThreadedExecutor, handle_invalidation)
{
    MySingleThreadedExecutor executor{mr_};

    auto handle = executor.registerCallback([&](auto) {});
    executor.releaseAllCallbackNodes();
    EXPECT_FALSE(handle.scheduleAt({}, Schedule::Once{}));
}

// NOLINTEND(cppcoreguidelines-avoid-magic-numbers, readability-magic-numbers)

}  // namespace
