/// @copyright
/// Copyright (C) OpenCyphal Development Team  <opencyphal.org>
/// Copyright Amazon.com Inc. or its affiliates.
/// SPDX-License-Identifier: MIT

#include "memory_resource_mock.hpp"
#include "tracking_memory_resource.hpp"
#include "transport/msg_sessions_mock.hpp"
#include "transport/svc_sessions_mock.hpp"
#include "transport/transport_gtest_helpers.hpp"
#include "transport/transport_mock.hpp"
#include "virtual_time_scheduler.hpp"

#include <cetl/pf17/cetlpf.hpp>
#include <libcyphal/errors.hpp>
#include <libcyphal/presentation/client.hpp>
#include <libcyphal/presentation/client_impl.hpp>
#include <libcyphal/presentation/presentation.hpp>
#include <libcyphal/presentation/publisher.hpp>
#include <libcyphal/presentation/publisher_impl.hpp>
#include <libcyphal/presentation/server.hpp>
#include <libcyphal/presentation/subscriber.hpp>
#include <libcyphal/presentation/subscriber_impl.hpp>
#include <libcyphal/transport/errors.hpp>
#include <libcyphal/transport/msg_sessions.hpp>
#include <libcyphal/transport/svc_sessions.hpp>
#include <libcyphal/transport/transfer_id_generators.hpp>
#include <libcyphal/transport/types.hpp>
#include <libcyphal/types.hpp>

#include <nunavut/support/serialization.hpp>
#include <uavcan/node/GetInfo_1_0.hpp>
#include <uavcan/node/Heartbeat_1_0.hpp>

#include <gmock/gmock.h>
#include <gtest/gtest.h>

#include <algorithm>
#include <cstddef>
#include <cstdint>
#include <limits>

namespace
{

using libcyphal::TimePoint;
using libcyphal::UniquePtr;
using libcyphal::MemoryError;
using namespace libcyphal::presentation;  // NOLINT This our main concern here in the unit tests.
using namespace libcyphal::transport;     // NOLINT This our main concern here in the unit tests.

using testing::_;
using testing::Eq;
using testing::Invoke;
using testing::Return;
using testing::IsEmpty;
using testing::StrictMock;
using testing::VariantWith;

// NOLINTBEGIN(cppcoreguidelines-avoid-magic-numbers, readability-magic-numbers)

namespace Custom
{

struct PubMessage final
{
    struct _traits_
    {
        static constexpr bool          HasFixedPortID               = true;
        static constexpr std::uint16_t FixedPortId                  = 147U;
        static constexpr std::size_t   SerializationBufferSizeBytes = sizeof(std::uint64_t);
    };

    std::uint64_t id{};  // NOLINT

};  // PubMessage

struct SubMessage final
{
    struct _traits_
    {
        static constexpr bool          HasFixedPortID = true;
        static constexpr std::uint16_t FixedPortId    = 147U;
        static constexpr std::size_t   ExtentBytes    = sizeof(std::uint64_t);
    };

    std::uint64_t id{};  // NOLINT

};  // SubMessage

struct Service final
{
    struct _traits_
    {
        static constexpr bool IsService = true;
    };

    struct Request final
    {
        struct _traits_
        {
            static constexpr bool          HasFixedPortID = true;
            static constexpr std::uint16_t FixedPortId    = 147U;
            static constexpr std::size_t   ExtentBytes    = sizeof(std::uint64_t) * 2;
        };

        std::uint64_t id{};  // NOLINT

    };  // Request

    struct Response final
    {
        struct _traits_
        {
            static constexpr std::size_t ExtentBytes                  = sizeof(std::uint64_t) * 3;
            static constexpr std::size_t SerializationBufferSizeBytes = sizeof(std::uint64_t);
        };

        std::uint64_t id{};  // NOLINT

    };  // Response

};  // Service

template <typename T>
nunavut::support::SerializeResult serialize(const T& obj, nunavut::support::bitspan out_buffer)
{
    const auto result = out_buffer.setUxx(obj.id, 64U);
    if (not result)
    {
        return -result.error();
    }
    out_buffer.add_offset(64U);
    return out_buffer.offset_bytes_ceil();
}

template <typename T>
nunavut::support::SerializeResult deserialize(T& obj, nunavut::support::const_bitspan in_buffer)
{
    obj.id = in_buffer.getU64(64U);
    in_buffer.add_offset(64U);
    return {std::min<std::size_t>(64U, in_buffer.size()) / 8U};
}

}  // namespace Custom

class TestPresentation : public testing::Test
{
protected:
    using UniquePtrMsgRxSpec = MessageRxSessionMock::RefWrapper::Spec;
    using UniquePtrMsgTxSpec = MessageTxSessionMock::RefWrapper::Spec;
    using UniquePtrReqRxSpec = RequestRxSessionMock::RefWrapper::Spec;
    using UniquePtrReqTxSpec = RequestTxSessionMock::RefWrapper::Spec;
    using UniquePtrResRxSpec = ResponseRxSessionMock::RefWrapper::Spec;
    using UniquePtrResTxSpec = ResponseTxSessionMock::RefWrapper::Spec;

    void SetUp() override
    {
        EXPECT_CALL(transport_mock_, getProtocolParams())
            .WillRepeatedly(Return(ProtocolParams{std::numeric_limits<TransferId>::max(), 0, 0}));
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
    constexpr MessageTxParams        tx_params{Message::_traits_::FixedPortId};
    EXPECT_CALL(msg_tx_session_mock, getParams()).WillOnce(Return(tx_params));

    EXPECT_CALL(transport_mock_, makeMessageTxSession(MessageTxParamsEq(tx_params)))  //
        .WillOnce(Invoke([&](const auto&) {                                           //
            return libcyphal::detail::makeUniquePtr<UniquePtrMsgTxSpec>(mr_, msg_tx_session_mock);
        }));

    auto maybe_pub1 = presentation.makePublisher<Message>(tx_params.subject_id);
    EXPECT_THAT(maybe_pub1, VariantWith<Publisher<Message>>(_));

    auto maybe_pub2 = presentation.makePublisher<Message>(tx_params.subject_id);
    EXPECT_THAT(maybe_pub2, VariantWith<Publisher<Message>>(_));

    EXPECT_CALL(msg_tx_session_mock, deinit()).Times(1);
}

TEST_F(TestPresentation, makePublisher_custom)
{
    using Message = Custom::PubMessage;

    Presentation presentation{mr_, scheduler_, transport_mock_};

    StrictMock<MessageTxSessionMock> msg_tx_session_mock;
    constexpr MessageTxParams        tx_params{Message::_traits_::FixedPortId};
    EXPECT_CALL(msg_tx_session_mock, getParams()).WillOnce(Return(tx_params));

    EXPECT_CALL(transport_mock_, makeMessageTxSession(MessageTxParamsEq(tx_params)))  //
        .WillOnce(Invoke([&](const auto&) {                                           //
            return libcyphal::detail::makeUniquePtr<UniquePtrMsgTxSpec>(mr_, msg_tx_session_mock);
        }));

    auto maybe_pub = presentation.makePublisher<Message>();
    ASSERT_THAT(maybe_pub, VariantWith<Publisher<Message>>(_));
    auto publisher_copy = cetl::get<Publisher<Message>>(maybe_pub);

    EXPECT_CALL(msg_tx_session_mock, send(_, _)).WillOnce(Return(cetl::nullopt));
    EXPECT_THAT(publisher_copy.publish(now(), {}), Eq(cetl::nullopt));

    EXPECT_CALL(msg_tx_session_mock, deinit()).Times(1);
}

TEST_F(TestPresentation, makePublisher_raw)
{
    Presentation presentation{mr_, scheduler_, transport_mock_};

    StrictMock<MessageTxSessionMock> msg_tx_session_mock;
    constexpr MessageTxParams        tx_params{147};
    EXPECT_CALL(msg_tx_session_mock, getParams()).WillOnce(Return(tx_params));

    EXPECT_CALL(transport_mock_, makeMessageTxSession(MessageTxParamsEq(tx_params)))  //
        .WillOnce(Invoke([&](const auto&) {                                           //
            return libcyphal::detail::makeUniquePtr<UniquePtrMsgTxSpec>(mr_, msg_tx_session_mock);
        }));

    auto maybe_pub = presentation.makePublisher<void>(tx_params.subject_id);
    EXPECT_THAT(maybe_pub, VariantWith<Publisher<void>>(_));

    EXPECT_CALL(msg_tx_session_mock, deinit()).Times(1);
}

TEST_F(TestPresentation, makePublisher_with_failure)
{
    using Message = uavcan::node::Heartbeat_1_0;

    StrictMock<MemoryResourceMock> mr_mock;
    mr_mock.redirectExpectedCallsTo(mr_);

    Presentation presentation{mr_mock, scheduler_, transport_mock_};

    constexpr MessageTxParams tx_params{Message::_traits_::FixedPortId};

    // AlreadyExistsError
    {
        EXPECT_CALL(transport_mock_, makeMessageTxSession(MessageTxParamsEq(tx_params)))  //
            .WillOnce(Return(AlreadyExistsError{}));

        auto maybe_pub = presentation.makePublisher<Message>(tx_params.subject_id);
        EXPECT_THAT(maybe_pub, VariantWith<Presentation::MakeFailure>(VariantWith<AlreadyExistsError>(_)));
    }
    // MemoryError due to nullptr
    {
        EXPECT_CALL(transport_mock_, makeMessageTxSession(MessageTxParamsEq(tx_params)))  //
            .WillOnce(Return(nullptr));

        auto maybe_pub = presentation.makePublisher<Message>(tx_params.subject_id);
        EXPECT_THAT(maybe_pub, VariantWith<Presentation::MakeFailure>(VariantWith<MemoryError>(_)));
    }
    // Emulate that there is no memory available for the `PublisherImpl`.
    {
        using PublisherImpl = libcyphal::presentation::detail::PublisherImpl;

        StrictMock<MessageTxSessionMock> msg_tx_session_mock;
        EXPECT_CALL(msg_tx_session_mock, deinit()).Times(1);

        EXPECT_CALL(transport_mock_, makeMessageTxSession(MessageTxParamsEq(tx_params)))  //
            .WillOnce(Invoke([&](const auto&) {                                           //
                return libcyphal::detail::makeUniquePtr<UniquePtrMsgTxSpec>(mr_, msg_tx_session_mock);
            }));
        EXPECT_CALL(mr_mock, do_allocate(sizeof(PublisherImpl), _)).WillOnce(Return(nullptr));

        auto maybe_pub = presentation.makePublisher<Message>(tx_params.subject_id);
        EXPECT_THAT(maybe_pub, VariantWith<Presentation::MakeFailure>(VariantWith<MemoryError>(_)));
    }
}

TEST_F(TestPresentation, makeSubscriber)
{
    using Message = uavcan::node::Heartbeat_1_0;

    Presentation presentation{mr_, scheduler_, transport_mock_};

    StrictMock<MessageRxSessionMock> msg_rx_session_mock;
    constexpr MessageRxParams        rx_params{Message::_traits_::ExtentBytes, Message::_traits_::FixedPortId};
    EXPECT_CALL(msg_rx_session_mock, getParams()).WillOnce(Return(rx_params));
    EXPECT_CALL(msg_rx_session_mock, setOnReceiveCallback(_)).Times(1);

    EXPECT_CALL(transport_mock_, makeMessageRxSession(MessageRxParamsEq(rx_params)))  //
        .WillOnce(Invoke([&](const auto&) {                                           //
            return libcyphal::detail::makeUniquePtr<UniquePtrMsgRxSpec>(mr_, msg_rx_session_mock);
        }));

    auto maybe_sub1 = presentation.makeSubscriber<Message>(rx_params.subject_id);
    EXPECT_THAT(maybe_sub1, VariantWith<Subscriber<Message>>(_));

    auto maybe_sub2 = presentation.makeSubscriber<Message>([](const auto&) {});
    EXPECT_THAT(maybe_sub2, VariantWith<Subscriber<Message>>(_));

    EXPECT_CALL(msg_rx_session_mock, deinit()).Times(1);
}

TEST_F(TestPresentation, makeSubscriber_custom)
{
    using Message = Custom::SubMessage;

    Presentation presentation{mr_, scheduler_, transport_mock_};

    StrictMock<MessageRxSessionMock> msg_rx_session_mock;
    constexpr MessageRxParams        rx_params{Message::_traits_::ExtentBytes, Message::_traits_::FixedPortId};
    EXPECT_CALL(msg_rx_session_mock, getParams()).WillOnce(Return(rx_params));
    EXPECT_CALL(msg_rx_session_mock, setOnReceiveCallback(_)).Times(1);

    EXPECT_CALL(transport_mock_, makeMessageRxSession(MessageRxParamsEq(rx_params)))  //
        .WillOnce(Invoke([&](const auto&) {                                           //
            return libcyphal::detail::makeUniquePtr<UniquePtrMsgRxSpec>(mr_, msg_rx_session_mock);
        }));

    auto maybe_sub = presentation.makeSubscriber<Message>([](const auto&) {});
    EXPECT_THAT(maybe_sub, VariantWith<Subscriber<Message>>(_));

    EXPECT_CALL(msg_rx_session_mock, deinit()).Times(1);
}

TEST_F(TestPresentation, makeSubscriber_raw)
{
    Presentation presentation{mr_, scheduler_, transport_mock_};

    StrictMock<MessageRxSessionMock> msg_rx_session_mock;
    constexpr MessageRxParams        rx_params{0, 147};
    EXPECT_CALL(msg_rx_session_mock, getParams()).WillOnce(Return(rx_params));
    EXPECT_CALL(msg_rx_session_mock, setOnReceiveCallback(_)).Times(1);

    EXPECT_CALL(transport_mock_, makeMessageRxSession(MessageRxParamsEq(rx_params)))  //
        .WillOnce(Invoke([&](const auto&) {                                           //
            return libcyphal::detail::makeUniquePtr<UniquePtrMsgRxSpec>(mr_, msg_rx_session_mock);
        }));

    auto maybe_sub1 = presentation.makeSubscriber(rx_params.subject_id, rx_params.extent_bytes);
    EXPECT_THAT(maybe_sub1, VariantWith<Subscriber<void>>(_));

    auto maybe_sub2 = presentation.makeSubscriber(rx_params.subject_id, rx_params.extent_bytes, [](const auto&) {});
    EXPECT_THAT(maybe_sub2, VariantWith<Subscriber<void>>(_));

    EXPECT_CALL(msg_rx_session_mock, deinit()).Times(1);
}

TEST_F(TestPresentation, makeSubscriber_with_failure)
{
    using Message = uavcan::node::Heartbeat_1_0;

    StrictMock<MemoryResourceMock> mr_mock;
    mr_mock.redirectExpectedCallsTo(mr_);

    Presentation presentation{mr_mock, scheduler_, transport_mock_};

    constexpr MessageRxParams rx_params{Message::_traits_::ExtentBytes, Message::_traits_::FixedPortId};

    // AlreadyExistsError
    {
        EXPECT_CALL(transport_mock_, makeMessageRxSession(MessageRxParamsEq(rx_params)))  //
            .WillOnce(Return(AlreadyExistsError{}));

        auto maybe_sub = presentation.makeSubscriber<Message>(rx_params.subject_id);
        EXPECT_THAT(maybe_sub, VariantWith<Presentation::MakeFailure>(VariantWith<AlreadyExistsError>(_)));

        EXPECT_CALL(transport_mock_, makeMessageRxSession(MessageRxParamsEq(rx_params)))  //
            .WillOnce(Return(AlreadyExistsError{}));

        auto maybe_raw_sub = presentation.makeSubscriber(rx_params.subject_id, rx_params.extent_bytes);
        EXPECT_THAT(maybe_sub, VariantWith<Presentation::MakeFailure>(VariantWith<AlreadyExistsError>(_)));
    }
    // MemoryError due to nullptr
    {
        EXPECT_CALL(transport_mock_, makeMessageRxSession(MessageRxParamsEq(rx_params)))  //
            .WillOnce(Return(nullptr));

        auto maybe_sub = presentation.makeSubscriber<Message>(rx_params.subject_id);
        EXPECT_THAT(maybe_sub, VariantWith<Presentation::MakeFailure>(VariantWith<MemoryError>(_)));
    }
    // Emulate that there is no memory available for the `SubscriberImpl`.
    {
        using SubscriberImpl = libcyphal::presentation::detail::SubscriberImpl;

        StrictMock<MessageRxSessionMock> msg_rx_session_mock;
        EXPECT_CALL(msg_rx_session_mock, deinit()).Times(1);

        EXPECT_CALL(transport_mock_, makeMessageRxSession(MessageRxParamsEq(rx_params)))  //
            .WillOnce(Invoke([&](const auto&) {                                           //
                return libcyphal::detail::makeUniquePtr<UniquePtrMsgRxSpec>(mr_, msg_rx_session_mock);
            }));
        EXPECT_CALL(mr_mock, do_allocate(sizeof(SubscriberImpl), _)).WillOnce(Return(nullptr));

        auto maybe_sub = presentation.makeSubscriber(rx_params.subject_id, rx_params.extent_bytes);
        EXPECT_THAT(maybe_sub, VariantWith<Presentation::MakeFailure>(VariantWith<MemoryError>(_)));
    }
}

TEST_F(TestPresentation, makeServer)
{
    using Service = uavcan::node::GetInfo_1_0;

    Presentation presentation{mr_, scheduler_, transport_mock_};

    StrictMock<ResponseTxSessionMock> res_tx_session_mock;
    StrictMock<RequestRxSessionMock>  req_rx_session_mock;
    EXPECT_CALL(req_rx_session_mock, setOnReceiveCallback(_))  //
        .WillRepeatedly(Return());

    constexpr RequestRxParams rx_params{Service::Request::_traits_::ExtentBytes,
                                        Service::Request::_traits_::FixedPortId};
    EXPECT_CALL(transport_mock_, makeRequestRxSession(RequestRxParamsEq(rx_params)))  //
        .WillOnce(Invoke([&](const auto&) {                                           //
            return libcyphal::detail::makeUniquePtr<UniquePtrReqRxSpec>(mr_, req_rx_session_mock);
        }));
    constexpr ResponseTxParams tx_params{rx_params.service_id};
    EXPECT_CALL(transport_mock_, makeResponseTxSession(ResponseTxParamsEq(tx_params)))  //
        .WillOnce(Invoke([&](const auto&) {                                             //
            return libcyphal::detail::makeUniquePtr<UniquePtrResTxSpec>(mr_, res_tx_session_mock);
        }));

    auto maybe_server = presentation.makeServer<Service>();
    ASSERT_THAT(maybe_server, VariantWith<ServiceServer<Service>>(_));

    EXPECT_CALL(req_rx_session_mock, deinit()).Times(1);
    EXPECT_CALL(res_tx_session_mock, deinit()).Times(1);
}

TEST_F(TestPresentation, makeServer_custom)
{
    using Service = Custom::Service;

    Presentation presentation{mr_, scheduler_, transport_mock_};

    StrictMock<ResponseTxSessionMock> res_tx_session_mock;
    StrictMock<RequestRxSessionMock>  req_rx_session_mock;
    EXPECT_CALL(req_rx_session_mock, setOnReceiveCallback(_))  //
        .WillRepeatedly(Return());

    constexpr RequestRxParams rx_params{Service::Request::_traits_::ExtentBytes,
                                        Service::Request::_traits_::FixedPortId};
    EXPECT_CALL(transport_mock_, makeRequestRxSession(RequestRxParamsEq(rx_params)))  //
        .WillOnce(Invoke([&](const auto&) {                                           //
            return libcyphal::detail::makeUniquePtr<UniquePtrReqRxSpec>(mr_, req_rx_session_mock);
        }));
    constexpr ResponseTxParams tx_params{rx_params.service_id};
    EXPECT_CALL(transport_mock_, makeResponseTxSession(ResponseTxParamsEq(tx_params)))  //
        .WillOnce(Invoke([&](const auto&) {                                             //
            return libcyphal::detail::makeUniquePtr<UniquePtrResTxSpec>(mr_, res_tx_session_mock);
        }));

    auto maybe_server = presentation.makeServer<Service>([](const auto&, auto) {});
    ASSERT_THAT(maybe_server, VariantWith<ServiceServer<Service>>(_));

    EXPECT_CALL(req_rx_session_mock, deinit()).Times(1);
    EXPECT_CALL(res_tx_session_mock, deinit()).Times(1);
}

TEST_F(TestPresentation, makeServer_raw)
{
    Presentation presentation{mr_, scheduler_, transport_mock_};

    StrictMock<ResponseTxSessionMock> res_tx_session_mock;
    StrictMock<RequestRxSessionMock>  req_rx_session_mock;
    EXPECT_CALL(req_rx_session_mock, setOnReceiveCallback(_))  //
        .WillRepeatedly(Return());

    constexpr RequestRxParams rx_params{16, 147};
    EXPECT_CALL(transport_mock_, makeRequestRxSession(RequestRxParamsEq(rx_params)))  //
        .WillOnce(Invoke([&](const auto&) {                                           //
            return libcyphal::detail::makeUniquePtr<UniquePtrReqRxSpec>(mr_, req_rx_session_mock);
        }));
    constexpr ResponseTxParams tx_params{rx_params.service_id};
    EXPECT_CALL(transport_mock_, makeResponseTxSession(ResponseTxParamsEq(tx_params)))  //
        .WillOnce(Invoke([&](const auto&) {                                             //
            return libcyphal::detail::makeUniquePtr<UniquePtrResTxSpec>(mr_, res_tx_session_mock);
        }));

    auto maybe_server = presentation.makeServer(rx_params.service_id, rx_params.extent_bytes, [](const auto&, auto) {});
    ASSERT_THAT(maybe_server, VariantWith<RawServiceServer>(_));

    EXPECT_CALL(req_rx_session_mock, deinit()).Times(1);
    EXPECT_CALL(res_tx_session_mock, deinit()).Times(1);
}

TEST_F(TestPresentation, makeServer_with_failure)
{
    using Service = uavcan::node::GetInfo_1_0;

    Presentation presentation{mr_, scheduler_, transport_mock_};

    constexpr RequestRxParams  rx_params{Service::Request::_traits_::ExtentBytes,
                                        Service::Request::_traits_::FixedPortId};
    constexpr ResponseTxParams tx_params{rx_params.service_id};

    // RX AlreadyExistsError & MemoryError (due to nullptr)
    {
        // AlreadyExistsError with typed Service
        EXPECT_CALL(transport_mock_, makeRequestRxSession(RequestRxParamsEq(rx_params)))  //
            .WillOnce(Return(AlreadyExistsError{}));
        auto maybe_server1 = presentation.makeServer<Service>();
        EXPECT_THAT(maybe_server1, VariantWith<Presentation::MakeFailure>(VariantWith<AlreadyExistsError>(_)));

        // AlreadyExistsError with raw Service
        EXPECT_CALL(transport_mock_, makeRequestRxSession(RequestRxParamsEq(rx_params)))  //
            .WillOnce(Return(AlreadyExistsError{}));
        auto maybe_server2 = presentation.makeServer(rx_params.service_id, rx_params.extent_bytes);
        EXPECT_THAT(maybe_server2, VariantWith<Presentation::MakeFailure>(VariantWith<AlreadyExistsError>(_)));

        // MemoryError due to nullptr
        EXPECT_CALL(transport_mock_, makeRequestRxSession(RequestRxParamsEq(rx_params)))  //
            .WillOnce(Return(nullptr));
        auto maybe_server3 = presentation.makeServer<Service>();
        EXPECT_THAT(maybe_server3, VariantWith<Presentation::MakeFailure>(VariantWith<MemoryError>(_)));
    }
    // TX AlreadyExistsError & MemoryError (due to nullptr)
    {
        StrictMock<RequestRxSessionMock> req_rx_session_mock;
        EXPECT_CALL(transport_mock_, makeRequestRxSession(RequestRxParamsEq(rx_params)))  //
            .WillRepeatedly(Invoke([&](const auto&) {                                     //
                return libcyphal::detail::makeUniquePtr<UniquePtrReqRxSpec>(mr_, req_rx_session_mock);
            }));
        EXPECT_CALL(req_rx_session_mock, deinit()).Times(2);

        // AlreadyExistsError
        EXPECT_CALL(transport_mock_, makeResponseTxSession(ResponseTxParamsEq(tx_params)))  //
            .WillOnce(Return(AlreadyExistsError{}));
        auto maybe_server1 = presentation.makeServer<Service>();
        EXPECT_THAT(maybe_server1, VariantWith<Presentation::MakeFailure>(VariantWith<AlreadyExistsError>(_)));

        // MemoryError due to nullptr
        EXPECT_CALL(transport_mock_, makeResponseTxSession(ResponseTxParamsEq(tx_params)))  //
            .WillOnce(Return(nullptr));
        auto maybe_server2 = presentation.makeServer<Service>();
        EXPECT_THAT(maybe_server2, VariantWith<Presentation::MakeFailure>(VariantWith<MemoryError>(_)));
    }
}

TEST_F(TestPresentation, makeClient)
{
    using Service = uavcan::node::GetInfo_1_0;

    Presentation presentation{mr_, scheduler_, transport_mock_};

    StrictMock<ResponseRxSessionMock> res_rx_session_mock;
    StrictMock<RequestTxSessionMock>  req_tx_session_mock;

    constexpr ResponseRxParams rx_params{Service::Response::_traits_::ExtentBytes,
                                         Service::Request::_traits_::FixedPortId,
                                         0x31};
    EXPECT_CALL(res_rx_session_mock, getParams())  //
        .WillOnce(Return(rx_params));
    EXPECT_CALL(res_rx_session_mock, setTransferIdTimeout(_))  //
        .WillOnce(Return());
    EXPECT_CALL(res_rx_session_mock, setOnReceiveCallback(_))  //
        .WillOnce(Return());

    EXPECT_CALL(transport_mock_, makeResponseRxSession(ResponseRxParamsEq(rx_params)))  //
        .WillOnce(Invoke([&](const auto&) {                                             //
            return libcyphal::detail::makeUniquePtr<UniquePtrResRxSpec>(mr_, res_rx_session_mock);
        }));
    constexpr RequestTxParams tx_params{rx_params.service_id, rx_params.server_node_id};
    EXPECT_CALL(transport_mock_, makeRequestTxSession(RequestTxParamsEq(tx_params)))  //
        .WillOnce(Invoke([&](const auto&) {                                           //
            return libcyphal::detail::makeUniquePtr<UniquePtrReqTxSpec>(mr_, req_tx_session_mock);
        }));

    auto maybe_client = presentation.makeClient<Service>(rx_params.server_node_id);
    ASSERT_THAT(maybe_client, VariantWith<ServiceClient<Service>>(_));

    EXPECT_CALL(req_tx_session_mock, deinit()).Times(1);
    EXPECT_CALL(res_rx_session_mock, deinit()).Times(1);
}

TEST_F(TestPresentation, makeClient_multiple_custom)
{
    Presentation presentation{mr_, scheduler_, transport_mock_};

    StrictMock<ResponseRxSessionMock> res_rx_session_mock;
    StrictMock<RequestTxSessionMock>  req_tx_session_mock;

    constexpr ResponseRxParams rx_params{Custom::Service::Response::_traits_::ExtentBytes,
                                         Custom::Service::Request::_traits_::FixedPortId,
                                         0x31};
    EXPECT_CALL(res_rx_session_mock, getParams()).WillOnce(Return(rx_params));
    EXPECT_CALL(res_rx_session_mock, setTransferIdTimeout(_)).WillOnce(Return());
    EXPECT_CALL(res_rx_session_mock, setOnReceiveCallback(_)).WillOnce(Return());

    EXPECT_CALL(transport_mock_, makeResponseRxSession(ResponseRxParamsEq(rx_params)))  //
        .WillOnce(Invoke([&](const auto&) {                                             //
            return libcyphal::detail::makeUniquePtr<UniquePtrResRxSpec>(mr_, res_rx_session_mock);
        }));
    constexpr RequestTxParams tx_params{rx_params.service_id, rx_params.server_node_id};
    EXPECT_CALL(transport_mock_, makeRequestTxSession(RequestTxParamsEq(tx_params)))  //
        .WillOnce(Invoke([&](const auto&) {                                           //
            return libcyphal::detail::makeUniquePtr<UniquePtrReqTxSpec>(mr_, req_tx_session_mock);
        }));

    auto maybe_client1a = presentation.makeClient<Custom::Service>(rx_params.server_node_id);
    ASSERT_THAT(maybe_client1a, VariantWith<ServiceClient<Custom::Service>>(_));

    auto maybe_client1b = presentation.makeClient<Custom::Service>(rx_params.server_node_id);
    ASSERT_THAT(maybe_client1b, VariantWith<ServiceClient<Custom::Service>>(_));

    // The same custom service but to different server
    {
        StrictMock<ResponseRxSessionMock> res_rx_session_mock2;
        StrictMock<RequestTxSessionMock>  req_tx_session_mock2;

        constexpr ResponseRxParams rx_params2{rx_params.extent_bytes, rx_params.service_id, 0x32};
        EXPECT_CALL(res_rx_session_mock2, getParams()).WillOnce(Return(rx_params2));
        EXPECT_CALL(res_rx_session_mock2, setTransferIdTimeout(_)).WillOnce(Return());
        EXPECT_CALL(res_rx_session_mock2, setOnReceiveCallback(_)).WillOnce(Return());

        EXPECT_CALL(transport_mock_, makeResponseRxSession(ResponseRxParamsEq(rx_params2)))  //
            .WillOnce(Invoke([&](const auto&) {                                              //
                return libcyphal::detail::makeUniquePtr<UniquePtrResRxSpec>(mr_, res_rx_session_mock2);
            }));
        constexpr RequestTxParams tx_params2{rx_params2.service_id, rx_params2.server_node_id};
        EXPECT_CALL(transport_mock_, makeRequestTxSession(RequestTxParamsEq(tx_params2)))  //
            .WillOnce(Invoke([&](const auto&) {                                            //
                return libcyphal::detail::makeUniquePtr<UniquePtrReqTxSpec>(mr_, req_tx_session_mock2);
            }));

        auto maybe_client2 = presentation.makeClient<Custom::Service>(rx_params2.server_node_id);
        ASSERT_THAT(maybe_client2, VariantWith<ServiceClient<Custom::Service>>(_));

        EXPECT_CALL(res_rx_session_mock2, deinit()).Times(1);
        EXPECT_CALL(req_tx_session_mock2, deinit()).Times(1);
    }

    EXPECT_CALL(res_rx_session_mock, deinit()).Times(1);
    EXPECT_CALL(req_tx_session_mock, deinit()).Times(1);
}

TEST_F(TestPresentation, makeClient_raw)
{
    Presentation presentation{mr_, scheduler_, transport_mock_};

    StrictMock<ResponseRxSessionMock> res_rx_session_mock;
    StrictMock<RequestTxSessionMock>  req_tx_session_mock;

    constexpr ResponseRxParams rx_params{8, 147, 0x31};
    EXPECT_CALL(res_rx_session_mock, getParams()).WillOnce(Return(rx_params));
    EXPECT_CALL(res_rx_session_mock, setTransferIdTimeout(_)).WillOnce(Return());
    EXPECT_CALL(res_rx_session_mock, setOnReceiveCallback(_)).WillOnce(Return());

    EXPECT_CALL(transport_mock_, makeResponseRxSession(ResponseRxParamsEq(rx_params)))  //
        .WillOnce(Invoke([&](const auto&) {                                             //
            return libcyphal::detail::makeUniquePtr<UniquePtrResRxSpec>(mr_, res_rx_session_mock);
        }));
    constexpr RequestTxParams tx_params{rx_params.service_id, rx_params.server_node_id};
    EXPECT_CALL(transport_mock_, makeRequestTxSession(RequestTxParamsEq(tx_params)))  //
        .WillOnce(Invoke([&](const auto&) {                                           //
            return libcyphal::detail::makeUniquePtr<UniquePtrReqTxSpec>(mr_, req_tx_session_mock);
        }));

    auto maybe_client = presentation.makeClient(rx_params.server_node_id, rx_params.service_id, rx_params.extent_bytes);
    ASSERT_THAT(maybe_client, VariantWith<RawServiceClient>(_));

    EXPECT_CALL(req_tx_session_mock, deinit()).Times(1);
    EXPECT_CALL(res_rx_session_mock, deinit()).Times(1);
}

TEST_F(TestPresentation, makeClient_with_failure)
{
    using Service = uavcan::node::GetInfo_1_0;

    StrictMock<MemoryResourceMock> mr_mock;
    mr_mock.redirectExpectedCallsTo(mr_);

    Presentation presentation{mr_mock, scheduler_, transport_mock_};

    constexpr ResponseRxParams rx_params{Service::Response::_traits_::ExtentBytes,
                                         Service::Request::_traits_::FixedPortId,
                                         0x31};
    const RequestTxParams      tx_params{rx_params.service_id, rx_params.server_node_id};

    // TX AlreadyExistsError & MemoryError (due to nullptr)
    {
        // AlreadyExistsError with typed Service
        EXPECT_CALL(transport_mock_, makeRequestTxSession(RequestTxParamsEq(tx_params)))  //
            .WillOnce(Return(AlreadyExistsError{}));
        auto maybe_client1 = presentation.makeClient<Service>(rx_params.server_node_id);
        EXPECT_THAT(maybe_client1, VariantWith<Presentation::MakeFailure>(VariantWith<AlreadyExistsError>(_)));

        // AlreadyExistsError with raw Service
        EXPECT_CALL(transport_mock_, makeRequestTxSession(RequestTxParamsEq(tx_params)))  //
            .WillOnce(Return(AlreadyExistsError{}));
        auto maybe_client2 =
            presentation.makeClient(rx_params.server_node_id, rx_params.service_id, rx_params.extent_bytes);
        EXPECT_THAT(maybe_client2, VariantWith<Presentation::MakeFailure>(VariantWith<AlreadyExistsError>(_)));

        // MemoryError due to nullptr
        EXPECT_CALL(transport_mock_, makeRequestTxSession(RequestTxParamsEq(tx_params)))  //
            .WillOnce(Return(nullptr));
        auto maybe_client3 = presentation.makeClient<Service>(rx_params.server_node_id);
        EXPECT_THAT(maybe_client3, VariantWith<Presentation::MakeFailure>(VariantWith<MemoryError>(_)));
    }
    // RX AlreadyExistsError & MemoryError (due to nullptr)
    {
        StrictMock<RequestTxSessionMock> req_tx_session_mock;
        EXPECT_CALL(transport_mock_, makeRequestTxSession(RequestTxParamsEq(tx_params)))  //
            .WillRepeatedly(Invoke([&](const auto&) {                                     //
                return libcyphal::detail::makeUniquePtr<UniquePtrReqTxSpec>(mr_, req_tx_session_mock);
            }));
        EXPECT_CALL(req_tx_session_mock, deinit()).Times(2);

        // AlreadyExistsError
        EXPECT_CALL(transport_mock_, makeResponseRxSession(ResponseRxParamsEq(rx_params)))  //
            .WillOnce(Return(AlreadyExistsError{}));
        auto maybe_client1 = presentation.makeClient<Service>(rx_params.server_node_id);
        EXPECT_THAT(maybe_client1, VariantWith<Presentation::MakeFailure>(VariantWith<AlreadyExistsError>(_)));

        // MemoryError due to nullptr
        EXPECT_CALL(transport_mock_, makeResponseRxSession(ResponseRxParamsEq(rx_params)))  //
            .WillOnce(Return(nullptr));
        auto maybe_client2 = presentation.makeClient<Service>(rx_params.server_node_id);
        EXPECT_THAT(maybe_client2, VariantWith<Presentation::MakeFailure>(VariantWith<MemoryError>(_)));
    }
    // Emulate that there is no memory available for the `ClientImpl`.
    {
        using TransferIdGeneratorMixin = libcyphal::transport::detail::TrivialTransferIdGenerator;
        using ClientImpl               = libcyphal::presentation::detail::ClientImpl<TransferIdGeneratorMixin>;

        StrictMock<ResponseRxSessionMock> res_rx_session_mock;
        StrictMock<RequestTxSessionMock>  req_tx_session_mock;

        EXPECT_CALL(res_rx_session_mock, setOnReceiveCallback(_)).WillRepeatedly(Return());
        EXPECT_CALL(res_rx_session_mock, deinit()).Times(1);
        EXPECT_CALL(req_tx_session_mock, deinit()).Times(1);

        EXPECT_CALL(transport_mock_, makeResponseRxSession(ResponseRxParamsEq(rx_params)))  //
            .WillOnce(Invoke([&](const auto&) {                                             //
                return libcyphal::detail::makeUniquePtr<UniquePtrResRxSpec>(mr_, res_rx_session_mock);
            }));
        EXPECT_CALL(transport_mock_, makeRequestTxSession(RequestTxParamsEq(tx_params)))  //
            .WillRepeatedly(Invoke([&](const auto&) {                                     //
                return libcyphal::detail::makeUniquePtr<UniquePtrReqTxSpec>(mr_, req_tx_session_mock);
            }));

        EXPECT_CALL(mr_mock, do_allocate(sizeof(ClientImpl), _)).WillOnce(Return(nullptr));

        auto maybe_client = presentation.makeClient(  //
            rx_params.server_node_id,
            rx_params.service_id,
            rx_params.extent_bytes);
        EXPECT_THAT(maybe_client, VariantWith<Presentation::MakeFailure>(VariantWith<MemoryError>(_)));
    }
}

// NOLINTEND(cppcoreguidelines-avoid-magic-numbers, readability-magic-numbers)

}  // namespace
