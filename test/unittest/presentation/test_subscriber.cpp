/// @copyright
/// Copyright (C) OpenCyphal Development Team  <opencyphal.org>
/// Copyright Amazon.com Inc. or its affiliates.
/// SPDX-License-Identifier: MIT

#include "../cetl_gtest_helpers.hpp"  // NOLINT(misc-include-cleaner)
#include "../tracking_memory_resource.hpp"
#include "../transport/msg_sessions_mock.hpp"
#include "../transport/transport_mock.hpp"
#include "../verification_utilities.hpp"
#include "../virtual_time_scheduler.hpp"

#include <cetl/pf17/cetlpf.hpp>
#include <libcyphal/presentation/presentation.hpp>
#include <libcyphal/presentation/subscriber.hpp>
#include <libcyphal/transport/errors.hpp>
#include <libcyphal/transport/msg_sessions.hpp>
#include <libcyphal/transport/types.hpp>
#include <libcyphal/types.hpp>

#include <nunavut/support/serialization.hpp>
#include <uavcan/node/GetInfo_1_0.hpp>
#include <uavcan/node/Heartbeat_1_0.hpp>

#include <gmock/gmock.h>
#include <gtest/gtest.h>

#include <type_traits>
#include <utility>

namespace
{

using libcyphal::TimePoint;
using libcyphal::UniquePtr;
using namespace libcyphal::presentation;  // NOLINT This our main concern here in the unit tests.
using namespace libcyphal::transport;     // NOLINT This our main concern here in the unit tests.

using libcyphal::verification_utilities::b;
using libcyphal::verification_utilities::makeIotaArray;

using testing::_;
using testing::Eq;
using testing::Invoke;
using testing::Return;
using testing::IsEmpty;
using testing::Optional;
using testing::StrictMock;
using testing::ElementsAre;
using testing::VariantWith;

// https://github.com/llvm/llvm-project/issues/53444
// NOLINTBEGIN(misc-unused-using-decls, misc-include-cleaner)
using std::literals::chrono_literals::operator""s;
using std::literals::chrono_literals::operator""ms;
// NOLINTEND(misc-unused-using-decls, misc-include-cleaner)

// NOLINTBEGIN(cppcoreguidelines-avoid-magic-numbers, readability-magic-numbers)

class TestSubscriber : public testing::Test
{
protected:
    void TearDown() override
    {
        EXPECT_THAT(mr_.allocations, IsEmpty());
        EXPECT_THAT(mr_.total_allocated_bytes, mr_.total_deallocated_bytes);
    }

    TimePoint now() const
    {
        return scheduler_.now();
    }

    // MARK: Data members:

    // NOLINTBEGIN
    libcyphal::VirtualTimeScheduler scheduler_{};
    TrackingMemoryResource          mr_;
    StrictMock<TransportMock>       transport_mock_;
    // NOLINTEND
};

// MARK: - Tests:

TEST_F(TestSubscriber, move)
{
    using Message = uavcan::node::Heartbeat_1_0;

    static_assert(!std::is_copy_assignable<Subscriber<Message>>::value, "Should not be copy assignable.");
    static_assert(std::is_move_assignable<Subscriber<Message>>::value, "Should be move assignable.");
    static_assert(!std::is_copy_constructible<Subscriber<Message>>::value, "Should not be copy constructible.");
    static_assert(std::is_move_constructible<Subscriber<Message>>::value, "Should be move constructible.");
    static_assert(!std::is_default_constructible<Subscriber<Message>>::value, "Should not be default constructible.");

    Presentation presentation{mr_, scheduler_, transport_mock_};

    StrictMock<MessageRxSessionMock> msg_rx_session_mock;
    EXPECT_CALL(msg_rx_session_mock, getParams())  //
        .WillOnce(Return(MessageRxParams{0, Message::_traits_::FixedPortId}));
    EXPECT_CALL(msg_rx_session_mock, setOnReceiveCallback(_)).Times(1);

    EXPECT_CALL(transport_mock_, makeMessageRxSession(_))  //
        .WillOnce(Invoke([&](const auto&) {
            return libcyphal::detail::makeUniquePtr<MessageRxSessionMock::RefWrapper::Spec>(mr_, msg_rx_session_mock);
        }));

    auto maybe_sub1 = presentation.makeSubscriber<Message>(Message::_traits_::FixedPortId);
    ASSERT_THAT(maybe_sub1, VariantWith<Subscriber<Message>>(_));
    auto sub1a = cetl::get<Subscriber<Message>>(std::move(maybe_sub1));

    auto sub1b = std::move(sub1a);

    auto maybe_sub2 = presentation.makeSubscriber<Message>(Message::_traits_::FixedPortId);
    ASSERT_THAT(maybe_sub2, VariantWith<Subscriber<Message>>(_));
    auto sub2 = cetl::get<Subscriber<Message>>(std::move(maybe_sub2));

    sub1b = std::move(sub2);

    EXPECT_CALL(msg_rx_session_mock, deinit()).Times(1);
}

// NOLINTEND(cppcoreguidelines-avoid-magic-numbers, readability-magic-numbers)

}  // namespace
