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
#include <libcyphal/presentation/client.hpp>
#include <libcyphal/presentation/presentation.hpp>
#include <libcyphal/presentation/publisher.hpp>
#include <libcyphal/presentation/publisher_impl.hpp>
#include <libcyphal/presentation/server.hpp>
#include <libcyphal/presentation/subscriber.hpp>
#include <libcyphal/presentation/subscriber_impl.hpp>
#include <libcyphal/transport/errors.hpp>
#include <libcyphal/transport/msg_sessions.hpp>
#include <libcyphal/transport/svc_sessions.hpp>
#include <libcyphal/types.hpp>

#include <nunavut/support/serialization.hpp>
#include <uavcan/node/Heartbeat_1_0.hpp>

#include <gmock/gmock.h>
#include <gtest/gtest.h>

#include <algorithm>
#include <cstddef>
#include <cstdint>
#include <uavcan/node/GetInfo_1_0.hpp>

namespace
{

using libcyphal::TimePoint;
using libcyphal::UniquePtr;
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
            static constexpr std::size_t   ExtentBytes    = sizeof(std::uint64_t);
        };

        std::uint64_t id{};  // NOLINT

    };  // Request

    struct Response final
    {
        struct _traits_
        {
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

TEST_F(TestPresentation, makePublisher_custom)
{
    using Message = Custom::PubMessage;

    Presentation presentation{mr_, scheduler_, transport_mock_};

    StrictMock<MessageTxSessionMock> msg_tx_session_mock;
    EXPECT_CALL(msg_tx_session_mock, getParams())  //
        .WillOnce(Return(MessageTxParams{Message::_traits_::FixedPortId}));

    EXPECT_CALL(transport_mock_, makeMessageTxSession(_))  //
        .WillOnce(Invoke([&](const auto&) {
            return libcyphal::detail::makeUniquePtr<MessageTxSessionMock::RefWrapper::Spec>(mr_, msg_tx_session_mock);
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
    EXPECT_CALL(msg_tx_session_mock, getParams())  //
        .WillOnce(Return(MessageTxParams{147}));

    EXPECT_CALL(transport_mock_, makeMessageTxSession(_))  //
        .WillOnce(Invoke([&](const auto&) {
            return libcyphal::detail::makeUniquePtr<MessageTxSessionMock::RefWrapper::Spec>(mr_, msg_tx_session_mock);
        }));

    auto maybe_pub = presentation.makePublisher<void>(147);
    EXPECT_THAT(maybe_pub, VariantWith<Publisher<void>>(_));

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
        EXPECT_THAT(maybe_pub, VariantWith<Presentation::MakeFailure>(VariantWith<AlreadyExistsError>(_)));
    }
    // MemoryError due to nullptr
    {
        EXPECT_CALL(transport_mock_, makeMessageTxSession(_))  //
            .WillOnce(Return(nullptr));

        auto maybe_pub = presentation.makePublisher<Message>(Message::_traits_::FixedPortId);
        EXPECT_THAT(maybe_pub, VariantWith<Presentation::MakeFailure>(VariantWith<MemoryError>(_)));
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
        EXPECT_THAT(maybe_pub, VariantWith<Presentation::MakeFailure>(VariantWith<MemoryError>(_)));
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

    auto maybe_sub2 = presentation.makeSubscriber<Message>([](const auto&) {});
    EXPECT_THAT(maybe_sub2, VariantWith<Subscriber<Message>>(_));

    EXPECT_CALL(msg_rx_session_mock, deinit()).Times(1);
}

TEST_F(TestPresentation, makeSubscriber_custom)
{
    using Message = Custom::SubMessage;

    Presentation presentation{mr_, scheduler_, transport_mock_};

    StrictMock<MessageRxSessionMock> msg_rx_session_mock;
    EXPECT_CALL(msg_rx_session_mock, getParams())  //
        .WillOnce(Return(MessageRxParams{0, Message::_traits_::FixedPortId}));
    EXPECT_CALL(msg_rx_session_mock, setOnReceiveCallback(_)).Times(1);

    EXPECT_CALL(transport_mock_, makeMessageRxSession(_))  //
        .WillOnce(Invoke([&](const auto&) {
            return libcyphal::detail::makeUniquePtr<MessageRxSessionMock::RefWrapper::Spec>(mr_, msg_rx_session_mock);
        }));

    auto maybe_sub = presentation.makeSubscriber<Message>([](const auto&) {});
    EXPECT_THAT(maybe_sub, VariantWith<Subscriber<Message>>(_));

    EXPECT_CALL(msg_rx_session_mock, deinit()).Times(1);
}

TEST_F(TestPresentation, makeSubscriber_raw)
{
    Presentation presentation{mr_, scheduler_, transport_mock_};

    StrictMock<MessageRxSessionMock> msg_rx_session_mock;
    EXPECT_CALL(msg_rx_session_mock, getParams())  //
        .WillOnce(Return(MessageRxParams{0, 147}));
    EXPECT_CALL(msg_rx_session_mock, setOnReceiveCallback(_)).Times(1);

    EXPECT_CALL(transport_mock_, makeMessageRxSession(_))  //
        .WillOnce(Invoke([&](const auto&) {
            return libcyphal::detail::makeUniquePtr<MessageRxSessionMock::RefWrapper::Spec>(mr_, msg_rx_session_mock);
        }));

    auto maybe_sub1 = presentation.makeSubscriber(147, 16);
    EXPECT_THAT(maybe_sub1, VariantWith<Subscriber<void>>(_));

    auto maybe_sub2 = presentation.makeSubscriber(147, 16, [](const auto&) {});
    EXPECT_THAT(maybe_sub2, VariantWith<Subscriber<void>>(_));

    EXPECT_CALL(msg_rx_session_mock, deinit()).Times(1);
}

TEST_F(TestPresentation, makeSubscriber_with_failure)
{
    using Message       = uavcan::node::Heartbeat_1_0;
    using UniquePtrSpec = MessageRxSessionMock::RefWrapper::Spec;

    StrictMock<MemoryResourceMock> mr_mock;
    mr_mock.redirectExpectedCallsTo(mr_);

    Presentation presentation{mr_mock, scheduler_, transport_mock_};

    // AlreadyExistsError
    {
        EXPECT_CALL(transport_mock_, makeMessageRxSession(_))  //
            .WillOnce(Return(AlreadyExistsError{}));

        auto maybe_sub = presentation.makeSubscriber<Message>(Message::_traits_::FixedPortId);
        EXPECT_THAT(maybe_sub, VariantWith<Presentation::MakeFailure>(VariantWith<AlreadyExistsError>(_)));

        EXPECT_CALL(transport_mock_, makeMessageRxSession(_))  //
            .WillOnce(Return(AlreadyExistsError{}));

        auto maybe_raw_sub = presentation.makeSubscriber(Message::_traits_::FixedPortId, 16);
        EXPECT_THAT(maybe_sub, VariantWith<Presentation::MakeFailure>(VariantWith<AlreadyExistsError>(_)));
    }
    // MemoryError due to nullptr
    {
        EXPECT_CALL(transport_mock_, makeMessageRxSession(_))  //
            .WillOnce(Return(nullptr));

        auto maybe_sub = presentation.makeSubscriber<Message>(Message::_traits_::FixedPortId);
        EXPECT_THAT(maybe_sub, VariantWith<Presentation::MakeFailure>(VariantWith<MemoryError>(_)));
    }
    // Emulate that there is no memory available for the `SubscriberImpl`.
    {
        StrictMock<MessageRxSessionMock> msg_rx_session_mock;
        EXPECT_CALL(msg_rx_session_mock, deinit()).Times(1);

        EXPECT_CALL(transport_mock_, makeMessageRxSession(_))  //
            .WillOnce(Invoke([&](const auto&) {
                return libcyphal::detail::makeUniquePtr<UniquePtrSpec>(mr_, msg_rx_session_mock);
            }));
        EXPECT_CALL(mr_mock, do_allocate(sizeof(detail::SubscriberImpl), _)).WillOnce(Return(nullptr));

        auto maybe_sub = presentation.makeSubscriber(Message::_traits_::FixedPortId, 16);
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

    const RequestRxParams rx_params{Service::Request::_traits_::ExtentBytes, Service::Request::_traits_::FixedPortId};
    EXPECT_CALL(transport_mock_, makeRequestRxSession(RequestRxParamsEq(rx_params)))  //
        .WillOnce(Invoke([&](const auto&) {
            return libcyphal::detail::makeUniquePtr<RequestRxSessionMock::RefWrapper::Spec>(mr_, req_rx_session_mock);
        }));
    const ResponseTxParams tx_params{rx_params.service_id};
    EXPECT_CALL(transport_mock_, makeResponseTxSession(ResponseTxParamsEq(tx_params)))  //
        .WillOnce(Invoke([&](const auto&) {
            return libcyphal::detail::makeUniquePtr<ResponseTxSessionMock::RefWrapper::Spec>(mr_, res_tx_session_mock);
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

    const RequestRxParams rx_params{Service::Request::_traits_::ExtentBytes, Service::Request::_traits_::FixedPortId};
    EXPECT_CALL(transport_mock_, makeRequestRxSession(RequestRxParamsEq(rx_params)))  //
        .WillOnce(Invoke([&](const auto&) {
            return libcyphal::detail::makeUniquePtr<RequestRxSessionMock::RefWrapper::Spec>(mr_, req_rx_session_mock);
        }));
    const ResponseTxParams tx_params{rx_params.service_id};
    EXPECT_CALL(transport_mock_, makeResponseTxSession(ResponseTxParamsEq(tx_params)))  //
        .WillOnce(Invoke([&](const auto&) {
            return libcyphal::detail::makeUniquePtr<ResponseTxSessionMock::RefWrapper::Spec>(mr_, res_tx_session_mock);
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

    const RequestRxParams rx_params{16, 147};
    EXPECT_CALL(transport_mock_, makeRequestRxSession(RequestRxParamsEq(rx_params)))  //
        .WillOnce(Invoke([&](const auto&) {
            return libcyphal::detail::makeUniquePtr<RequestRxSessionMock::RefWrapper::Spec>(mr_, req_rx_session_mock);
        }));
    const ResponseTxParams tx_params{rx_params.service_id};
    EXPECT_CALL(transport_mock_, makeResponseTxSession(ResponseTxParamsEq(tx_params)))  //
        .WillOnce(Invoke([&](const auto&) {
            return libcyphal::detail::makeUniquePtr<ResponseTxSessionMock::RefWrapper::Spec>(mr_, res_tx_session_mock);
        }));

    auto maybe_server = presentation.makeServer(rx_params.service_id, rx_params.extent_bytes, [](const auto&, auto) {});
    ASSERT_THAT(maybe_server, VariantWith<RawServiceServer>(_));

    EXPECT_CALL(req_rx_session_mock, deinit()).Times(1);
    EXPECT_CALL(res_tx_session_mock, deinit()).Times(1);
}

TEST_F(TestPresentation, makeServer_with_failure)
{
    using Service         = uavcan::node::GetInfo_1_0;
    using UniquePtrRxSpec = RequestRxSessionMock::RefWrapper::Spec;

    Presentation presentation{mr_, scheduler_, transport_mock_};

    const RequestRxParams  rx_params{Service::Request::_traits_::ExtentBytes, Service::Request::_traits_::FixedPortId};
    const ResponseTxParams tx_params{rx_params.service_id};

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
            .WillRepeatedly(Invoke([&](const auto&) {
                return libcyphal::detail::makeUniquePtr<UniquePtrRxSpec>(mr_, req_rx_session_mock);
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

    const ResponseRxParams rx_params{Service::Response::_traits_::ExtentBytes,
                                     Service::Request::_traits_::FixedPortId,
                                     0x31};
    EXPECT_CALL(res_rx_session_mock, setOnReceiveCallback(_))  //
        .WillRepeatedly(Return());
    EXPECT_CALL(res_rx_session_mock, getParams())  //
        .WillOnce(Return(rx_params));

    EXPECT_CALL(transport_mock_, makeResponseRxSession(ResponseRxParamsEq(rx_params)))  //
        .WillOnce(Invoke([&](const auto&) {
            return libcyphal::detail::makeUniquePtr<ResponseRxSessionMock::RefWrapper::Spec>(mr_, res_rx_session_mock);
        }));
    const RequestTxParams tx_params{rx_params.service_id, rx_params.server_node_id};
    EXPECT_CALL(transport_mock_, makeRequestTxSession(RequestTxParamsEq(tx_params)))  //
        .WillOnce(Invoke([&](const auto&) {
            return libcyphal::detail::makeUniquePtr<RequestTxSessionMock::RefWrapper::Spec>(mr_, req_tx_session_mock);
        }));

    auto maybe_client = presentation.makeClient<Service>(0x31);
    ASSERT_THAT(maybe_client, VariantWith<ServiceClient<Service>>(_));

    EXPECT_CALL(req_tx_session_mock, deinit()).Times(1);
    EXPECT_CALL(res_rx_session_mock, deinit()).Times(1);
}

// NOLINTEND(cppcoreguidelines-avoid-magic-numbers, readability-magic-numbers)

}  // namespace
