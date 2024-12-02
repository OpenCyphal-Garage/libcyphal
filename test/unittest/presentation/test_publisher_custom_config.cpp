/// @copyright
/// Copyright (C) OpenCyphal Development Team  <opencyphal.org>
/// Copyright Amazon.com Inc. or its affiliates.
/// SPDX-License-Identifier: MIT

#define LIBCYPHAL_CONFIG "custom_libcyphal_config.hpp"

#include "cetl_gtest_helpers.hpp"  // NOLINT(misc-include-cleaner)
#include "my_custom/bar_1_0.hpp"
#include "tracking_memory_resource.hpp"
#include "transport/msg_sessions_mock.hpp"
#include "transport/transport_gtest_helpers.hpp"
#include "transport/transport_mock.hpp"
#include "verification_utilities.hpp"
#include "virtual_time_scheduler.hpp"

#include <cetl/pf17/cetlpf.hpp>
#include <libcyphal/presentation/presentation.hpp>
#include <libcyphal/presentation/publisher.hpp>
#include <libcyphal/transport/errors.hpp>
#include <libcyphal/transport/msg_sessions.hpp>
#include <libcyphal/transport/types.hpp>
#include <libcyphal/types.hpp>

#include <nunavut/support/serialization.hpp>
#include <uavcan/node/Heartbeat_1_0.hpp>
#include <uavcan/node/GetInfo_1_0.hpp>

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
using libcyphal::verification_utilities::makeSpansFrom;

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

class TestPublisherCustomConfig : public testing::Test
{
protected:
    using UniquePtrMsgTxSpec = MessageTxSessionMock::RefWrapper::Spec;

    void SetUp() override
    {
        cetl::pmr::set_default_resource(&mr_);
    }

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
    libcyphal::VirtualTimeScheduler        scheduler_{};
    TrackingMemoryResource                 mr_;
    StrictMock<TransportMock>              transport_mock_;
    cetl::pmr::polymorphic_allocator<void> mr_alloc_{&mr_};
    // NOLINTEND
};

// MARK: - Tests:

TEST_F(TestPublisherCustomConfig, publish)
{
    using Message = uavcan::node::Heartbeat_1_0;
    // using Message = uavcan::node::GetInfo::Request_1_0;

    Presentation presentation{mr_, scheduler_, transport_mock_};

    StrictMock<MessageTxSessionMock> msg_tx_session_mock;
    constexpr MessageTxParams        tx_params{Message::_traits_::FixedPortId};
    EXPECT_CALL(msg_tx_session_mock, getParams()).WillOnce(Return(tx_params));

    EXPECT_CALL(transport_mock_, makeMessageTxSession(MessageTxParamsEq(tx_params)))  //
        .WillOnce(Invoke([&](const auto&) {                                           //
            return libcyphal::detail::makeUniquePtr<UniquePtrMsgTxSpec>(mr_, msg_tx_session_mock);
        }));

    auto maybe_pub = presentation.makePublisher<Message>(tx_params.subject_id);
    ASSERT_THAT(maybe_pub, VariantWith<Publisher<Message>>(_));
    cetl::optional<Publisher<Message>> publisher{cetl::get<Publisher<Message>>(std::move(maybe_pub))};
    EXPECT_THAT(publisher->getPriority(), Priority::Nominal);
    publisher->setPriority(Priority::Exceptional);

    scheduler_.scheduleAt(1s, [&](const auto&) {
        //
        EXPECT_CALL(msg_tx_session_mock, send(_, _))  //
            .WillOnce(Invoke([now = now()](const auto& metadata, const auto) {
                //
                EXPECT_THAT(metadata.base.transfer_id, 1);
                EXPECT_THAT(metadata.base.priority, Priority::Exceptional);
                EXPECT_THAT(metadata.deadline, now + 200ms);
                return cetl::nullopt;
            }));

        EXPECT_THAT(publisher->publish(now() + 200ms, Message{&mr_}), Eq(cetl::nullopt));
    });
    scheduler_.scheduleAt(2s, [&](const auto&) {
        //
        EXPECT_CALL(msg_tx_session_mock, send(_, _))  //
            .WillOnce(Invoke([now = now()](const auto& metadata, const auto) {
                //
                EXPECT_THAT(metadata.base.transfer_id, 2);
                EXPECT_THAT(metadata.base.priority, Priority::Fast);
                EXPECT_THAT(metadata.deadline, now + 100ms);
                return cetl::nullopt;
            }));

        publisher->setPriority(Priority::Fast);
        EXPECT_THAT(publisher->publish(now() + 100ms, Message{&mr_}), Eq(cetl::nullopt));
    });
    scheduler_.scheduleAt(3s, [&](const auto&) {
        //
        EXPECT_CALL(msg_tx_session_mock, send(_, _))  //
            .WillOnce(Return(CapacityError{}));

        EXPECT_THAT(publisher->publish(now() + 100ms, Message{&mr_}), Optional(VariantWith<CapacityError>(_)));
    });
    scheduler_.scheduleAt(9s, [&](const auto&) {
        //
        publisher.reset();
        testing::Mock::VerifyAndClearExpectations(&msg_tx_session_mock);
        EXPECT_CALL(msg_tx_session_mock, deinit()).Times(1);
    });
    scheduler_.spinFor(10s);
}

// NOLINTEND(cppcoreguidelines-avoid-magic-numbers, readability-magic-numbers)

}  // namespace
