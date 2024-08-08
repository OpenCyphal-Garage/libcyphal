/// @copyright
/// Copyright (C) OpenCyphal Development Team  <opencyphal.org>
/// Copyright Amazon.com Inc. or its affiliates.
/// SPDX-License-Identifier: MIT

#include "../gtest_helpers.hpp"  // NOLINT(misc-include-cleaner) `PrintTo`-s are implicitly in use by gtest.

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

using testing::Eq;
using testing::Ge;
using testing::Le;
using testing::AllOf;
using testing::Return;
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
    class MySingleThreadedExecutor final : public SingleThreadedExecutor
    {
    public:
        class NowMock
        {
        public:
            MOCK_METHOD(TimePoint, now, (), (const, noexcept));  // NOLINT(bugprone-exception-escape)
        };

        TimePoint now() const noexcept override
        {
            return now_mock_.now();
        }

        // NOLINTBEGIN
        StrictMock<NowMock> now_mock_;
        // NOLINTEND

    };  // MySingleThreadedExecutor
};

// MARK: - Tests:

TEST_F(TestSingleThreadedExecutor, now)
{
    auto expected = TimePoint{std::chrono::duration_cast<Duration>(  //
        std::chrono::steady_clock::now().time_since_epoch())};

    const auto epsilon = 1ms;

    const SingleThreadedExecutor executor;
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
    MySingleThreadedExecutor executor;

    auto nop = [](const TimePoint) { return; };

    Callback::Any cb1;
    EXPECT_FALSE(cb1.has_value());

    cb1 = executor.registerCallback(nop);
    EXPECT_TRUE(cb1.has_value());

    auto cb2a{executor.registerCallback(nop)};
    EXPECT_TRUE(cb2a.has_value());

    auto cb2b{std::move(cb2a)};
    EXPECT_TRUE(cb2b.has_value());

    cb2b = executor.registerCallback(nop);
    EXPECT_TRUE(cb2b.has_value());

    cb1 = {};
    EXPECT_FALSE(cb1.has_value());

    cb2b = std::move(cb1);
    EXPECT_FALSE(cb2b.has_value());
}

TEST_F(TestSingleThreadedExecutor, scheduleAt_no_spin)
{
    MySingleThreadedExecutor executor;

    auto virtual_now = TimePoint{};

    bool was_called = false;
    auto callback   = executor.registerCallback([&](auto) { was_called = true; });
    EXPECT_TRUE(callback.has_value());
    EXPECT_FALSE(was_called);

    executor.scheduleCallback(callback, Schedule::Once{});
    EXPECT_FALSE(was_called);

    executor.scheduleCallback(callback, Schedule::Once{virtual_now + 1ms});
    EXPECT_FALSE(was_called);

    callback.reset();
    EXPECT_FALSE(callback.has_value());
    EXPECT_FALSE(was_called);

    // callback is already reset
    executor.scheduleCallback(callback, Schedule::Once{virtual_now});
}

TEST_F(TestSingleThreadedExecutor, spinOnce_no_callbacks)
{
    MySingleThreadedExecutor executor;

    const auto spin_result = executor.spinOnce();
    EXPECT_THAT(spin_result.next_exec_time, Eq(cetl::nullopt));
    EXPECT_THAT(spin_result.worst_lateness, Duration::zero());
}

TEST_F(TestSingleThreadedExecutor, spinOnce)
{
    MySingleThreadedExecutor executor;

    int  called   = 0;
    auto callback = executor.registerCallback([&](auto) { ++called; });

    // Registered but not scheduled yet.
    //
    auto virtual_now = TimePoint{};
    EXPECT_CALL(executor.now_mock_, now()).WillRepeatedly(Return(virtual_now));
    auto spin_result = executor.spinOnce();
    EXPECT_THAT(called, 0);
    EXPECT_THAT(spin_result.next_exec_time, Eq(cetl::nullopt));
    EXPECT_THAT(spin_result.worst_lateness, Duration::zero());

    executor.scheduleCallback(callback, Schedule::Once{virtual_now});
    executor.scheduleCallback(callback, Schedule::Once{virtual_now + 4ms});

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

TEST_F(TestSingleThreadedExecutor, schedule_once_multiple)
{
    MySingleThreadedExecutor executor;

    std::vector<std::tuple<std::string, TimePoint>> calls;
    auto cb1 = executor.registerCallback([&](auto now) { calls.emplace_back(std::make_tuple("1", now)); });
    auto cb2 = executor.registerCallback([&](auto now) { calls.emplace_back(std::make_tuple("2", now)); });
    auto cb3 = executor.registerCallback([&](auto now) { calls.emplace_back(std::make_tuple("3", now)); });

    auto virtual_now = TimePoint{};
    executor.scheduleCallback(cb1, Schedule::Once{virtual_now + 8ms});
    executor.scheduleCallback(cb2, Schedule::Once{virtual_now + 3ms});
    executor.scheduleCallback(cb3, Schedule::Once{virtual_now + 5ms});

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
    MySingleThreadedExecutor executor;

    std::vector<std::tuple<std::string, TimePoint>> calls;
    auto cb1 = executor.registerCallback([&](auto now) { calls.emplace_back(std::make_tuple("1", now)); });
    auto cb2 = executor.registerCallback([&](auto now) { calls.emplace_back(std::make_tuple("2", now)); });
    auto cb3 = executor.registerCallback([&](auto now) { calls.emplace_back(std::make_tuple("3", now)); });

    auto       virtual_now = TimePoint{};
    const auto exec_time   = virtual_now + 5ms;
    executor.scheduleCallback(cb2, Schedule::Once{exec_time});
    executor.scheduleCallback(cb1, Schedule::Once{exec_time});
    executor.scheduleCallback(cb3, Schedule::Once{exec_time});

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
    MySingleThreadedExecutor executor;

    std::vector<std::tuple<int, TimePoint>> calls;

    int           counter = 0;
    Callback::Any cb;
    cb = executor.registerCallback([&](auto now) {
        //
        ++counter;
        calls.emplace_back(std::make_tuple(counter, now));

        executor.scheduleCallback(cb, Schedule::Once{now + 2ms});
    });

    auto virtual_now = TimePoint{};
    executor.scheduleCallback(cb, Schedule::Once{virtual_now + 5ms});

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
    MySingleThreadedExecutor executor;

    std::vector<std::tuple<int, TimePoint>> calls;

    int           counter = 0;
    Callback::Any cb;
    cb = executor.registerCallback([&](auto now) {
        //
        ++counter;
        calls.emplace_back(std::make_tuple(counter, now));

        cb.reset();
    });

    auto virtual_now = TimePoint{};
    executor.scheduleCallback(cb, Schedule::Once{virtual_now + 5ms});

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
    MySingleThreadedExecutor executor;

    std::vector<std::tuple<int, TimePoint>> calls;

    int           counter = 0;
    Callback::Any cb      = executor.registerCallback([&](auto now) {
        //
        ++counter;
        calls.emplace_back(std::make_tuple(counter, now));

        if (counter == 3)
        {
            cb.reset();
        }
    });

    auto virtual_now = TimePoint{};
    executor.scheduleCallback(cb, Schedule::Repeat{virtual_now + 20ms, 5ms});

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
    MySingleThreadedExecutor executor;

    std::vector<std::tuple<int, TimePoint>> calls;

    auto cb1 = executor.registerCallback([&](auto now) {
        //
        calls.emplace_back(std::make_tuple(1, now));
    });
    auto cb2 = executor.registerCallback([&](auto now) {
        //
        calls.emplace_back(std::make_tuple(2, now));
    });

    auto virtual_now = TimePoint{};
    executor.scheduleCallback(cb1, Schedule::Once{virtual_now + 7ms});
    executor.scheduleCallback(cb2, Schedule::Once{virtual_now + 4ms});

    // Emulate lateness by spinning at +10ms
    virtual_now += 10ms;
    EXPECT_CALL(executor.now_mock_, now()).WillRepeatedly(Return(virtual_now));

    const auto spin_result = executor.spinOnce();
    EXPECT_THAT(spin_result.next_exec_time, Eq(cetl::nullopt));
    EXPECT_THAT(spin_result.worst_lateness, 6ms);

    EXPECT_THAT(calls, ElementsAre(std::make_tuple(2, TimePoint{10ms}), std::make_tuple(1, TimePoint{10ms})));
}

// NOLINTEND(cppcoreguidelines-avoid-magic-numbers, readability-magic-numbers)

}  // namespace
