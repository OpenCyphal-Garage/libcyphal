/// @copyright
/// Copyright (C) OpenCyphal Development Team  <opencyphal.org>
/// Copyright Amazon.com Inc. or its affiliates.
/// SPDX-License-Identifier: MIT

#include "../memory_resource_mock.hpp"
#include "../tracking_memory_resource.hpp"
#include "../transport/msg_sessions_mock.hpp"
#include "../transport/transport_mock.hpp"
#include "../virtual_time_scheduler.hpp"

#include <libcyphal/presentation/presentation.hpp>
#include <libcyphal/presentation/publisher.hpp>
#include <libcyphal/presentation/publisher_impl.hpp>
#include <libcyphal/presentation/subscriber.hpp>
#include <libcyphal/presentation/subscriber_impl.hpp>
#include <libcyphal/transport/errors.hpp>
#include <libcyphal/transport/msg_sessions.hpp>
#include <libcyphal/types.hpp>

#include <uavcan/node/Heartbeat_1_0.hpp>

#include <gmock/gmock.h>
#include <gtest/gtest.h>

namespace
{

using libcyphal::TimePoint;
using libcyphal::UniquePtr;
using namespace libcyphal::presentation;  // NOLINT This our main concern here in the unit tests.
using namespace libcyphal::transport;     // NOLINT This our main concern here in the unit tests.

using testing::_;
using testing::Invoke;
using testing::Return;
using testing::IsEmpty;
using testing::StrictMock;
using testing::VariantWith;

// NOLINTBEGIN(cppcoreguidelines-avoid-magic-numbers, readability-magic-numbers)

class TestPresentation : public testing::Test
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

TEST_F(TestPresentation, makePublisher)
{
    using Message = uavcan::node::Heartbeat_1_0;

    Presentation presentation{mr_, scheduler_, transport_mock_};

    StrictMock<MessageTxSessionMock> msg_tx_session_mock;
    EXPECT_CALL(msg_tx_session_mock, getParams())  //
        .WillOnce(Return(MessageTxParams{Message::_traits_::FixedPortId}));

    EXPECT_CALL(transport_mock_, makeMessageTxSession(_))  //
        .WillOnce(Invoke([&](const auto&) {
            return libcyphal::detail::makeUniquePtr<MessageTxSessionMock::RefWrapper::Spec>(mr_, msg_tx_session_mock);
        }));

    auto maybe_pub1 = presentation.makePublisher<Message>(Message::_traits_::FixedPortId);
    EXPECT_THAT(maybe_pub1, VariantWith<Publisher<Message>>(_));

    auto maybe_pub2 = presentation.makePublisher<Message>(Message::_traits_::FixedPortId);
    EXPECT_THAT(maybe_pub2, VariantWith<Publisher<Message>>(_));

    EXPECT_CALL(msg_tx_session_mock, deinit()).Times(1);
}

TEST_F(TestPresentation, makePublisher_with_failure)
{
    using Message = uavcan::node::Heartbeat_1_0;

    StrictMock<MemoryResourceMock> mr_mock;
    mr_mock.redirectExpectedCallsTo(mr_);

    Presentation presentation{mr_mock, scheduler_, transport_mock_};

    // AlreadyExistsError
    {
        EXPECT_CALL(transport_mock_, makeMessageTxSession(_))  //
            .WillOnce(Return(AlreadyExistsError{}));

        auto maybe_pub = presentation.makePublisher<Message>(Message::_traits_::FixedPortId);
        EXPECT_THAT(maybe_pub, VariantWith<AnyFailure>(VariantWith<AlreadyExistsError>(_)));
    }
    // MemoryError due to nullptr
    {
        EXPECT_CALL(transport_mock_, makeMessageTxSession(_))  //
            .WillOnce(Return(nullptr));

        auto maybe_pub = presentation.makePublisher<Message>(Message::_traits_::FixedPortId);
        EXPECT_THAT(maybe_pub, VariantWith<AnyFailure>(VariantWith<MemoryError>(_)));
    }
    // Emulate that there is no memory available for the `PublisherImpl`.
    {
        using UniquePtrSpec = MessageTxSessionMock::RefWrapper::Spec;

        StrictMock<MessageTxSessionMock> msg_tx_session_mock;
        EXPECT_CALL(msg_tx_session_mock, deinit()).Times(1);

        EXPECT_CALL(transport_mock_, makeMessageTxSession(_))  //
            .WillOnce(Invoke([&](const auto&) {
                return libcyphal::detail::makeUniquePtr<UniquePtrSpec>(mr_, msg_tx_session_mock);
            }));
        EXPECT_CALL(mr_mock, do_allocate(sizeof(detail::PublisherImpl), _)).WillOnce(Return(nullptr));

        auto maybe_pub = presentation.makePublisher<Message>(Message::_traits_::FixedPortId);
        EXPECT_THAT(maybe_pub, VariantWith<AnyFailure>(VariantWith<MemoryError>(_)));
    }
}

TEST_F(TestPresentation, makeSubscriber)
{
    using Message = uavcan::node::Heartbeat_1_0;

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
    EXPECT_THAT(maybe_sub1, VariantWith<Subscriber<Message>>(_));

    auto maybe_sub2 = presentation.makeSubscriber<Message>(Message::_traits_::FixedPortId);
    EXPECT_THAT(maybe_sub2, VariantWith<Subscriber<Message>>(_));

    EXPECT_CALL(msg_rx_session_mock, deinit()).Times(1);
}

TEST_F(TestPresentation, makeSubscriber_with_failure)
{
    using Message = uavcan::node::Heartbeat_1_0;

    StrictMock<MemoryResourceMock> mr_mock;
    mr_mock.redirectExpectedCallsTo(mr_);

    Presentation presentation{mr_mock, scheduler_, transport_mock_};

    // AlreadyExistsError
    {
        EXPECT_CALL(transport_mock_, makeMessageRxSession(_))  //
            .WillOnce(Return(AlreadyExistsError{}));

        auto maybe_sub = presentation.makeSubscriber<Message>(Message::_traits_::FixedPortId);
        EXPECT_THAT(maybe_sub, VariantWith<AnyFailure>(VariantWith<AlreadyExistsError>(_)));
    }
    // MemoryError due to nullptr
    {
        EXPECT_CALL(transport_mock_, makeMessageRxSession(_))  //
            .WillOnce(Return(nullptr));

        auto maybe_sub = presentation.makeSubscriber<Message>(Message::_traits_::FixedPortId);
        EXPECT_THAT(maybe_sub, VariantWith<AnyFailure>(VariantWith<MemoryError>(_)));
    }
    // Emulate that there is no memory available for the `SubscriberImpl`.
    {
        using UniquePtrSpec = MessageRxSessionMock::RefWrapper::Spec;

        StrictMock<MessageRxSessionMock> msg_rx_session_mock;
        EXPECT_CALL(msg_rx_session_mock, deinit()).Times(1);

        EXPECT_CALL(transport_mock_, makeMessageRxSession(_))  //
            .WillOnce(Invoke([&](const auto&) {
                return libcyphal::detail::makeUniquePtr<UniquePtrSpec>(mr_, msg_rx_session_mock);
            }));
        EXPECT_CALL(mr_mock, do_allocate(sizeof(detail::SubscriberImpl), _)).WillOnce(Return(nullptr));

        auto maybe_sub = presentation.makeSubscriber<Message>(Message::_traits_::FixedPortId);
        EXPECT_THAT(maybe_sub, VariantWith<AnyFailure>(VariantWith<MemoryError>(_)));
    }
}

// NOLINTEND(cppcoreguidelines-avoid-magic-numbers, readability-magic-numbers)

}  // namespace
