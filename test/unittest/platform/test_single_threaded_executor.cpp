/// @copyright
/// Copyright (C) OpenCyphal Development Team  <opencyphal.org>
/// Copyright Amazon.com Inc. or its affiliates.
/// SPDX-License-Identifier: MIT

#include "../gtest_helpers.hpp"  // NOLINT(misc-include-cleaner) `PrintTo`-s are implicitly in use by gtest.

#include <cetl/pf17/cetlpf.hpp>
#include <cetl/rtti.hpp>
#include <cetl/unbounded_variant.hpp>
#include <libcyphal/executor.hpp>
#include <libcyphal/platform/single_threaded_executor.hpp>
#include <libcyphal/transport/errors.hpp>
#include <libcyphal/types.hpp>

#include <gmock/gmock.h>
#include <gtest/gtest.h>

#include <algorithm>
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
using testing::IsNull;
using testing::Return;
using testing::NotNull;
using testing::InSequence;
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

TEST_F(TestSingleThreadedExecutor, rtti)
{
    // mutable
    {
        SingleThreadedExecutor mut_executor;
        EXPECT_THAT(cetl::rtti_cast<libcyphal::IExecutor*>(&mut_executor), NotNull());
        EXPECT_THAT(cetl::rtti_cast<libcyphal::IExecutor::Callback::Interface*>(&mut_executor), IsNull());
    }
    // const
    {
        const SingleThreadedExecutor const_executor;
        EXPECT_THAT(cetl::rtti_cast<libcyphal::IExecutor*>(&const_executor), NotNull());
        EXPECT_THAT(cetl::rtti_cast<libcyphal::IExecutor::Callback::Interface*>(&const_executor), IsNull());
    }
}

TEST_F(TestSingleThreadedExecutor, registerCallback)
{
    MySingleThreadedExecutor executor;

    auto nop = [](const Callback::Arg&) {};

    Callback::Any cb1;
    EXPECT_FALSE(cb1);

    cb1 = executor.registerCallback(nop);
    EXPECT_TRUE(cb1);

    auto cb2a{executor.registerCallback(nop)};
    EXPECT_TRUE(cb2a);

    // To cover RTTI casts.
    EXPECT_THAT(cetl::get_if<libcyphal::transport::AnyFailure>(&cb2a), IsNull());
    EXPECT_THAT(cetl::get_if<libcyphal::transport::AnyFailure>(static_cast<const Callback::Any*>(&cb2a)), IsNull());
    EXPECT_THAT(cetl::get_if<libcyphal::IExecutor::Callback::Interface>(static_cast<const Callback::Any*>(&cb2a)),
                NotNull());

    auto cb2b{std::move(cb2a)};
    EXPECT_TRUE(cb2b);

    cb2b = executor.registerCallback(nop);
    EXPECT_TRUE(cb2b);

    cb1 = {};
    EXPECT_FALSE(cb1);

    cb2b = std::move(cb1);
    EXPECT_FALSE(cb2b);

    // To cover RTTI const cast.
    const auto cb3 = executor.registerCallback(nop);
    EXPECT_THAT(cetl::get_if<libcyphal::transport::AnyFailure>(&cb3), IsNull());
}

TEST_F(TestSingleThreadedExecutor, scheduleAt_no_spin)
{
    MySingleThreadedExecutor executor;

    auto virtual_now = TimePoint{};

    bool was_called = false;
    auto callback   = executor.registerCallback([&](const auto&) { was_called = true; });
    EXPECT_TRUE(callback);
    EXPECT_FALSE(was_called);

    EXPECT_TRUE(callback.schedule(Schedule::Once{}));
    EXPECT_FALSE(was_called);

    EXPECT_TRUE(callback.schedule(Schedule::Once{virtual_now + 1ms}));
    EXPECT_FALSE(was_called);

    callback.reset();
    EXPECT_FALSE(callback);
    EXPECT_FALSE(was_called);

    // callback is already reset
    EXPECT_FALSE(callback.schedule(Schedule::Once{virtual_now}));
}

TEST_F(TestSingleThreadedExecutor, spinOnce_no_callbacks)
{
    MySingleThreadedExecutor executor;

    EXPECT_CALL(executor.now_mock_, now()).WillOnce(Return(TimePoint{123us}));

    const auto spin_result = executor.spinOnce();
    EXPECT_THAT(spin_result.next_exec_time, Eq(cetl::nullopt));
    EXPECT_THAT(spin_result.worst_lateness, Duration::zero());
    EXPECT_THAT(spin_result.approx_now, TimePoint{123us});
}

TEST_F(TestSingleThreadedExecutor, spinOnce)
{
    MySingleThreadedExecutor executor;

    int  called   = 0;
    auto callback = executor.registerCallback([&](const auto&) { ++called; });

    // Registered but not scheduled yet.
    //
    auto virtual_now = TimePoint{};
    EXPECT_CALL(executor.now_mock_, now()).WillRepeatedly(Return(virtual_now));
    auto spin_result = executor.spinOnce();
    EXPECT_THAT(called, 0);
    EXPECT_THAT(spin_result.next_exec_time, Eq(cetl::nullopt));
    EXPECT_THAT(spin_result.worst_lateness, Duration::zero());
    EXPECT_THAT(spin_result.approx_now, virtual_now);

    EXPECT_TRUE(callback.schedule(Schedule::Once{virtual_now}));
    EXPECT_TRUE(callback.schedule(Schedule::Once{virtual_now + 4ms}));

    const auto deadline = virtual_now + 10ms;

    while (virtual_now < deadline)
    {
        spin_result = executor.spinOnce();
        EXPECT_THAT(spin_result.worst_lateness, Duration::zero());
        EXPECT_THAT(spin_result.approx_now, virtual_now);

        virtual_now += 1ms;
        EXPECT_CALL(executor.now_mock_, now()).WillRepeatedly(Return(virtual_now));
    }

    EXPECT_THAT(called, 1);
}

TEST_F(TestSingleThreadedExecutor, schedule_once_multiple)
{
    MySingleThreadedExecutor executor;

    std::vector<std::tuple<std::string, TimePoint, TimePoint>> calls;

    auto cb1 = executor.registerCallback([&](const auto& arg) {  //
        calls.emplace_back("1", arg.exec_time, arg.approx_now);
    });
    auto cb2 = executor.registerCallback([&](const auto& arg) {  //
        calls.emplace_back("2", arg.exec_time, arg.approx_now);
    });
    auto cb3 = executor.registerCallback([&](const auto& arg) {  //
        calls.emplace_back("3", arg.exec_time, arg.approx_now);
    });

    auto virtual_now = TimePoint{};
    EXPECT_TRUE(cb1.schedule(Schedule::Once{virtual_now + 8ms}));
    EXPECT_TRUE(cb2.schedule(Schedule::Once{virtual_now + 3ms}));
    EXPECT_TRUE(cb3.schedule(Schedule::Once{virtual_now + 5ms}));

    const auto deadline = virtual_now + 10ms;
    EXPECT_CALL(executor.now_mock_, now()).WillRepeatedly(Return(virtual_now));

    while (virtual_now < deadline)
    {
        const auto spin_result = executor.spinOnce();
        EXPECT_THAT(spin_result.worst_lateness, Duration::zero());
        EXPECT_THAT(spin_result.approx_now, virtual_now);

        virtual_now += 1ms;
        EXPECT_CALL(executor.now_mock_, now()).WillRepeatedly(Return(virtual_now));
    }
    EXPECT_THAT(calls,
                ElementsAre(std::make_tuple("2", TimePoint{3ms}, TimePoint{3ms}),
                            std::make_tuple("3", TimePoint{5ms}, TimePoint{5ms}),
                            std::make_tuple("1", TimePoint{8ms}, TimePoint{8ms})));
}

TEST_F(TestSingleThreadedExecutor, schedule_once_multiple_with_the_same_exec_time)
{
    MySingleThreadedExecutor executor;

    std::vector<std::tuple<std::string, TimePoint, TimePoint>> calls;

    auto cb1 = executor.registerCallback([&](const auto& arg) {  //
        calls.emplace_back("1", arg.exec_time, arg.approx_now);
    });
    auto cb2 = executor.registerCallback([&](const auto& arg) {  //
        calls.emplace_back("2", arg.exec_time, arg.approx_now);
    });
    auto cb3 = executor.registerCallback([&](const auto& arg) {  //
        calls.emplace_back("3", arg.exec_time, arg.approx_now);
    });

    auto       virtual_now = TimePoint{};
    const auto exec_time   = virtual_now + 5ms;
    EXPECT_TRUE(cb2.schedule(Schedule::Once{exec_time}));
    EXPECT_TRUE(cb1.schedule(Schedule::Once{exec_time}));
    EXPECT_TRUE(cb3.schedule(Schedule::Once{exec_time}));

    const auto deadline = virtual_now + 10ms;
    EXPECT_CALL(executor.now_mock_, now()).WillRepeatedly(Return(virtual_now));

    while (virtual_now < deadline)
    {
        const auto spin_result = executor.spinOnce();
        EXPECT_THAT(spin_result.worst_lateness, Duration::zero());
        EXPECT_THAT(spin_result.approx_now, virtual_now);

        virtual_now += 1ms;
        EXPECT_CALL(executor.now_mock_, now()).WillRepeatedly(Return(virtual_now));
    }
    EXPECT_THAT(calls,
                ElementsAre(std::make_tuple("2", TimePoint{5ms}, TimePoint{5ms}),
                            std::make_tuple("1", TimePoint{5ms}, TimePoint{5ms}),
                            std::make_tuple("3", TimePoint{5ms}, TimePoint{5ms})));
}

TEST_F(TestSingleThreadedExecutor, schedule_once_callback_recursively)
{
    MySingleThreadedExecutor executor;

    std::vector<std::tuple<int, TimePoint, TimePoint>> calls;

    int           counter = 0;
    Callback::Any cb;
    cb = executor.registerCallback([&](const auto& arg) {
        //
        ++counter;
        calls.emplace_back(counter, arg.exec_time, arg.approx_now);

        EXPECT_TRUE(cb.schedule(Schedule::Once{arg.approx_now + 2ms}));
    });

    auto virtual_now = TimePoint{};
    EXPECT_TRUE(cb.schedule(Schedule::Once{virtual_now + 5ms}));

    const auto deadline = virtual_now + 10ms;
    EXPECT_CALL(executor.now_mock_, now()).WillRepeatedly(Return(virtual_now));

    while (virtual_now < deadline)
    {
        const auto spin_result = executor.spinOnce();
        EXPECT_THAT(spin_result.worst_lateness, Duration::zero());
        EXPECT_THAT(spin_result.approx_now, virtual_now);

        virtual_now += 1ms;
        EXPECT_CALL(executor.now_mock_, now()).WillRepeatedly(Return(virtual_now));
    }
    EXPECT_THAT(calls,
                ElementsAre(std::make_tuple(1, TimePoint{5ms}, TimePoint{5ms}),
                            std::make_tuple(2, TimePoint{7ms}, TimePoint{7ms}),
                            std::make_tuple(3, TimePoint{9ms}, TimePoint{9ms})));
}

TEST_F(TestSingleThreadedExecutor, reset_once_scheduling_from_callback)
{
    MySingleThreadedExecutor executor;

    std::vector<std::tuple<int, TimePoint, TimePoint>> calls;

    int           counter = 0;
    Callback::Any cb;
    cb = executor.registerCallback([&](const auto& arg) {
        //
        ++counter;
        calls.emplace_back(counter, arg.exec_time, arg.approx_now);

        cb.reset();
    });

    auto virtual_now = TimePoint{};
    EXPECT_TRUE(cb.schedule(Schedule::Once{virtual_now + 5ms}));

    const auto deadline = virtual_now + 10ms;
    EXPECT_CALL(executor.now_mock_, now()).WillRepeatedly(Return(virtual_now));

    while (virtual_now < deadline)
    {
        const auto spin_result = executor.spinOnce();
        EXPECT_THAT(spin_result.worst_lateness, Duration::zero());
        EXPECT_THAT(spin_result.approx_now, virtual_now);

        virtual_now += 1ms;
        EXPECT_CALL(executor.now_mock_, now()).WillRepeatedly(Return(virtual_now));
    }

    EXPECT_THAT(calls, ElementsAre(std::make_tuple(1, TimePoint{5ms}, TimePoint{5ms})));
}

TEST_F(TestSingleThreadedExecutor, reset_repeat_scheduling_from_callback)
{
    MySingleThreadedExecutor executor;

    std::vector<std::tuple<int, TimePoint, TimePoint>> calls;

    int           counter = 0;
    Callback::Any cb      = executor.registerCallback([&](const auto& arg) {
        //
        ++counter;
        calls.emplace_back(counter, arg.exec_time, arg.approx_now);

        if (counter == 3)
        {
            cb.reset();
        }
    });

    auto virtual_now = TimePoint{};
    EXPECT_TRUE(cb.schedule(Schedule::Repeat{virtual_now + 20ms, 5ms}));

    const auto deadline = virtual_now + 100ms;
    EXPECT_CALL(executor.now_mock_, now()).WillRepeatedly(Return(virtual_now));

    while (virtual_now < deadline)
    {
        const auto spin_result = executor.spinOnce();
        EXPECT_THAT(spin_result.worst_lateness, Duration::zero());
        EXPECT_THAT(spin_result.approx_now, virtual_now);

        virtual_now = spin_result.next_exec_time.value_or(virtual_now + 1ms);
        EXPECT_CALL(executor.now_mock_, now()).WillRepeatedly(Return(virtual_now));
    }

    EXPECT_THAT(calls,
                ElementsAre(std::make_tuple(1, TimePoint{20ms}, TimePoint{20ms}),
                            std::make_tuple(2, TimePoint{25ms}, TimePoint{25ms}),
                            std::make_tuple(3, TimePoint{30ms}, TimePoint{30ms})));
}

TEST_F(TestSingleThreadedExecutor, spinOnce_worsth_lateness)
{
    MySingleThreadedExecutor executor;

    std::vector<std::tuple<int, TimePoint, TimePoint>> calls;

    auto cb1 = executor.registerCallback([&](const auto& arg) {
        //
        calls.emplace_back(1, arg.exec_time, arg.approx_now);
    });
    auto cb2 = executor.registerCallback([&](const auto& arg) {
        //
        calls.emplace_back(2, arg.exec_time, arg.approx_now);
    });

    const TimePoint start_time = TimePoint{100ms};
    EXPECT_TRUE(cb1.schedule(Schedule::Once{start_time + 7ms}));
    EXPECT_TRUE(cb2.schedule(Schedule::Once{start_time + 4ms}));

    // Emulate lateness by spinning at +10ms
    const InSequence seq;
    EXPECT_CALL(executor.now_mock_, now()).WillOnce(Return(start_time + 6ms));
    EXPECT_CALL(executor.now_mock_, now()).WillOnce(Return(start_time + 15ms));
    EXPECT_CALL(executor.now_mock_, now()).WillOnce(Return(start_time + 17ms));

    const auto spin_result = executor.spinOnce();
    EXPECT_THAT(spin_result.next_exec_time, Eq(cetl::nullopt));
    EXPECT_THAT(spin_result.worst_lateness, std::max(6ms - 4ms, 15ms - 7ms));
    EXPECT_THAT(spin_result.approx_now, start_time + 17ms);

    EXPECT_THAT(calls,
                ElementsAre(std::make_tuple(2, start_time + 4ms, start_time + 6ms),
                            std::make_tuple(1, start_time + 7ms, start_time + 15ms)));
}

// NOLINTEND(cppcoreguidelines-avoid-magic-numbers, readability-magic-numbers)

}  // namespace
