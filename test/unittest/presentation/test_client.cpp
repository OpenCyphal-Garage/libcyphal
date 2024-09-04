/// @copyright
/// Copyright (C) OpenCyphal Development Team  <opencyphal.org>
/// Copyright Amazon.com Inc. or its affiliates.
/// SPDX-License-Identifier: MIT

#include "cetl_gtest_helpers.hpp"          // NOLINT(misc-include-cleaner)
#include "gtest_helpers.hpp"               // NOLINT(misc-include-cleaner)
#include "presentation_gtest_helpers.hpp"  // NOLINT(misc-include-cleaner)
#include "tracking_memory_resource.hpp"
#include "transport/scattered_buffer_storage_mock.hpp"
#include "transport/svc_sessions_mock.hpp"
#include "transport/transport_gtest_helpers.hpp"
#include "transport/transport_mock.hpp"
#include "virtual_time_scheduler.hpp"

#include <cetl/pf17/cetlpf.hpp>
#include <libcyphal/presentation/client.hpp>
#include <libcyphal/presentation/presentation.hpp>
#include <libcyphal/presentation/response_promise.hpp>
#include <libcyphal/transport/svc_sessions.hpp>
#include <libcyphal/transport/types.hpp>
#include <libcyphal/types.hpp>

#include <uavcan/node/GetInfo_1_0.hpp>

#include <gmock/gmock.h>
#include <gtest/gtest.h>

#include <cstddef>
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
using testing::Eq;
using testing::Invoke;
using testing::Return;
using testing::IsEmpty;
using testing::NiceMock;
using testing::Optional;
using testing::FieldsAre;
using testing::StrictMock;
using testing::ElementsAre;
using testing::VariantWith;

// https://github.com/llvm/llvm-project/issues/53444
// NOLINTBEGIN(misc-unused-using-decls, misc-include-cleaner)
using std::literals::chrono_literals::operator""s;
using std::literals::chrono_literals::operator""ms;
// NOLINTEND(misc-unused-using-decls, misc-include-cleaner)

// NOLINTBEGIN(cppcoreguidelines-avoid-magic-numbers, readability-magic-numbers, *-function-cognitive-complexity)

class TestClient : public testing::Test
{
protected:
    using UniquePtrReqTxSpec = RequestTxSessionMock::RefWrapper::Spec;
    using UniquePtrResRxSpec = ResponseRxSessionMock::RefWrapper::Spec;

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

TEST_F(TestClient, copy_move_getSetPriority)
{
    using Service = uavcan::node::GetInfo_1_0;

    static_assert(std::is_copy_assignable<ServiceClient<Service>>::value, "Should not be copy assignable.");
    static_assert(std::is_copy_constructible<ServiceClient<Service>>::value, "Should not be copy constructible.");
    static_assert(std::is_move_assignable<ServiceClient<Service>>::value, "Should not be move assignable.");
    static_assert(std::is_move_constructible<ServiceClient<Service>>::value, "Should be move constructible.");
    static_assert(!std::is_default_constructible<ServiceClient<Service>>::value,
                  "Should not be default constructible.");

    static_assert(std::is_copy_assignable<RawServiceClient>::value, "Should be copy assignable.");
    static_assert(std::is_copy_constructible<RawServiceClient>::value, "Should be copy constructible.");
    static_assert(std::is_move_assignable<RawServiceClient>::value, "Should be move assignable.");
    static_assert(std::is_move_constructible<RawServiceClient>::value, "Should be move constructible.");
    static_assert(!std::is_default_constructible<RawServiceClient>::value, "Should not be default constructible.");

    Presentation presentation{mr_, scheduler_, transport_mock_};

    StrictMock<RequestTxSessionMock>  req_tx_session_mock;
    StrictMock<ResponseRxSessionMock> res_rx_session_mock;

    const ResponseRxParams rx_params{Service::Response::_traits_::ExtentBytes,
                                     Service::Request::_traits_::FixedPortId,
                                     0x31};
    EXPECT_CALL(res_rx_session_mock, getParams())  //
        .WillOnce(Return(rx_params));
    EXPECT_CALL(res_rx_session_mock, setOnReceiveCallback(_)).WillRepeatedly(Return());

    const RequestTxParams tx_params{rx_params.service_id, rx_params.server_node_id};
    EXPECT_CALL(transport_mock_, makeRequestTxSession(RequestTxParamsEq(tx_params)))  //
        .WillOnce(Invoke([&](const auto&) {                                           //
            return libcyphal::detail::makeUniquePtr<UniquePtrReqTxSpec>(mr_, req_tx_session_mock);
        }));
    EXPECT_CALL(transport_mock_, makeResponseRxSession(ResponseRxParamsEq(rx_params)))  //
        .WillOnce(Invoke([&](const auto&) {                                             //
            return libcyphal::detail::makeUniquePtr<UniquePtrResRxSpec>(mr_, res_rx_session_mock);
        }));

    auto maybe_client1 = presentation.makeClient<Service>(rx_params.server_node_id);
    ASSERT_THAT(maybe_client1, VariantWith<ServiceClient<Service>>(_));
    auto client1a = cetl::get<ServiceClient<Service>>(std::move(maybe_client1));
    EXPECT_THAT(client1a.getPriority(), Priority::Nominal);

    client1a.setPriority(Priority::Immediate);
    EXPECT_THAT(client1a.getPriority(), Priority::Immediate);

    auto client1b = std::move(client1a);
    EXPECT_THAT(client1b.getPriority(), Priority::Immediate);

    auto client2 = client1b;
    EXPECT_THAT(client2.getPriority(), Priority::Immediate);
    client2.setPriority(Priority::Slow);
    EXPECT_THAT(client2.getPriority(), Priority::Slow);
    EXPECT_THAT(client1b.getPriority(), Priority::Immediate);

    client1b = client2;
    EXPECT_THAT(client1b.getPriority(), Priority::Slow);

    // Verify self-assignment.
    auto& client1c = client1b;
    client1c       = client1b;

    client2.setPriority(Priority::Optional);
    client1c = std::move(client2);
    EXPECT_THAT(client1c.getPriority(), Priority::Optional);

    EXPECT_CALL(res_rx_session_mock, deinit()).Times(1);
    EXPECT_CALL(req_tx_session_mock, deinit()).Times(1);
}

TEST_F(TestClient, request_response_get_fetch_result)
{
    using Service       = uavcan::node::GetInfo_1_0;
    using SvcResPromise = ResponsePromise<Service::Response>;

    Presentation presentation{mr_, scheduler_, transport_mock_};

    StrictMock<RequestTxSessionMock>  req_tx_session_mock;
    StrictMock<ResponseRxSessionMock> res_rx_session_mock;

    const ResponseRxParams rx_params{Service::Response::_traits_::ExtentBytes,
                                     Service::Request::_traits_::FixedPortId,
                                     0x31};
    EXPECT_CALL(res_rx_session_mock, getParams())  //
        .WillOnce(Return(rx_params));
    IResponseRxSession::OnReceiveCallback::Function res_rx_cb_fn;
    EXPECT_CALL(res_rx_session_mock, setOnReceiveCallback(_))  //
        .WillRepeatedly(Invoke([&](auto&& cb_fn) {             //
            res_rx_cb_fn = std::forward<IResponseRxSession::OnReceiveCallback::Function>(cb_fn);
        }));

    const RequestTxParams tx_params{rx_params.service_id, rx_params.server_node_id};
    EXPECT_CALL(transport_mock_, makeRequestTxSession(RequestTxParamsEq(tx_params)))  //
        .WillOnce(Invoke([&](const auto&) {                                           //
            return libcyphal::detail::makeUniquePtr<UniquePtrReqTxSpec>(mr_, req_tx_session_mock);
        }));
    EXPECT_CALL(transport_mock_, makeResponseRxSession(ResponseRxParamsEq(rx_params)))  //
        .WillOnce(Invoke([&](const auto&) {                                             //
            return libcyphal::detail::makeUniquePtr<UniquePtrResRxSpec>(mr_, res_rx_session_mock);
        }));

    auto maybe_client = presentation.makeClient<Service>(rx_params.server_node_id);
    ASSERT_THAT(maybe_client, VariantWith<ServiceClient<Service>>(_));
    auto client = cetl::get<ServiceClient<Service>>(std::move(maybe_client));

    constexpr TransferId transfer_id = 0;
    ASSERT_TRUE(res_rx_cb_fn);
    cetl::optional<SvcResPromise> response_promise;

    scheduler_.scheduleAt(1s, [&](const auto&) {
        //
        EXPECT_CALL(req_tx_session_mock, send(_, _))  //
            .WillOnce(Invoke([now = now()](const auto& metadata, const auto) {
                //
                EXPECT_THAT(metadata.base.transfer_id, transfer_id);
                EXPECT_THAT(metadata.base.priority, Priority::Nominal);
                EXPECT_THAT(metadata.deadline, now + 100ms);
                return cetl::nullopt;
            }));

        auto maybe_promise = client.request(now() + 100ms, Service::Request{}, now() + 2s);
        EXPECT_THAT(maybe_promise, VariantWith<SvcResPromise>(_));
        response_promise.emplace(cetl::get<SvcResPromise>(std::move(maybe_promise)));

        EXPECT_THAT(response_promise->getRequestTime(), now());
        EXPECT_THAT(response_promise->getResult(), Eq(cetl::nullopt));
        EXPECT_THAT(response_promise->fetchResult(), Eq(cetl::nullopt));
    });
    scheduler_.scheduleAt(1s + 300ms, [&](const auto&) {
        //
        EXPECT_THAT(response_promise->getResult(), Eq(cetl::nullopt));
        EXPECT_THAT(response_promise->fetchResult(), Eq(cetl::nullopt));
    });
    scheduler_.scheduleAt(2s, [&](const auto&) {
        //
        ServiceRxTransfer transfer{{{{transfer_id, Priority::Nominal}, now()}, 0x31}, {}};
        res_rx_cb_fn({transfer});
    });
    scheduler_.scheduleAt(2s + 1ms, [&](const auto&) {
        //
        EXPECT_THAT(response_promise->getResult(), Optional(VariantWith<SvcResPromise::Success>(_)));
        EXPECT_THAT(response_promise->fetchResult(), Optional(VariantWith<SvcResPromise::Success>(_)));

        EXPECT_THAT(response_promise->getResult(), Eq(cetl::nullopt));
        EXPECT_THAT(response_promise->fetchResult(), Eq(cetl::nullopt));
    });
    scheduler_.scheduleAt(2s + 10ms, [&](const auto&) {
        //
        // Emulate double reception of the same transfer (from hypothetical other redundant transport).
        ServiceRxTransfer transfer{{{{transfer_id, Priority::Nominal}, now()}, 0x31}, {}};
        res_rx_cb_fn({transfer});
    });
    scheduler_.spinFor(10s);

    EXPECT_CALL(res_rx_session_mock, deinit()).Times(1);
    EXPECT_CALL(req_tx_session_mock, deinit()).Times(1);
}

TEST_F(TestClient, request_response_via_callabck)
{
    using Service       = uavcan::node::GetInfo_1_0;
    using SvcResPromise = ResponsePromise<Service::Response>;

    Presentation presentation{mr_, scheduler_, transport_mock_};

    StrictMock<RequestTxSessionMock>  req_tx_session_mock;
    StrictMock<ResponseRxSessionMock> res_rx_session_mock;

    const ResponseRxParams rx_params{Service::Response::_traits_::ExtentBytes,
                                     Service::Request::_traits_::FixedPortId,
                                     0x31};
    EXPECT_CALL(res_rx_session_mock, getParams())  //
        .WillOnce(Return(rx_params));
    IResponseRxSession::OnReceiveCallback::Function res_rx_cb_fn;
    EXPECT_CALL(res_rx_session_mock, setOnReceiveCallback(_))  //
        .WillRepeatedly(Invoke([&](auto&& cb_fn) {             //
            res_rx_cb_fn = std::forward<IResponseRxSession::OnReceiveCallback::Function>(cb_fn);
        }));

    const RequestTxParams tx_params{rx_params.service_id, rx_params.server_node_id};
    EXPECT_CALL(transport_mock_, makeRequestTxSession(RequestTxParamsEq(tx_params)))  //
        .WillOnce(Invoke([&](const auto&) {                                           //
            return libcyphal::detail::makeUniquePtr<UniquePtrReqTxSpec>(mr_, req_tx_session_mock);
        }));
    EXPECT_CALL(transport_mock_, makeResponseRxSession(ResponseRxParamsEq(rx_params)))  //
        .WillOnce(Invoke([&](const auto&) {                                             //
            return libcyphal::detail::makeUniquePtr<UniquePtrResRxSpec>(mr_, res_rx_session_mock);
        }));

    auto maybe_client = presentation.makeClient<Service>(rx_params.server_node_id);
    ASSERT_THAT(maybe_client, VariantWith<ServiceClient<Service>>(_));
    auto client = cetl::get<ServiceClient<Service>>(std::move(maybe_client));

    constexpr TransferId transfer_id = 0;
    ASSERT_TRUE(res_rx_cb_fn);
    cetl::optional<SvcResPromise> response_promise;

    NiceMock<ScatteredBufferStorageMock> storage_mock;
    EXPECT_CALL(storage_mock, deinit()).Times(1);
    EXPECT_CALL(storage_mock, size()).WillRepeatedly(Return(0));
    ScatteredBufferStorageMock::Wrapper storage{&storage_mock};

    std::vector<std::tuple<ServiceRxMetadata, TimePoint>> responses;

    scheduler_.scheduleAt(1s, [&](const auto&) {
        //
        EXPECT_CALL(req_tx_session_mock, send(_, _))  //
            .WillOnce(Invoke([now = now()](const auto& metadata, const auto) {
                //
                EXPECT_THAT(metadata.base.transfer_id, transfer_id);
                EXPECT_THAT(metadata.base.priority, Priority::Nominal);
                EXPECT_THAT(metadata.deadline, now + 100ms);
                return cetl::nullopt;
            }));

        auto maybe_promise = client.request(now() + 100ms, Service::Request{}, now() + 2s);
        EXPECT_THAT(maybe_promise, VariantWith<SvcResPromise>(_));
        response_promise.emplace(cetl::get<SvcResPromise>(std::move(maybe_promise)));
    });
    scheduler_.scheduleAt(1s + 300ms, [&](const auto&) {
        //
        response_promise
            ->setCallback([](const auto&) {
                //
                FAIL() << "Unexpected dummy callback.";
            })
            .setCallback([&responses](const auto& arg) {
                //
                ASSERT_THAT(arg.result, VariantWith<SvcResPromise::Success>(_));
                auto success = cetl::get<SvcResPromise::Success>(std::move(arg.result));
                responses.emplace_back(success.metadata, arg.approx_now);
            });
    });
    scheduler_.scheduleAt(2s, [&](const auto&) {
        //
        EXPECT_THAT(responses, IsEmpty());

        ServiceRxTransfer transfer{{{{transfer_id, Priority::Nominal}, now()}, 0x31},
                                   ScatteredBuffer{std::move(storage)}};
        res_rx_cb_fn({transfer});
    });
    scheduler_.scheduleAt(2s + 1ms, [&](const auto&) {
        //
        EXPECT_THAT(responses,
                    ElementsAre(FieldsAre(ServiceRxMetadataEq({{{0, Priority::Nominal}, TimePoint{2s}}, 0x31}),
                                          TimePoint{2s})));

        EXPECT_THAT(response_promise->getResult(), Eq(cetl::nullopt));
        EXPECT_THAT(response_promise->fetchResult(), Eq(cetl::nullopt));
    });
    scheduler_.scheduleAt(2s + 10ms, [&](const auto&) {
        //
        // Emulate double reception of the same transfer (from hypothetical other redundant transport).
        ServiceRxTransfer transfer{{{{transfer_id, Priority::Nominal}, now()}, 0x31}, {}};
        res_rx_cb_fn({transfer});

        // Try set callback after the response has been received.
        response_promise->setCallback([&responses](const auto& arg) {
            //
            ASSERT_THAT(arg.result, VariantWith<SvcResPromise::Success>(_));
            auto success = cetl::get<SvcResPromise::Success>(std::move(arg.result));
            responses.emplace_back(success.metadata, arg.approx_now);
        });
    });
    scheduler_.spinFor(10s);

    EXPECT_THAT(responses,
                ElementsAre(
                    FieldsAre(ServiceRxMetadataEq({{{0, Priority::Nominal}, TimePoint{2s}}, 0x31}), TimePoint{2s})));

    EXPECT_CALL(res_rx_session_mock, deinit()).Times(1);
    EXPECT_CALL(req_tx_session_mock, deinit()).Times(1);
}

TEST_F(TestClient, raw_request_response_via_callabck)
{
    using SvcResPromise = ResponsePromise<void>;

    Presentation presentation{mr_, scheduler_, transport_mock_};

    StrictMock<RequestTxSessionMock>  req_tx_session_mock;
    StrictMock<ResponseRxSessionMock> res_rx_session_mock;

    const ResponseRxParams rx_params{16, 147, 0x31};
    EXPECT_CALL(res_rx_session_mock, getParams())  //
        .WillOnce(Return(rx_params));
    IResponseRxSession::OnReceiveCallback::Function res_rx_cb_fn;
    EXPECT_CALL(res_rx_session_mock, setOnReceiveCallback(_))  //
        .WillRepeatedly(Invoke([&](auto&& cb_fn) {             //
            res_rx_cb_fn = std::forward<IResponseRxSession::OnReceiveCallback::Function>(cb_fn);
        }));

    const RequestTxParams tx_params{rx_params.service_id, rx_params.server_node_id};
    EXPECT_CALL(transport_mock_, makeRequestTxSession(RequestTxParamsEq(tx_params)))  //
        .WillOnce(Invoke([&](const auto&) {                                           //
            return libcyphal::detail::makeUniquePtr<UniquePtrReqTxSpec>(mr_, req_tx_session_mock);
        }));
    EXPECT_CALL(transport_mock_, makeResponseRxSession(ResponseRxParamsEq(rx_params)))  //
        .WillOnce(Invoke([&](const auto&) {                                             //
            return libcyphal::detail::makeUniquePtr<UniquePtrResRxSpec>(mr_, res_rx_session_mock);
        }));

    auto maybe_client = presentation.makeClient(rx_params.server_node_id, rx_params.service_id, rx_params.extent_bytes);
    ASSERT_THAT(maybe_client, VariantWith<RawServiceClient>(_));
    auto client = cetl::get<RawServiceClient>(std::move(maybe_client));

    constexpr TransferId transfer_id = 0;
    ASSERT_TRUE(res_rx_cb_fn);
    cetl::optional<SvcResPromise> response_promise;

    NiceMock<ScatteredBufferStorageMock> storage_mock;
    EXPECT_CALL(storage_mock, deinit()).Times(1);
    EXPECT_CALL(storage_mock, size()).WillRepeatedly(Return(7));
    ScatteredBufferStorageMock::Wrapper storage{&storage_mock};

    std::vector<std::tuple<std::size_t, ServiceRxMetadata, TimePoint>> responses;

    scheduler_.scheduleAt(1s, [&](const auto&) {
        //
        EXPECT_CALL(req_tx_session_mock, send(_, _))  //
            .WillOnce(Invoke([now = now()](const auto& metadata, const auto) {
                //
                EXPECT_THAT(metadata.base.transfer_id, transfer_id);
                EXPECT_THAT(metadata.base.priority, Priority::Nominal);
                EXPECT_THAT(metadata.deadline, now + 100ms);
                return cetl::nullopt;
            }));

        auto maybe_promise = client.request(now() + 100ms, {}, now() + 2s);
        EXPECT_THAT(maybe_promise, VariantWith<SvcResPromise>(_));
        response_promise.emplace(cetl::get<SvcResPromise>(std::move(maybe_promise)));
    });
    scheduler_.scheduleAt(1s + 300ms, [&](const auto&) {
        //
        response_promise
            ->setCallback([](const auto&) {
                //
                FAIL() << "Unexpected dummy callback.";
            })
            .setCallback([&responses](const auto& arg) {
                //
                ASSERT_THAT(arg.result, VariantWith<SvcResPromise::Success>(_));
                auto success = cetl::get<SvcResPromise::Success>(std::move(arg.result));
                responses.emplace_back(success.response.size(), success.metadata, arg.approx_now);
            });
    });
    scheduler_.scheduleAt(2s, [&](const auto&) {
        //
        EXPECT_THAT(responses, IsEmpty());

        ServiceRxTransfer transfer{{{{transfer_id, Priority::Nominal}, now()}, 0x31},
                                   ScatteredBuffer{std::move(storage)}};
        res_rx_cb_fn({transfer});
    });
    scheduler_.scheduleAt(2s + 1ms, [&](const auto&) {
        //
        EXPECT_THAT(responses,
                    ElementsAre(FieldsAre(7,
                                          ServiceRxMetadataEq({{{0, Priority::Nominal}, TimePoint{2s}}, 0x31}),
                                          TimePoint{2s})));
    });
    scheduler_.scheduleAt(2s + 10ms, [&](const auto&) {
        //
        // Emulate double reception of the same transfer (from hypothetical other redundant transport).
        ServiceRxTransfer transfer{{{{transfer_id, Priority::Nominal}, now()}, 0x31}, {}};
        res_rx_cb_fn({transfer});

        // Try set callback after the response has been received.
        response_promise->setCallback([&responses](const auto& arg) {
            //
            ASSERT_THAT(arg.result, VariantWith<SvcResPromise::Success>(_));
            auto success = cetl::get<SvcResPromise::Success>(std::move(arg.result));
            responses.emplace_back(success.response.size(), success.metadata, arg.approx_now);
        });
    });
    scheduler_.spinFor(10s);

    EXPECT_THAT(responses,
                ElementsAre(
                    FieldsAre(7, ServiceRxMetadataEq({{{0, Priority::Nominal}, TimePoint{2s}}, 0x31}), TimePoint{2s})));

    EXPECT_CALL(res_rx_session_mock, deinit()).Times(1);
    EXPECT_CALL(req_tx_session_mock, deinit()).Times(1);
}

// NOLINTEND(cppcoreguidelines-avoid-magic-numbers, readability-magic-numbers, *-function-cognitive-complexity)

}  // namespace
