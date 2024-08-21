/// @copyright
/// Copyright (C) OpenCyphal Development Team  <opencyphal.org>
/// Copyright Amazon.com Inc. or its affiliates.
/// SPDX-License-Identifier: MIT

#include "../cetl_gtest_helpers.hpp"  // NOLINT(misc-include-cleaner)
#include "../gtest_helpers.hpp"       // NOLINT(misc-include-cleaner)
#include "../tracking_memory_resource.hpp"
#include "../transport/msg_sessions_mock.hpp"
#include "../transport/scattered_buffer_storage_mock.hpp"
#include "../transport/transport_mock.hpp"
#include "../virtual_time_scheduler.hpp"
#include "my_custom/bar_1_0.hpp"

#include <cetl/pf17/cetlpf.hpp>
#include <libcyphal/presentation/presentation.hpp>
#include <libcyphal/presentation/subscriber.hpp>
#include <libcyphal/transport/msg_sessions.hpp>
#include <libcyphal/transport/types.hpp>
#include <libcyphal/types.hpp>

#include <uavcan/node/Health_1_0.hpp>
#include <uavcan/node/Heartbeat_1_0.hpp>
#include <uavcan/node/Mode_1_0.hpp>

#include <gmock/gmock.h>
#include <gtest/gtest.h>

#include <array>
#include <cstdint>
#include <cstring>
#include <functional>
#include <string>
#include <tuple>
#include <type_traits>
#include <utility>
#include <vector>

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
using testing::NiceMock;
using testing::Optional;
using testing::StrictMock;
using testing::ElementsAre;
using testing::VariantWith;
using testing::UnorderedElementsAre;

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
    static_assert(!std::is_copy_constructible<Subscriber<Message>>::value, "Should not be copy constructible.");
    static_assert(std::is_move_assignable<Subscriber<Message>>::value, "Should be move assignable.");
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

    cetl::optional<Subscriber<Message>> sub1b{std::move(sub1a)};

    auto maybe_sub2 = presentation.makeSubscriber<Message>(Message::_traits_::FixedPortId);
    ASSERT_THAT(maybe_sub2, VariantWith<Subscriber<Message>>(_));
    auto sub2 = cetl::get<Subscriber<Message>>(std::move(maybe_sub2));

    sub1b = std::move(sub2);

    EXPECT_CALL(msg_rx_session_mock, deinit()).Times(1);
    sub1b.reset();
    testing::Mock::VerifyAndClearExpectations(&msg_rx_session_mock);
}

TEST_F(TestSubscriber, onReceive)
{
    using Message = uavcan::node::Heartbeat_1_0;

    Presentation presentation{mr_, scheduler_, transport_mock_};

    IMessageRxSession::OnReceiveCallback::Function msg_rx_cb_fn;

    StrictMock<MessageRxSessionMock> msg_rx_session_mock;
    EXPECT_CALL(msg_rx_session_mock, getParams())  //
        .WillOnce(Return(MessageRxParams{0, Message::_traits_::FixedPortId}));
    EXPECT_CALL(msg_rx_session_mock, setOnReceiveCallback(_))  //
        .WillOnce(Invoke([&](auto&& cb_fn) {                   //
            msg_rx_cb_fn = std::forward<IMessageRxSession::OnReceiveCallback::Function>(cb_fn);
        }));

    EXPECT_CALL(transport_mock_, makeMessageRxSession(_))  //
        .WillOnce(Invoke([&](const auto&) {
            return libcyphal::detail::makeUniquePtr<MessageRxSessionMock::RefWrapper::Spec>(mr_, msg_rx_session_mock);
        }));

    auto maybe_sub = presentation.makeSubscriber<Message>(Message::_traits_::FixedPortId);
    ASSERT_THAT(maybe_sub, VariantWith<Subscriber<Message>>(_));
    auto subscriber = cetl::get<Subscriber<Message>>(std::move(maybe_sub));

    EXPECT_TRUE(msg_rx_cb_fn);

    Message test_message{7, {uavcan::node::Health_1_0::WARNING}, {uavcan::node::Mode_1_0::MAINTENANCE}, 42};

    NiceMock<ScatteredBufferStorageMock> storage_mock;
    ScatteredBufferStorageMock::Wrapper  storage{&storage_mock};
    EXPECT_CALL(storage_mock, size()).WillRepeatedly(Return(Message::_traits_::SerializationBufferSizeBytes));
    EXPECT_CALL(storage_mock, copy(_, _, _))                           //
        .WillRepeatedly(Invoke([&](auto, auto* const dst, auto len) {  //
            //
            std::array<std::uint8_t, Message::_traits_::SerializationBufferSizeBytes> buffer{};
            const auto result = serialize(test_message, buffer);
            const auto size   = std::min(result.value(), len);
            (void) std::memmove(dst, &test_message, size);
            return size;
        }));

    std::vector<std::tuple<TimePoint, TransferId, std::uint32_t>> messages;
    subscriber.setOnReceiveCallback([&](const auto& arg) {
        //
        messages.emplace_back(arg.approx_now, arg.metadata.rx_meta.base.transfer_id, arg.message.uptime);
        EXPECT_THAT(arg.metadata.rx_meta.base.priority, Priority::Fast);
        EXPECT_THAT(arg.metadata.publisher_node_id, Optional(NodeId{0x31}));
        EXPECT_THAT(arg.message.health.value, uavcan::node::Health_1_0::WARNING);
        EXPECT_THAT(arg.message.mode.value, uavcan::node::Mode_1_0::MAINTENANCE);
        EXPECT_THAT(arg.message.vendor_specific_status_code, 42);
    });

    MessageRxTransfer transfer{{{{123, Priority::Fast}, {}}, NodeId{0x31}}, ScatteredBuffer{std::move(storage)}};

    scheduler_.scheduleAt(1s, [&](const auto&) {
        //
        transfer.metadata.rx_meta.timestamp = now();
        msg_rx_cb_fn({transfer});
    });
    scheduler_.scheduleAt(2s, [&](const auto&) {
        //
        test_message.uptime++;
        transfer.metadata.rx_meta.base.transfer_id++;
        transfer.metadata.rx_meta.timestamp = now();
        msg_rx_cb_fn({transfer});
    });
    scheduler_.scheduleAt(3s, [&](const auto&) {
        //
        test_message.uptime++;
        transfer.metadata.rx_meta.base.transfer_id++;
        transfer.metadata.rx_meta.timestamp = now();
        msg_rx_cb_fn({transfer});
    });
    scheduler_.scheduleAt(4s, [&](const auto&) {
        //
        // Cancel callback, so there should be no msg reception #4.
        subscriber.setOnReceiveCallback({});
        msg_rx_cb_fn({transfer});
    });
    scheduler_.spinFor(10s);

    EXPECT_THAT(messages,
                ElementsAre(std::make_tuple(TimePoint{1s}, 123, 7),
                            std::make_tuple(TimePoint{2s}, 124, 8),
                            std::make_tuple(TimePoint{3s}, 125, 9)));
    EXPECT_CALL(msg_rx_session_mock, deinit()).Times(1);
}

TEST_F(TestSubscriber, onReceive_deserialize_failure)
{
    using Message = my_custom::bar_1_0;

    Presentation presentation{mr_, scheduler_, transport_mock_};

    IMessageRxSession::OnReceiveCallback::Function msg_rx_cb_fn;

    StrictMock<MessageRxSessionMock> msg_rx_session_mock;
    EXPECT_CALL(msg_rx_session_mock, getParams())  //
        .WillOnce(Return(MessageRxParams{0, 0x123}));
    EXPECT_CALL(msg_rx_session_mock, setOnReceiveCallback(_))  //
        .WillOnce(Invoke([&](auto&& cb_fn) {                   //
            msg_rx_cb_fn = std::forward<IMessageRxSession::OnReceiveCallback::Function>(cb_fn);
        }));

    EXPECT_CALL(transport_mock_, makeMessageRxSession(_))  //
        .WillOnce(Invoke([&](const auto&) {
            return libcyphal::detail::makeUniquePtr<MessageRxSessionMock::RefWrapper::Spec>(mr_, msg_rx_session_mock);
        }));

    auto maybe_sub = presentation.makeSubscriber<Message>(0x123);
    ASSERT_THAT(maybe_sub, VariantWith<Subscriber<Message>>(_));
    auto subscriber = cetl::get<Subscriber<Message>>(std::move(maybe_sub));

    EXPECT_TRUE(msg_rx_cb_fn);

    NiceMock<ScatteredBufferStorageMock> storage_mock;
    ScatteredBufferStorageMock::Wrapper  storage{&storage_mock};
    EXPECT_CALL(storage_mock, size()).WillRepeatedly(Return(Message::_traits_::SerializationBufferSizeBytes));
    EXPECT_CALL(storage_mock, copy(_, _, _))                           //
        .WillRepeatedly(Invoke([&](auto, auto* const dst, auto len) {  //
            //
            // This will cause SerializationBadArrayLength
            std::array<std::uint8_t, 1> buffer{Message::_traits_::SerializationBufferSizeBytes};
            const auto                  size = std::min(buffer.size(), len);
            (void) std::memmove(dst, buffer.data(), size);
            return size;
        }));

    std::vector<std::tuple<TimePoint, TransferId>> messages;
    subscriber.setOnReceiveCallback([&](const auto& arg) {
        //
        messages.emplace_back(arg.approx_now, arg.metadata.rx_meta.base.transfer_id);
        EXPECT_THAT(arg.metadata.rx_meta.base.priority, Priority::Fast);
        EXPECT_THAT(arg.metadata.publisher_node_id, Optional(NodeId{0x31}));
    });

    MessageRxTransfer transfer{{{{13, Priority::Fast}, {}}, NodeId{0x31}}, ScatteredBuffer{std::move(storage)}};

    scheduler_.scheduleAt(1s, [&](const auto&) {
        //
        transfer.metadata.rx_meta.timestamp = now();
        msg_rx_cb_fn({transfer});
    });
    scheduler_.spinFor(10s);

    EXPECT_THAT(messages, IsEmpty());
    EXPECT_CALL(msg_rx_session_mock, deinit()).Times(1);
}

TEST_F(TestSubscriber, onReceive_release_same_subject_subscriber_during_callback)
{
    using Message = my_custom::bar_1_0;

    Presentation presentation{mr_, scheduler_, transport_mock_};

    IMessageRxSession::OnReceiveCallback::Function msg_rx_cb_fn;

    StrictMock<MessageRxSessionMock> msg_rx_session_mock;
    EXPECT_CALL(msg_rx_session_mock, getParams())  //
        .WillOnce(Return(MessageRxParams{0, 0x123}));
    EXPECT_CALL(msg_rx_session_mock, setOnReceiveCallback(_))  //
        .WillOnce(Invoke([&](auto&& cb_fn) {                   //
            msg_rx_cb_fn = std::forward<IMessageRxSession::OnReceiveCallback::Function>(cb_fn);
        }));

    EXPECT_CALL(transport_mock_, makeMessageRxSession(_))  //
        .WillOnce(Invoke([&](const auto&) {
            return libcyphal::detail::makeUniquePtr<MessageRxSessionMock::RefWrapper::Spec>(mr_, msg_rx_session_mock);
        }));

    auto maybe_sub_a = presentation.makeSubscriber<Message>(0x123);
    ASSERT_THAT(maybe_sub_a, VariantWith<Subscriber<Message>>(_));
    cetl::optional<Subscriber<Message>> sub_a = cetl::get<Subscriber<Message>>(std::move(maybe_sub_a));

    auto maybe_sub_b = presentation.makeSubscriber<Message>(0x123);
    ASSERT_THAT(maybe_sub_b, VariantWith<Subscriber<Message>>(_));
    cetl::optional<Subscriber<Message>> sub_b = cetl::get<Subscriber<Message>>(std::move(maybe_sub_b));

    std::vector<std::tuple<std::string, TimePoint, TransferId>> messages;

    const std::function<void(const Subscriber<Message>::OnReceiveCallback::Arg&)> on_sub_a_receive_logic{
        [&](const Subscriber<Message>::OnReceiveCallback::Arg& arg) {
            //
            messages.emplace_back("A", arg.approx_now, arg.metadata.rx_meta.base.transfer_id);
            if (arg.approx_now == TimePoint{2s})
            {
                // Release other "next" subscriber "B" while callback iteration is in progress.
                sub_b.reset();
            }
            if (arg.approx_now == TimePoint{3s})
            {
                // Release self subscriber "A" while callback iteration is in progress.
                // This will be the last subscriber to be released, so we expect RX session `deinit` to be called.
                EXPECT_CALL(msg_rx_session_mock, deinit()).Times(1);
                sub_a.reset();
            }
        }};

    sub_a->setOnReceiveCallback([&](const auto& arg) {
        //
        on_sub_a_receive_logic(arg);
    });
    sub_b->setOnReceiveCallback([&](const auto& arg) {
        //
        messages.emplace_back("B", arg.approx_now, arg.metadata.rx_meta.base.transfer_id);
    });

    MessageRxTransfer transfer{{{{42, Priority::Fast}, {}}, NodeId{0x31}}, {}};

    scheduler_.scheduleAt(1s, [&](const auto&) {
        //
        transfer.metadata.rx_meta.base.transfer_id++;
        transfer.metadata.rx_meta.timestamp = now();
        msg_rx_cb_fn({transfer});
    });
    scheduler_.scheduleAt(2s, [&](const auto&) {
        //
        transfer.metadata.rx_meta.base.transfer_id++;
        transfer.metadata.rx_meta.timestamp = now();
        msg_rx_cb_fn({transfer});
    });
    scheduler_.scheduleAt(3s, [&](const auto&) {
        //
        transfer.metadata.rx_meta.base.transfer_id++;
        transfer.metadata.rx_meta.timestamp = now();
        msg_rx_cb_fn({transfer});
    });
    scheduler_.scheduleAt(4s, [&](const auto&) {
        //
        transfer.metadata.rx_meta.base.transfer_id++;
        transfer.metadata.rx_meta.timestamp = now();
        msg_rx_cb_fn({transfer});
    });
    scheduler_.spinFor(10s);

    EXPECT_THAT(messages,
                ElementsAre(std::make_tuple("A", TimePoint{1s}, 43),
                            std::make_tuple("B", TimePoint{1s}, 43),
                            std::make_tuple("A", TimePoint{2s}, 44),
                            std::make_tuple("A", TimePoint{3s}, 45)));
}

TEST_F(TestSubscriber, onReceive_move_same_subject_subscriber_during_callback)
{
    using Message = my_custom::bar_1_0;

    Presentation presentation{mr_, scheduler_, transport_mock_};

    IMessageRxSession::OnReceiveCallback::Function msg_rx_cb_fn;

    StrictMock<MessageRxSessionMock> msg_rx_session_mock;
    EXPECT_CALL(msg_rx_session_mock, getParams())  //
        .WillOnce(Return(MessageRxParams{0, 0x123}));
    EXPECT_CALL(msg_rx_session_mock, setOnReceiveCallback(_))  //
        .WillOnce(Invoke([&](auto&& cb_fn) {                   //
            msg_rx_cb_fn = std::forward<IMessageRxSession::OnReceiveCallback::Function>(cb_fn);
        }));

    EXPECT_CALL(transport_mock_, makeMessageRxSession(_))  //
        .WillOnce(Invoke([&](const auto&) {
            return libcyphal::detail::makeUniquePtr<MessageRxSessionMock::RefWrapper::Spec>(mr_, msg_rx_session_mock);
        }));

    auto maybe_sub1 = presentation.makeSubscriber<Message>(0x123);
    ASSERT_THAT(maybe_sub1, VariantWith<Subscriber<Message>>(_));
    cetl::optional<Subscriber<Message>> sub1 = cetl::get<Subscriber<Message>>(std::move(maybe_sub1));

    auto maybe_sub2 = presentation.makeSubscriber<Message>(0x123);
    ASSERT_THAT(maybe_sub2, VariantWith<Subscriber<Message>>(_));
    cetl::optional<Subscriber<Message>> sub2a = cetl::get<Subscriber<Message>>(std::move(maybe_sub2));
    cetl::optional<Subscriber<Message>> sub2b;

    std::vector<std::tuple<std::string, TimePoint, TransferId>> messages;

    sub1->setOnReceiveCallback([&](const auto& arg) {
        //
        messages.emplace_back("#1", arg.approx_now, arg.metadata.rx_meta.base.transfer_id);
        // This should trigger the "next" subscriber "#2" to be moved.
        std::swap(sub2a, sub2b);
    });
    sub2a->setOnReceiveCallback([&](const auto& arg) {
        //
        messages.emplace_back("#2", arg.approx_now, arg.metadata.rx_meta.base.transfer_id);
        // This should trigger the "self" subscriber "#2" to be moved.
        std::swap(sub2a, sub2b);
    });

    MessageRxTransfer transfer{{{{42, Priority::Fast}, {}}, NodeId{0x31}}, {}};

    scheduler_.scheduleAt(1s, [&](const auto&) {
        //
        transfer.metadata.rx_meta.base.transfer_id++;
        transfer.metadata.rx_meta.timestamp = now();
        msg_rx_cb_fn({transfer});
    });
    scheduler_.scheduleAt(2s, [&](const auto&) {
        //
        transfer.metadata.rx_meta.base.transfer_id++;
        transfer.metadata.rx_meta.timestamp = now();
        msg_rx_cb_fn({transfer});
    });
    scheduler_.spinFor(10s);

    EXPECT_THAT(messages,
                ElementsAre(std::make_tuple("#1", TimePoint{1s}, 43),
                            std::make_tuple("#2", TimePoint{1s}, 43),
                            std::make_tuple("#1", TimePoint{2s}, 44),
                            std::make_tuple("#2", TimePoint{2s}, 44)));
    EXPECT_CALL(msg_rx_session_mock, deinit()).Times(1);
}

TEST_F(TestSubscriber, onReceive_append_same_subject_subscriber_during_callback)
{
    using Message = my_custom::bar_1_0;

    Presentation presentation{mr_, scheduler_, transport_mock_};

    IMessageRxSession::OnReceiveCallback::Function msg_rx_cb_fn;

    StrictMock<MessageRxSessionMock> msg_rx_session_mock;
    EXPECT_CALL(msg_rx_session_mock, getParams())  //
        .WillOnce(Return(MessageRxParams{0, 0x123}));
    EXPECT_CALL(msg_rx_session_mock, setOnReceiveCallback(_))  //
        .WillOnce(Invoke([&](auto&& cb_fn) {                   //
            msg_rx_cb_fn = std::forward<IMessageRxSession::OnReceiveCallback::Function>(cb_fn);
        }));

    EXPECT_CALL(transport_mock_, makeMessageRxSession(_))  //
        .WillOnce(Invoke([&](const auto&) {
            return libcyphal::detail::makeUniquePtr<MessageRxSessionMock::RefWrapper::Spec>(mr_, msg_rx_session_mock);
        }));

    auto maybe_sub1 = presentation.makeSubscriber<Message>(0x123);
    ASSERT_THAT(maybe_sub1, VariantWith<Subscriber<Message>>(_));
    cetl::optional<Subscriber<Message>> sub1 = cetl::get<Subscriber<Message>>(std::move(maybe_sub1));

    auto maybe_sub2 = presentation.makeSubscriber<Message>(0x123);
    ASSERT_THAT(maybe_sub2, VariantWith<Subscriber<Message>>(_));
    cetl::optional<Subscriber<Message>> sub2 = cetl::get<Subscriber<Message>>(std::move(maybe_sub2));

    cetl::optional<Subscriber<Message>> sub3;

    std::vector<std::tuple<std::string, TimePoint, TransferId>> messages;

    auto append_sub3 = [&] {
        //
        auto maybe_sub3 = presentation.makeSubscriber<Message>(0x123);
        ASSERT_THAT(maybe_sub3, VariantWith<Subscriber<Message>>(_));
        sub3 = cetl::get<Subscriber<Message>>(std::move(maybe_sub3));
        sub3->setOnReceiveCallback([&](const auto& arg) {
            //
            messages.emplace_back("#3", arg.approx_now, arg.metadata.rx_meta.base.transfer_id);
        });
    };
    sub1->setOnReceiveCallback([&](const auto& arg) {
        //
        messages.emplace_back("#1", arg.approx_now, arg.metadata.rx_meta.base.transfer_id);
        if (arg.approx_now == TimePoint{1s})
        {
            append_sub3();
        }
    });
    sub2->setOnReceiveCallback([&](const auto& arg) {
        //
        messages.emplace_back("#2", arg.approx_now, arg.metadata.rx_meta.base.transfer_id);
    });

    MessageRxTransfer transfer{{{{42, Priority::Fast}, {}}, NodeId{0x31}}, {}};

    scheduler_.scheduleAt(1s, [&](const auto&) {
        //
        transfer.metadata.rx_meta.base.transfer_id++;
        transfer.metadata.rx_meta.timestamp = now();
        msg_rx_cb_fn({transfer});
    });
    scheduler_.scheduleAt(2s, [&](const auto&) {
        //
        transfer.metadata.rx_meta.base.transfer_id++;
        transfer.metadata.rx_meta.timestamp = now();
        msg_rx_cb_fn({transfer});
    });
    scheduler_.spinFor(10s);

    EXPECT_THAT(messages,
                ElementsAre(std::make_tuple("#1", TimePoint{1s}, 43),
                            std::make_tuple("#2", TimePoint{1s}, 43),
                            std::make_tuple("#1", TimePoint{2s}, 44),
                            std::make_tuple("#2", TimePoint{2s}, 44),
                            std::make_tuple("#3", TimePoint{2s}, 44)));
    EXPECT_CALL(msg_rx_session_mock, deinit()).Times(1);
}

TEST_F(TestSubscriber, onReceive_different_type_deserializers_on_same_subject)
{
    using BarMsg       = my_custom::bar_1_0;
    using HeartbeatMsg = uavcan::node::Heartbeat_1_0;

    Presentation presentation{mr_, scheduler_, transport_mock_};

    IMessageRxSession::OnReceiveCallback::Function msg_rx_cb_fn;

    StrictMock<MessageRxSessionMock> msg_rx_session_mock;
    EXPECT_CALL(msg_rx_session_mock, getParams())  //
        .WillOnce(Return(MessageRxParams{0, 0x123}));
    EXPECT_CALL(msg_rx_session_mock, setOnReceiveCallback(_))  //
        .WillOnce(Invoke([&](auto&& cb_fn) {                   //
            msg_rx_cb_fn = std::forward<IMessageRxSession::OnReceiveCallback::Function>(cb_fn);
        }));

    EXPECT_CALL(transport_mock_, makeMessageRxSession(_))  //
        .WillOnce(Invoke([&](const auto&) {
            return libcyphal::detail::makeUniquePtr<MessageRxSessionMock::RefWrapper::Spec>(mr_, msg_rx_session_mock);
        }));

    std::vector<std::tuple<std::string, TimePoint, TransferId>> messages;

    constexpr PortId port_id        = 0x123;
    auto             maybe_bar_sub1 = presentation.makeSubscriber<BarMsg>(port_id);
    ASSERT_THAT(maybe_bar_sub1, VariantWith<Subscriber<BarMsg>>(_));
    auto bar_sub1 = cetl::get<Subscriber<BarMsg>>(std::move(maybe_bar_sub1));
    bar_sub1.setOnReceiveCallback([&](const auto& arg) {
        //
        messages.emplace_back("Bar_#1", arg.approx_now, arg.metadata.rx_meta.base.transfer_id);
    });
    auto maybe_bar_sub2 = presentation.makeSubscriber<BarMsg>(port_id);
    ASSERT_THAT(maybe_bar_sub2, VariantWith<Subscriber<BarMsg>>(_));
    auto bar_sub2 = cetl::get<Subscriber<BarMsg>>(std::move(maybe_bar_sub2));
    bar_sub2.setOnReceiveCallback([&](const auto& arg) {
        //
        messages.emplace_back("Bar_#2", arg.approx_now, arg.metadata.rx_meta.base.transfer_id);
    });
    auto maybe_hb_sub = presentation.makeSubscriber<HeartbeatMsg>(port_id);
    ASSERT_THAT(maybe_hb_sub, VariantWith<Subscriber<HeartbeatMsg>>(_));
    auto hb_sub = cetl::get<Subscriber<HeartbeatMsg>>(std::move(maybe_hb_sub));
    hb_sub.setOnReceiveCallback([&](const auto& arg) {
        //
        messages.emplace_back("Heartbeat", arg.approx_now, arg.metadata.rx_meta.base.transfer_id);
    });

    NiceMock<ScatteredBufferStorageMock> storage_mock;
    ScatteredBufferStorageMock::Wrapper  storage{&storage_mock};
    // We have 3 subscribers, but only 2 different types of messages ("bar" and "heartbeat"),
    // so only 2 times de-serializations (which involves `copy` as well) should be performed.
    EXPECT_CALL(storage_mock, copy(_, _, _)).Times(2).WillRepeatedly(Return(0));
    MessageRxTransfer transfer{{{{42, Priority::Fast}, {}}, NodeId{0x31}}, ScatteredBuffer{std::move(storage)}};

    scheduler_.scheduleAt(1s, [&](const auto&) {
        //
        transfer.metadata.rx_meta.base.transfer_id++;
        transfer.metadata.rx_meta.timestamp = now();
        msg_rx_cb_fn({transfer});
    });
    scheduler_.spinFor(10s);

    EXPECT_THAT(messages,
                UnorderedElementsAre(std::make_tuple("Bar_#1", TimePoint{1s}, 43),
                                     std::make_tuple("Bar_#2", TimePoint{1s}, 43),
                                     std::make_tuple("Heartbeat", TimePoint{1s}, 43)));
    EXPECT_CALL(msg_rx_session_mock, deinit()).Times(1);
}

// NOLINTEND(cppcoreguidelines-avoid-magic-numbers, readability-magic-numbers)

}  // namespace
