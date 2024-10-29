/// @copyright
/// Copyright (C) OpenCyphal Development Team  <opencyphal.org>
/// Copyright Amazon.com Inc. or its affiliates.
/// SPDX-License-Identifier: MIT

#include "cetl_gtest_helpers.hpp"  // NOLINT(misc-include-cleaner)
#include "gtest_helpers.hpp"       // NOLINT(misc-include-cleaner)
#include "memory_resource_mock.hpp"
#include "my_custom/baz_1_0.hpp"
#include "tracking_memory_resource.hpp"
#include "transport/scattered_buffer_storage_mock.hpp"
#include "transport/svc_sessions_mock.hpp"
#include "transport/transport_gtest_helpers.hpp"
#include "transport/transport_mock.hpp"
#include "virtual_time_scheduler.hpp"

#include <cetl/pf17/cetlpf.hpp>
#include <libcyphal/errors.hpp>
#include <libcyphal/presentation/presentation.hpp>
#include <libcyphal/presentation/server.hpp>
#include <libcyphal/transport/errors.hpp>
#include <libcyphal/transport/svc_sessions.hpp>
#include <libcyphal/transport/types.hpp>
#include <libcyphal/types.hpp>

#include <nunavut/support/serialization.hpp>
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

using testing::_;
using testing::Invoke;
using testing::Return;
using testing::IsEmpty;
using testing::NotNull;
using testing::NiceMock;
using testing::Optional;
using testing::StrictMock;
using testing::VariantWith;

// https://github.com/llvm/llvm-project/issues/53444
// NOLINTBEGIN(misc-unused-using-decls, misc-include-cleaner)
using std::literals::chrono_literals::operator""s;
using std::literals::chrono_literals::operator""ms;
// NOLINTEND(misc-unused-using-decls, misc-include-cleaner)

// NOLINTBEGIN(cppcoreguidelines-avoid-magic-numbers, readability-magic-numbers)

class TestServer : public testing::Test
{
protected:
    using UniquePtrReqRxSpec = RequestRxSessionMock::RefWrapper::Spec;
    using UniquePtrResTxSpec = ResponseTxSessionMock::RefWrapper::Spec;

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

TEST_F(TestServer, move)
{
    using Service = uavcan::node::GetInfo_1_0;

    static_assert(!std::is_copy_assignable<ServiceServer<Service>>::value, "Should not be copy assignable.");
    static_assert(!std::is_copy_constructible<ServiceServer<Service>>::value, "Should not be copy constructible.");
    static_assert(!std::is_move_assignable<ServiceServer<Service>>::value, "Should not be move assignable.");
    static_assert(std::is_move_constructible<ServiceServer<Service>>::value, "Should be move constructible.");
    static_assert(!std::is_default_constructible<ServiceServer<Service>>::value,
                  "Should not be default constructible.");

    static_assert(!std::is_copy_assignable<RawServiceServer>::value, "Should not be copy assignable.");
    static_assert(!std::is_copy_constructible<RawServiceServer>::value, "Should not be copy constructible.");
    static_assert(!std::is_move_assignable<RawServiceServer>::value, "Should not be move assignable.");
    static_assert(std::is_move_constructible<RawServiceServer>::value, "Should be move constructible.");
    static_assert(!std::is_default_constructible<RawServiceServer>::value, "Should not be default constructible.");

    Presentation presentation{mr_, scheduler_, transport_mock_};

    StrictMock<ResponseTxSessionMock> res_tx_session_mock;
    StrictMock<RequestRxSessionMock>  req_rx_session_mock;
    EXPECT_CALL(req_rx_session_mock, setOnReceiveCallback(_)).WillRepeatedly(Return());

    constexpr RequestRxParams rx_params{Service::Request::_traits_::ExtentBytes,
                                        Service::Request::_traits_::FixedPortId};
    EXPECT_CALL(transport_mock_, makeRequestRxSession(RequestRxParamsEq(rx_params)))  //
        .WillOnce(Invoke([&](const auto&) {                                           //
            return libcyphal::detail::makeUniquePtr<UniquePtrReqRxSpec>(mr_, req_rx_session_mock);
        }));
    constexpr ResponseTxParams tx_params{Service::Request::_traits_::FixedPortId};
    EXPECT_CALL(transport_mock_, makeResponseTxSession(ResponseTxParamsEq(tx_params)))  //
        .WillOnce(Invoke([&](const auto&) {                                             //
            return libcyphal::detail::makeUniquePtr<UniquePtrResTxSpec>(mr_, res_tx_session_mock);
        }));

    auto maybe_svr1 = presentation.makeServer<Service>(rx_params.service_id);
    ASSERT_THAT(maybe_svr1, VariantWith<ServiceServer<Service>>(_));
    auto srv1a = cetl::get<ServiceServer<Service>>(std::move(maybe_svr1));

    cetl::optional<ServiceServer<Service>> srv1b{std::move(srv1a)};

    testing::Mock::VerifyAndClearExpectations(&transport_mock_);

    EXPECT_CALL(transport_mock_, makeRequestRxSession(RequestRxParamsEq(rx_params)))  //
        .WillOnce(Return(AlreadyExistsError{}));

    auto maybe_srv2 = presentation.makeServer<Service>(rx_params.service_id);
    EXPECT_THAT(maybe_srv2, VariantWith<Presentation::MakeFailure>(VariantWith<AlreadyExistsError>(_)));

    EXPECT_CALL(req_rx_session_mock, deinit()).Times(1);
    EXPECT_CALL(res_tx_session_mock, deinit()).Times(1);
    srv1b.reset();
    testing::Mock::VerifyAndClearExpectations(&req_rx_session_mock);
    testing::Mock::VerifyAndClearExpectations(&res_tx_session_mock);
}

TEST_F(TestServer, service_request_response)
{
    using Service = uavcan::node::GetInfo_1_0;

    Presentation presentation{mr_, scheduler_, transport_mock_};

    IRequestRxSession::OnReceiveCallback::Function req_rx_cb_fn;
    StrictMock<RequestRxSessionMock>               req_rx_session_mock;
    EXPECT_CALL(req_rx_session_mock, setOnReceiveCallback(_))  //
        .WillRepeatedly(Invoke([&](auto&& cb_fn) {             //
            req_rx_cb_fn = std::forward<IRequestRxSession::OnReceiveCallback::Function>(cb_fn);
        }));

    StrictMock<ResponseTxSessionMock> res_tx_session_mock;

    constexpr RequestRxParams rx_params{Service::Request::_traits_::ExtentBytes,
                                        Service::Request::_traits_::FixedPortId};
    EXPECT_CALL(transport_mock_, makeRequestRxSession(RequestRxParamsEq(rx_params)))  //
        .WillOnce(Invoke([&](const auto&) {                                           //
            return libcyphal::detail::makeUniquePtr<UniquePtrReqRxSpec>(mr_, req_rx_session_mock);
        }));
    constexpr ResponseTxParams tx_params{Service::Request::_traits_::FixedPortId};
    EXPECT_CALL(transport_mock_, makeResponseTxSession(ResponseTxParamsEq(tx_params)))  //
        .WillOnce(Invoke([&](const auto&) {                                             //
            return libcyphal::detail::makeUniquePtr<UniquePtrResTxSpec>(mr_, res_tx_session_mock);
        }));

    auto maybe_server = presentation.makeServer<Service>();
    ASSERT_THAT(maybe_server, VariantWith<ServiceServer<Service>>(_));
    auto server = cetl::get<ServiceServer<Service>>(std::move(maybe_server));

    ASSERT_TRUE(req_rx_cb_fn);

    ServiceServer<Service>::OnRequestCallback::Continuation req_continuation;

    ServiceRxTransfer request{{{{123, Priority::Fast}, {}}, NodeId{0x31}}, {}};

    scheduler_.scheduleAt(1s, [&](const auto&) {
        //
        request.metadata.rx_meta.timestamp = now();
        req_rx_cb_fn({request});
    });
    scheduler_.scheduleAt(2s, [&](const auto&) {
        //
        server.setOnRequestCallback([&req_continuation](const auto&, auto cont) {
            //
            req_continuation = std::move(cont);
        });
        request.metadata.rx_meta.timestamp = now();
        req_rx_cb_fn({request});
    });
    scheduler_.scheduleAt(3s, [&](const auto&) {
        //
        ASSERT_TRUE(req_continuation);
        EXPECT_CALL(res_tx_session_mock, send(_, _))  //
            .WillOnce(Invoke([now = now()](const auto& metadata, const auto) {
                //
                EXPECT_THAT(metadata.tx_meta.base.transfer_id, 123);
                EXPECT_THAT(metadata.tx_meta.base.priority, Priority::Fast);
                EXPECT_THAT(metadata.tx_meta.deadline, now + 200ms);
                EXPECT_THAT(metadata.remote_node_id, NodeId{0x31});
                return cetl::nullopt;
            }));
        req_continuation(now() + 200ms, Service::Response{mr_alloc_});
    });
    scheduler_.spinFor(10s);

    EXPECT_CALL(req_rx_session_mock, deinit()).Times(1);
    EXPECT_CALL(res_tx_session_mock, deinit()).Times(1);
}

TEST_F(TestServer, service_request_response_failures)
{
    using Service = my_custom::baz_1_0;

    StrictMock<MemoryResourceMock> mr_mock;
    mr_mock.redirectExpectedCallsTo(mr_);

    Presentation presentation{mr_mock, scheduler_, transport_mock_};

    IRequestRxSession::OnReceiveCallback::Function req_rx_cb_fn;
    StrictMock<RequestRxSessionMock>               req_rx_session_mock;
    EXPECT_CALL(req_rx_session_mock, setOnReceiveCallback(_))  //
        .WillRepeatedly(Invoke([&](auto&& cb_fn) {             //
            req_rx_cb_fn = std::forward<IRequestRxSession::OnReceiveCallback::Function>(cb_fn);
        }));

    StrictMock<ResponseTxSessionMock> res_tx_session_mock;

    constexpr RequestRxParams rx_params{Service::Request::_traits_::ExtentBytes, 123};
    EXPECT_CALL(transport_mock_, makeRequestRxSession(RequestRxParamsEq(rx_params)))  //
        .WillOnce(Invoke([&](const auto&) {                                           //
            return libcyphal::detail::makeUniquePtr<UniquePtrReqRxSpec>(mr_, req_rx_session_mock);
        }));
    constexpr ResponseTxParams tx_params{rx_params.service_id};
    EXPECT_CALL(transport_mock_, makeResponseTxSession(ResponseTxParamsEq(tx_params)))  //
        .WillOnce(Invoke([&](const auto&) {                                             //
            return libcyphal::detail::makeUniquePtr<UniquePtrResTxSpec>(mr_, res_tx_session_mock);
        }));

    ServiceServer<Service>::OnRequestCallback::Continuation req_continuation;

    auto maybe_server = presentation.makeServer<Service>(rx_params.service_id);
    ASSERT_THAT(maybe_server, VariantWith<ServiceServer<Service>>(_));
    auto server = cetl::get<ServiceServer<Service>>(std::move(maybe_server));
    server.setOnRequestCallback([&req_continuation](const auto&, auto cont) {
        //
        req_continuation = std::move(cont);
    });
    ASSERT_TRUE(req_rx_cb_fn);

    NiceMock<ScatteredBufferStorageMock> storage_mock;
    EXPECT_CALL(storage_mock, deinit()).Times(2);

    scheduler_.scheduleAt(1s, [&](const auto&) {
        //
        EXPECT_CALL(storage_mock, size()).WillRepeatedly(Return(1));
        EXPECT_CALL(storage_mock, copy(0, NotNull(), 1))
            .WillOnce(Invoke([](const auto, auto* const dst, const auto) {  //
                // this will make it fail to deserialize request with SerializationBadArrayLength
                *dst = cetl::byte(255);
                return 1;
            }));
        ScatteredBufferStorageMock::Wrapper storage{&storage_mock};
        ServiceRxTransfer request{{{{123, Priority::Fast}, now()}, NodeId{0x31}}, ScatteredBuffer{std::move(storage)}};
        req_rx_cb_fn({request});
    });
    scheduler_.scheduleAt(2s, [&](const auto&) {
        //
        using libcyphal::presentation::detail::SmallPayloadSize;

        // Emulate that there is no memory available for the request deserialization.
        EXPECT_CALL(storage_mock, size()).WillRepeatedly(Return(SmallPayloadSize + 1));
        EXPECT_CALL(mr_mock, do_allocate(SmallPayloadSize + 1, _)).WillOnce(Return(nullptr));
        ScatteredBufferStorageMock::Wrapper storage{&storage_mock};

        ServiceRxTransfer request{{{{123, Priority::Fast}, now()}, NodeId{0x31}}, ScatteredBuffer{std::move(storage)}};
        req_rx_cb_fn({request});
    });
    scheduler_.scheduleAt(3s, [&](const auto&) {
        //
        ServiceRxTransfer request{{{{123, Priority::Fast}, now()}, NodeId{0x31}}, {}};
        req_rx_cb_fn({request});
        ASSERT_TRUE(req_continuation);

        // Emulate failure of response sending.
        EXPECT_CALL(res_tx_session_mock, send(_, _))  //
            .WillOnce(Return(libcyphal::ArgumentError{}));
        EXPECT_THAT(req_continuation(now() + 200ms, Service::Response{mr_alloc_}),
                    Optional(VariantWith<libcyphal::ArgumentError>(_)));
    });
    scheduler_.scheduleAt(3s, [&](const auto&) {
        //
        ServiceRxTransfer request{{{{123, Priority::Fast}, now()}, NodeId{0x31}}, {}};
        req_rx_cb_fn({request});
        ASSERT_TRUE(req_continuation);

        // Emulate failure to serialize the response.
        // This will make it fail to serialize the response with `SerializationBadArrayLength`.
        Service::Response response{mr_alloc_};
        response.some_other_crap = Service::Response::_traits_::TypeOf::some_other_crap{mr_alloc_};
        response.some_other_crap.resize(255);
        EXPECT_THAT(req_continuation(now() + 200ms, response),
                    Optional(
                        VariantWith<nunavut::support::Error>(nunavut::support::Error::SerializationBadArrayLength)));
    });
    scheduler_.spinFor(10s);

    EXPECT_CALL(req_rx_session_mock, deinit()).Times(1);
    EXPECT_CALL(res_tx_session_mock, deinit()).Times(1);
}

TEST_F(TestServer, raw_request_response)
{
    Presentation presentation{mr_, scheduler_, transport_mock_};

    IRequestRxSession::OnReceiveCallback::Function req_rx_cb_fn;
    StrictMock<RequestRxSessionMock>               req_rx_session_mock;
    EXPECT_CALL(req_rx_session_mock, setOnReceiveCallback(_))  //
        .WillRepeatedly(Invoke([&](auto&& cb_fn) {             //
            req_rx_cb_fn = std::forward<IRequestRxSession::OnReceiveCallback::Function>(cb_fn);
        }));

    StrictMock<ResponseTxSessionMock> res_tx_session_mock;

    constexpr RequestRxParams rx_params{0x456, 0x123};
    EXPECT_CALL(transport_mock_, makeRequestRxSession(RequestRxParamsEq(rx_params)))  //
        .WillOnce(Invoke([&](const auto&) {                                           //
            return libcyphal::detail::makeUniquePtr<UniquePtrReqRxSpec>(mr_, req_rx_session_mock);
        }));
    constexpr ResponseTxParams tx_params{rx_params.service_id};
    EXPECT_CALL(transport_mock_, makeResponseTxSession(ResponseTxParamsEq(tx_params)))  //
        .WillOnce(Invoke([&](const auto&) {                                             //
            return libcyphal::detail::makeUniquePtr<UniquePtrResTxSpec>(mr_, res_tx_session_mock);
        }));

    auto maybe_server = presentation.makeServer(rx_params.service_id, rx_params.extent_bytes);
    ASSERT_THAT(maybe_server, VariantWith<RawServiceServer>(_));
    auto raw_server = cetl::get<RawServiceServer>(std::move(maybe_server));

    ASSERT_TRUE(req_rx_cb_fn);

    RawServiceServer::OnRequestCallback::Continuation raw_req_continuation;

    ServiceRxTransfer request{{{{123, Priority::Fast}, {}}, NodeId{0x31}}, {}};

    scheduler_.scheduleAt(1s, [&](const auto&) {
        //
        request.metadata.rx_meta.timestamp = now();
        req_rx_cb_fn({request});
    });
    scheduler_.scheduleAt(2s, [&](const auto&) {
        //
        raw_server.setOnRequestCallback([&raw_req_continuation](const auto&, auto cont) {
            //
            raw_req_continuation = std::move(cont);
        });
        request.metadata.rx_meta.timestamp = now();
        req_rx_cb_fn({request});
    });
    scheduler_.scheduleAt(3s, [&](const auto&) {
        //
        ASSERT_TRUE(raw_req_continuation);
        EXPECT_CALL(res_tx_session_mock, send(_, _))  //
            .WillOnce(Invoke([now = now()](const auto& metadata, const auto) {
                //
                EXPECT_THAT(metadata.tx_meta.base.transfer_id, 123);
                EXPECT_THAT(metadata.tx_meta.base.priority, Priority::Fast);
                EXPECT_THAT(metadata.tx_meta.deadline, now + 200ms);
                EXPECT_THAT(metadata.remote_node_id, NodeId{0x31});
                return cetl::nullopt;
            }));
        raw_req_continuation(now() + 200ms, {});
    });
    scheduler_.spinFor(10s);

    EXPECT_CALL(req_rx_session_mock, deinit()).Times(1);
    EXPECT_CALL(res_tx_session_mock, deinit()).Times(1);
}

// NOLINTEND(cppcoreguidelines-avoid-magic-numbers, readability-magic-numbers)

}  // namespace
