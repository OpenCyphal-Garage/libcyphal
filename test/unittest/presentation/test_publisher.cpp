/// @copyright
/// Copyright (C) OpenCyphal Development Team  <opencyphal.org>
/// Copyright Amazon.com Inc. or its affiliates.
/// SPDX-License-Identifier: MIT

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

class TestPublisher : public testing::Test
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

TEST_F(TestPublisher, copy_move_getSetPriority)
{
    using Message = uavcan::node::Heartbeat_1_0;

    static_assert(std::is_copy_assignable<Publisher<Message>>::value, "Should be copy assignable.");
    static_assert(std::is_move_assignable<Publisher<Message>>::value, "Should be move assignable.");
    static_assert(std::is_copy_constructible<Publisher<Message>>::value, "Should be copy constructible.");
    static_assert(std::is_move_constructible<Publisher<Message>>::value, "Should be move constructible.");
    static_assert(!std::is_default_constructible<Publisher<Message>>::value, "Should not be default constructible.");

    static_assert(std::is_copy_assignable<Publisher<void>>::value, "Should be copy assignable.");
    static_assert(std::is_move_assignable<Publisher<void>>::value, "Should be move assignable.");
    static_assert(std::is_copy_constructible<Publisher<void>>::value, "Should be copy constructible.");
    static_assert(std::is_move_constructible<Publisher<void>>::value, "Should be move constructible.");
    static_assert(!std::is_default_constructible<Publisher<void>>::value, "Should not be default constructible.");

    Presentation presentation{mr_, scheduler_, transport_mock_};

    StrictMock<MessageTxSessionMock> msg_tx_session_mock;
    const MessageTxParams            tx_params{Message::_traits_::FixedPortId};
    EXPECT_CALL(msg_tx_session_mock, getParams()).WillOnce(Return(tx_params));

    EXPECT_CALL(transport_mock_, makeMessageTxSession(MessageTxParamsEq(tx_params)))  //
        .WillOnce(Invoke([&](const auto&) {
            return libcyphal::detail::makeUniquePtr<MessageTxSessionMock::RefWrapper::Spec>(mr_, msg_tx_session_mock);
        }));

    auto maybe_pub1 = presentation.makePublisher<Message>(tx_params.subject_id);
    ASSERT_THAT(maybe_pub1, VariantWith<Publisher<Message>>(_));
    auto pub1a = cetl::get<Publisher<Message>>(std::move(maybe_pub1));
    EXPECT_THAT(pub1a.getPriority(), Priority::Nominal);

    pub1a.setPriority(Priority::Immediate);
    EXPECT_THAT(pub1a.getPriority(), Priority::Immediate);

    auto pub1b = std::move(pub1a);
    EXPECT_THAT(pub1b.getPriority(), Priority::Immediate);

    auto pub2 = pub1b;
    EXPECT_THAT(pub2.getPriority(), Priority::Immediate);
    pub2.setPriority(Priority::Slow);
    EXPECT_THAT(pub2.getPriority(), Priority::Slow);
    EXPECT_THAT(pub1b.getPriority(), Priority::Immediate);

    pub1b = pub2;
    EXPECT_THAT(pub1b.getPriority(), Priority::Slow);

    // Verify self-assignment.
    auto& pub1c = pub1b;
    pub1c       = pub1b;

    pub2.setPriority(Priority::Optional);
    pub1c = std::move(pub2);
    EXPECT_THAT(pub1c.getPriority(), Priority::Optional);

    EXPECT_CALL(msg_tx_session_mock, deinit()).Times(1);
}

TEST_F(TestPublisher, publish)
{
    using Message = uavcan::node::Heartbeat_1_0;

    Presentation presentation{mr_, scheduler_, transport_mock_};

    StrictMock<MessageTxSessionMock> msg_tx_session_mock;
    const MessageTxParams            tx_params{Message::_traits_::FixedPortId};
    EXPECT_CALL(msg_tx_session_mock, getParams()).WillOnce(Return(tx_params));

    EXPECT_CALL(transport_mock_, makeMessageTxSession(MessageTxParamsEq(tx_params)))  //
        .WillOnce(Invoke([&](const auto&) {
            return libcyphal::detail::makeUniquePtr<MessageTxSessionMock::RefWrapper::Spec>(mr_, msg_tx_session_mock);
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

        EXPECT_THAT(publisher->publish(now() + 200ms, Message{}), Eq(cetl::nullopt));
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
        EXPECT_THAT(publisher->publish(now() + 100ms, Message{}), Eq(cetl::nullopt));
    });
    scheduler_.scheduleAt(3s, [&](const auto&) {
        //
        EXPECT_CALL(msg_tx_session_mock, send(_, _))  //
            .WillOnce(Return(CapacityError{}));

        EXPECT_THAT(publisher->publish(now() + 100ms, Message{}), Optional(VariantWith<CapacityError>(_)));
    });
    scheduler_.scheduleAt(9s, [&](const auto&) {
        //
        EXPECT_CALL(msg_tx_session_mock, deinit()).Times(1);
        publisher.reset();
        testing::Mock::VerifyAndClearExpectations(&msg_tx_session_mock);
    });
    scheduler_.spinFor(10s);
}

TEST_F(TestPublisher, publish_with_serialization_failure)
{
    using SerError = nunavut::support::Error;
    using Message  = my_custom::bar_1_0;

    Presentation presentation{mr_, scheduler_, transport_mock_};

    StrictMock<MessageTxSessionMock> msg_tx_session_mock;
    const MessageTxParams            tx_params{0x123};
    EXPECT_CALL(msg_tx_session_mock, getParams()).WillOnce(Return(tx_params));

    EXPECT_CALL(transport_mock_, makeMessageTxSession(MessageTxParamsEq(tx_params)))  //
        .WillOnce(Invoke([&](const auto&) {
            return libcyphal::detail::makeUniquePtr<MessageTxSessionMock::RefWrapper::Spec>(mr_, msg_tx_session_mock);
        }));

    auto maybe_pub = presentation.makePublisher<Message>(tx_params.subject_id);
    ASSERT_THAT(maybe_pub, VariantWith<Publisher<Message>>(_));
    cetl::optional<Publisher<Message>> publisher{cetl::get<Publisher<Message>>(std::move(maybe_pub))};
    EXPECT_THAT(publisher->getPriority(), Priority::Nominal);
    publisher->setPriority(Priority::Exceptional);

    scheduler_.scheduleAt(1s, [&](const auto&) {
        //
        Message message{};
        // This will cause a serialization failure.
        message.some_stuff.resize(Message::_traits_::SerializationBufferSizeBytes);

        EXPECT_THAT(publisher->publish(now() + 200ms, message),
                    Optional(VariantWith<SerError>(SerError::SerializationBadArrayLength)));
    });
    scheduler_.scheduleAt(9s, [&](const auto&) {
        //
        EXPECT_CALL(msg_tx_session_mock, deinit()).Times(1);
        publisher.reset();
        testing::Mock::VerifyAndClearExpectations(&msg_tx_session_mock);
    });
    scheduler_.spinFor(10s);
}

TEST_F(TestPublisher, publishRawData)
{
    Presentation presentation{mr_, scheduler_, transport_mock_};

    static_assert(std::is_copy_assignable<Publisher<void>>::value, "Should be copy assignable.");
    static_assert(std::is_move_assignable<Publisher<void>>::value, "Should be move assignable.");
    static_assert(std::is_copy_constructible<Publisher<void>>::value, "Should be copy constructible.");
    static_assert(std::is_move_constructible<Publisher<void>>::value, "Should be move constructible.");
    static_assert(!std::is_default_constructible<Publisher<void>>::value, "Should not be default constructible.");

    StrictMock<MessageTxSessionMock> msg_tx_session_mock;
    const MessageTxParams            tx_params{123};
    EXPECT_CALL(msg_tx_session_mock, getParams()).WillOnce(Return(tx_params));

    EXPECT_CALL(transport_mock_, makeMessageTxSession(MessageTxParamsEq(tx_params)))  //
        .WillOnce(Invoke([&](const auto&) {
            return libcyphal::detail::makeUniquePtr<MessageTxSessionMock::RefWrapper::Spec>(mr_, msg_tx_session_mock);
        }));

    auto maybe_pub = presentation.makePublisher<void>(tx_params.subject_id);
    ASSERT_THAT(maybe_pub, VariantWith<Publisher<void>>(_));
    cetl::optional<Publisher<void>> publisher{cetl::get<Publisher<void>>(std::move(maybe_pub))};

    scheduler_.scheduleAt(1s, [&](const auto&) {
        //
        EXPECT_CALL(msg_tx_session_mock, send(_, _))  //
            .WillOnce(Invoke([now = now()](const auto& metadata, const auto frags) {
                //
                EXPECT_THAT(metadata.base.transfer_id, 1);
                EXPECT_THAT(metadata.base.priority, Priority::Nominal);
                EXPECT_THAT(metadata.deadline, now + 200ms);
                EXPECT_THAT(frags.size(), 1);
                EXPECT_THAT(frags[0], ElementsAre(b('1'), b('2'), b('3'), b('4'), b('5'), b('6')));
                return cetl::nullopt;
            }));

        const auto payload = makeIotaArray<6>(b('1'));
        EXPECT_THAT(publisher->publish(now() + 200ms, payload), Eq(cetl::nullopt));
    });
    scheduler_.scheduleAt(9s, [&](const auto&) {
        //
        EXPECT_CALL(msg_tx_session_mock, deinit()).Times(1);
        publisher.reset();
        testing::Mock::VerifyAndClearExpectations(&msg_tx_session_mock);
    });
    scheduler_.spinFor(10s);
}

// NOLINTEND(cppcoreguidelines-avoid-magic-numbers, readability-magic-numbers)

}  // namespace
