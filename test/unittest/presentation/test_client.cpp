/// @copyright
/// Copyright (C) OpenCyphal Development Team  <opencyphal.org>
/// Copyright Amazon.com Inc. or its affiliates.
/// SPDX-License-Identifier: MIT

#include "cetl_gtest_helpers.hpp"  // NOLINT(misc-include-cleaner)
#include "gtest_helpers.hpp"       // NOLINT(misc-include-cleaner)
#include "memory_resource_mock.hpp"
#include "my_custom/baz_1_0.hpp"
#include "presentation_gtest_helpers.hpp"  // NOLINT(misc-include-cleaner)
#include "tracking_memory_resource.hpp"
#include "transport/scattered_buffer_storage_mock.hpp"
#include "transport/svc_sessions_mock.hpp"
#include "transport/transport_gtest_helpers.hpp"
#include "transport/transport_mock.hpp"
#include "virtual_time_scheduler.hpp"

#include <cetl/pf17/cetlpf.hpp>
#include <libcyphal/errors.hpp>
#include <libcyphal/presentation/client.hpp>
#include <libcyphal/presentation/common_helpers.hpp>
#include <libcyphal/presentation/presentation.hpp>
#include <libcyphal/presentation/response_promise.hpp>
#include <libcyphal/transport/errors.hpp>
#include <libcyphal/transport/svc_sessions.hpp>
#include <libcyphal/transport/types.hpp>
#include <libcyphal/types.hpp>

#include <cassert>  // NOLINT for NUNAVUT_ASSERT
#include <nunavut/support/serialization.hpp>
#include <uavcan/node/GetInfo_1_0.hpp>

#include <gmock/gmock.h>
#include <gtest/gtest.h>

#include <cstddef>
#include <limits>
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
using testing::Eq;
using testing::Invoke;
using testing::Return;
using testing::IsEmpty;
using testing::NotNull;
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

    void SetUp() override
    {
        cetl::pmr::set_default_resource(&mr_);

        EXPECT_CALL(transport_mock_, getProtocolParams())
            .WillRepeatedly(Return(ProtocolParams{std::numeric_limits<TransferId>::max(), 0, 0}));
    }

    void TearDown() override
    {
        EXPECT_THAT(mr_.allocations, IsEmpty());
        EXPECT_THAT(mr_.total_allocated_bytes, mr_.total_deallocated_bytes);
    }

    struct State
    {
        // NOLINTBEGIN
        StrictMock<RequestTxSessionMock>                req_tx_session_mock_;
        StrictMock<ResponseRxSessionMock>               res_rx_session_mock_;
        IResponseRxSession::OnReceiveCallback::Function res_rx_cb_fn_;
        // NOLINTEND

        State(cetl::pmr::memory_resource& memory,
              StrictMock<TransportMock>&  transport_mock,
              const ResponseRxParams&     rx_params)
        {
            EXPECT_CALL(res_rx_session_mock_, getParams())  //
                .WillOnce(Return(rx_params));
            EXPECT_CALL(res_rx_session_mock_, setTransferIdTimeout(Eq(0s)))  //
                .WillOnce(Return());
            EXPECT_CALL(res_rx_session_mock_, setOnReceiveCallback(_))  //
                .WillRepeatedly(Invoke([&](auto&& cb_fn) {              //
                    res_rx_cb_fn_ = std::forward<IResponseRxSession::OnReceiveCallback::Function>(cb_fn);
                }));

            const RequestTxParams tx_params{rx_params.service_id, rx_params.server_node_id};
            EXPECT_CALL(transport_mock, makeRequestTxSession(RequestTxParamsEq(tx_params)))  //
                .WillOnce(Invoke([&](const auto&) {                                          //
                    return libcyphal::detail::makeUniquePtr<UniquePtrReqTxSpec>(memory, req_tx_session_mock_);
                }));
            EXPECT_CALL(transport_mock, makeResponseRxSession(ResponseRxParamsEq(rx_params)))  //
                .WillOnce(Invoke([&](const auto&) {                                            //
                    return libcyphal::detail::makeUniquePtr<UniquePtrResRxSpec>(memory, res_rx_session_mock_);
                }));

            EXPECT_CALL(res_rx_session_mock_, deinit()).Times(1);
            EXPECT_CALL(req_tx_session_mock_, deinit()).Times(1);
        }

    };  // State

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

    constexpr ResponseRxParams rx_params{Service::Response::_traits_::ExtentBytes,
                                         Service::Request::_traits_::FixedPortId,
                                         0x31};

    const State state{mr_, transport_mock_, rx_params};

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
}

TEST_F(TestClient, request_response_get_fetch_result)
{
    using Service       = uavcan::node::GetInfo_1_0;
    using SvcResPromise = ResponsePromise<Service::Response>;

    Presentation presentation{mr_, scheduler_, transport_mock_};

    constexpr ResponseRxParams rx_params{Service::Response::_traits_::ExtentBytes,
                                         Service::Request::_traits_::FixedPortId,
                                         0x31};

    State state{mr_, transport_mock_, rx_params};

    auto maybe_client = presentation.makeClient<Service>(rx_params.server_node_id);
    ASSERT_THAT(maybe_client, VariantWith<ServiceClient<Service>>(_));
    const auto client = cetl::get<ServiceClient<Service>>(std::move(maybe_client));

    constexpr TransferId transfer_id = 0;
    ASSERT_TRUE(state.res_rx_cb_fn_);
    cetl::optional<SvcResPromise> response_promise;

    scheduler_.scheduleAt(1s, [&](const auto&) {
        //
        EXPECT_CALL(state.req_tx_session_mock_, send(_, _))  //
            .WillOnce(Invoke([now = now()](const auto& metadata, const auto) {
                //
                EXPECT_THAT(metadata.base.transfer_id, transfer_id);
                EXPECT_THAT(metadata.base.priority, Priority::Nominal);
                EXPECT_THAT(metadata.deadline, now + 100ms);
                return cetl::nullopt;
            }));

        auto maybe_promise = client.request(now() + 100ms, Service::Request{mr_alloc_}, now() + 2s);
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
        state.res_rx_cb_fn_({transfer});
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
        state.res_rx_cb_fn_({transfer});
    });
    scheduler_.spinFor(10s);
}

TEST_F(TestClient, request_response_via_callabck)
{
    using Service       = uavcan::node::GetInfo_1_0;
    using SvcResPromise = ResponsePromise<Service::Response>;

    Presentation presentation{mr_, scheduler_, transport_mock_};

    constexpr ResponseRxParams rx_params{Service::Response::_traits_::ExtentBytes,
                                         Service::Request::_traits_::FixedPortId,
                                         0x31};

    State state{mr_, transport_mock_, rx_params};

    auto maybe_client = presentation.makeClient<Service>(rx_params.server_node_id);
    ASSERT_THAT(maybe_client, VariantWith<ServiceClient<Service>>(_));
    auto client = cetl::get<ServiceClient<Service>>(std::move(maybe_client));

    constexpr TransferId transfer_id = 0;
    ASSERT_TRUE(state.res_rx_cb_fn_);
    cetl::optional<SvcResPromise> response_promise;

    NiceMock<ScatteredBufferStorageMock> storage_mock;
    EXPECT_CALL(storage_mock, deinit()).Times(1);
    EXPECT_CALL(storage_mock, size()).WillRepeatedly(Return(0));
    ScatteredBufferStorageMock::Wrapper storage{&storage_mock};

    std::vector<std::tuple<ServiceRxMetadata, TimePoint>> responses;

    scheduler_.scheduleAt(1s, [&](const auto&) {
        //
        EXPECT_CALL(state.req_tx_session_mock_, send(_, _))  //
            .WillOnce(Invoke([now = now()](const auto& metadata, const auto) {
                //
                EXPECT_THAT(metadata.base.transfer_id, transfer_id);
                EXPECT_THAT(metadata.base.priority, Priority::Nominal);
                EXPECT_THAT(metadata.deadline, now + 100ms);
                return cetl::nullopt;
            }));

        auto maybe_promise = client.request(now() + 100ms, Service::Request{mr_alloc_}, now() + 500ms);
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
            })
            .setDeadline(now() + 707ms);  // override previous +500ms deadline with new one (@2.007s)
    });
    scheduler_.scheduleAt(2s, [&](const auto&) {
        //
        EXPECT_THAT(responses, IsEmpty());

        ServiceRxTransfer transfer{{{{transfer_id, Priority::Nominal}, now()}, 0x31},
                                   ScatteredBuffer{std::move(storage)}};
        state.res_rx_cb_fn_({transfer});
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
        state.res_rx_cb_fn_({transfer});

        // Try set callback after the response has been received.
        response_promise->setCallback([](const auto&) {
            //
            FAIL() << "Unexpected dummy callback.";
        });

        // Also try set deadline after the response has been received.
        response_promise->setDeadline(now() + 1s);
    });
    scheduler_.scheduleAt(3s, [&](const auto&) {
        //
        EXPECT_CALL(state.req_tx_session_mock_, send(_, _)).WillOnce(Return(cetl::nullopt));

        auto maybe_promise = client.request(now() + 100ms, Service::Request{mr_alloc_}, now() + 500ms);
        EXPECT_THAT(maybe_promise, VariantWith<SvcResPromise>(_));
        response_promise.emplace(cetl::get<SvcResPromise>(std::move(maybe_promise)));

        response_promise->setCallback([](const auto&) {
            //
            FAIL() << "Unexpected dummy callback.";
        });
        response_promise->setCallback({});  // clear callback, so that `fetchResult` will return the result.

        ServiceRxTransfer transfer{{{{transfer_id + 1, Priority::Nominal}, now()}, 0x31}, {}};
        state.res_rx_cb_fn_({transfer});
    });
    scheduler_.scheduleAt(3s + 1ms, [&](const auto&) {
        //
        EXPECT_THAT(response_promise->getResult(), Optional(VariantWith<SvcResPromise::Success>(_)));
        EXPECT_THAT(response_promise->fetchResult(), Optional(VariantWith<SvcResPromise::Success>(_)));
    });
    scheduler_.spinFor(10s);

    EXPECT_THAT(responses,
                ElementsAre(
                    FieldsAre(ServiceRxMetadataEq({{{0, Priority::Nominal}, TimePoint{2s}}, 0x31}), TimePoint{2s})));
}

TEST_F(TestClient, request_response_set_callabck_after_reception)
{
    using Service       = uavcan::node::GetInfo_1_0;
    using SvcResPromise = ResponsePromise<Service::Response>;

    Presentation presentation{mr_, scheduler_, transport_mock_};

    constexpr ResponseRxParams rx_params{Service::Response::_traits_::ExtentBytes,
                                         Service::Request::_traits_::FixedPortId,
                                         0x31};

    State state{mr_, transport_mock_, rx_params};

    auto maybe_client = presentation.makeClient<Service>(rx_params.server_node_id);
    ASSERT_THAT(maybe_client, VariantWith<ServiceClient<Service>>(_));
    auto client = cetl::get<ServiceClient<Service>>(std::move(maybe_client));

    constexpr TransferId transfer_id = 0;
    ASSERT_TRUE(state.res_rx_cb_fn_);
    cetl::optional<SvcResPromise> response_promise;

    NiceMock<ScatteredBufferStorageMock> storage_mock;
    EXPECT_CALL(storage_mock, deinit()).Times(1);
    EXPECT_CALL(storage_mock, size()).WillRepeatedly(Return(0));
    ScatteredBufferStorageMock::Wrapper storage{&storage_mock};

    std::vector<std::tuple<ServiceRxMetadata, TimePoint>> responses;

    scheduler_.scheduleAt(1s, [&](const auto&) {
        //
        EXPECT_CALL(state.req_tx_session_mock_, send(_, _))  //
            .WillOnce(Invoke([now = now()](const auto& metadata, const auto) {
                //
                EXPECT_THAT(metadata.base.transfer_id, transfer_id);
                EXPECT_THAT(metadata.base.priority, Priority::Nominal);
                EXPECT_THAT(metadata.deadline, now + 100ms);
                return cetl::nullopt;
            }));

        auto maybe_promise = client.request(now() + 100ms, Service::Request{mr_alloc_}, now() + 200ms);
        EXPECT_THAT(maybe_promise, VariantWith<SvcResPromise>(_));
        response_promise.emplace(cetl::get<SvcResPromise>(std::move(maybe_promise)));
    });
    scheduler_.scheduleAt(1s + 100ms, [&](const auto&) {
        //
        ServiceRxTransfer transfer{{{{transfer_id, Priority::Nominal}, now()}, 0x31},
                                   ScatteredBuffer{std::move(storage)}};
        state.res_rx_cb_fn_({transfer});
    });
    scheduler_.scheduleAt(1s + 300ms, [&](const auto&) {
        //
        response_promise->setCallback([&responses](const auto& arg) {
            //
            ASSERT_THAT(arg.result, VariantWith<SvcResPromise::Success>(_));
            auto success = cetl::get<SvcResPromise::Success>(std::move(arg.result));
            responses.emplace_back(success.metadata, arg.approx_now);
        });
    });
    scheduler_.spinFor(10s);

    EXPECT_THAT(responses,
                ElementsAre(FieldsAre(ServiceRxMetadataEq({{{0, Priority::Nominal}, TimePoint{1s + 100ms}}, 0x31}),
                                      TimePoint{1s + 300ms})));
}

TEST_F(TestClient, request_response_failures)
{
    using Service       = my_custom::baz_1_0;
    using SvcResPromise = ResponsePromise<Service::Response>;

    StrictMock<MemoryResourceMock> mr_mock;
    mr_mock.redirectExpectedCallsTo(mr_);

    Presentation presentation{mr_mock, scheduler_, transport_mock_};

    constexpr ResponseRxParams rx_params{Service::Response::_traits_::ExtentBytes, 147, 0x31};

    State state{mr_, transport_mock_, rx_params};

    // Emulate that transport supports only 2 concurrent transfers by having module equal to 2^1.
    // This will make the client fail to make more than 2 request.
    EXPECT_CALL(transport_mock_, getProtocolParams()).WillRepeatedly(Return(ProtocolParams{2, 0, 0}));

    auto maybe_client = presentation.makeClient<Service>(rx_params.server_node_id, rx_params.service_id);
    ASSERT_THAT(maybe_client, VariantWith<ServiceClient<Service>>(_));
    const auto client = cetl::get<ServiceClient<Service>>(std::move(maybe_client));

    constexpr TransferId          transfer_id = 0;
    cetl::optional<SvcResPromise> response_promise;

    NiceMock<ScatteredBufferStorageMock> storage_mock;
    EXPECT_CALL(storage_mock, deinit()).Times(2);

    scheduler_.scheduleAt(1s, [&](const auto&) {
        //
        Service::Request request{mr_alloc_};
        request.some_stuff = Service::Request::_traits_::TypeOf::some_stuff{mr_alloc_};
        request.some_stuff.resize(32);  // this will make it fail to serialize
        const auto maybe_promise = client.request(now() + 100ms, request);
        EXPECT_THAT(maybe_promise,
                    VariantWith<ServiceClient<Service>::Failure>(
                        VariantWith<nunavut::support::Error>(nunavut::support::Error::SerializationBadArrayLength)));
    });
    scheduler_.scheduleAt(2s, [&](const auto&) {
        //
        // Emulate problem with sending the request.
        EXPECT_CALL(state.req_tx_session_mock_, send(_, _))  //
            .WillOnce(Return(CapacityError{}));

        const auto maybe_promise = client.request(now() + 100ms, Service::Request{mr_alloc_});
        EXPECT_THAT(maybe_promise, VariantWith<ServiceClient<Service>::Failure>(VariantWith<CapacityError>(_)));
    });
    scheduler_.scheduleAt(3s, [&](const auto&) {
        //
        EXPECT_CALL(state.req_tx_session_mock_, send(_, _)).WillOnce(Return(cetl::nullopt));

        auto maybe_promise = client.request(now() + 100ms, Service::Request{mr_alloc_});
        EXPECT_THAT(maybe_promise, VariantWith<SvcResPromise>(_));
        response_promise.emplace(cetl::get<SvcResPromise>(std::move(maybe_promise)));

        EXPECT_CALL(storage_mock, size()).WillRepeatedly(Return(1));
        EXPECT_CALL(storage_mock, copy(0, NotNull(), 1))
            .WillOnce(Invoke([](const auto, auto* const dst, const auto) {  //
                // this will make it fail to deserialize response with SerializationBadArrayLength
                *dst = cetl::byte(255);
                return 1;
            }));
        ScatteredBufferStorageMock::Wrapper storage{&storage_mock};

        ServiceRxTransfer transfer{{{{transfer_id + 1, Priority::Nominal}, now()}, 0x31},
                                   ScatteredBuffer{std::move(storage)}};
        state.res_rx_cb_fn_({transfer});

        scheduler_.scheduleAt(now() + 200ms, [&](const auto&) {
            //
            const auto result = response_promise->fetchResult();
            ASSERT_THAT(result,
                        Optional(VariantWith<ResponsePromiseFailure>(VariantWith<nunavut::support::Error>(
                            nunavut::support::Error::SerializationBadArrayLength))));
        });
    });
    scheduler_.scheduleAt(4s, [&](const auto&) {
        //
        using libcyphal::presentation::detail::SmallPayloadSize;

        EXPECT_CALL(state.req_tx_session_mock_, send(_, _)).WillOnce(Return(cetl::nullopt));

        auto maybe_promise = client.request(now() + 100ms, Service::Request{mr_alloc_});
        EXPECT_THAT(maybe_promise, VariantWith<SvcResPromise>(_));
        response_promise.emplace(cetl::get<SvcResPromise>(std::move(maybe_promise)));

        // Emulate that there is no memory available for the response deserialization.
        EXPECT_CALL(storage_mock, size()).WillRepeatedly(Return(SmallPayloadSize + 1));
        EXPECT_CALL(mr_mock, do_allocate(SmallPayloadSize + 1, _)).WillOnce(Return(nullptr));
        ScatteredBufferStorageMock::Wrapper storage{&storage_mock};

        ServiceRxTransfer transfer{{{{transfer_id + 0, Priority::Nominal}, now()}, 0x31},
                                   ScatteredBuffer{std::move(storage)}};
        state.res_rx_cb_fn_({transfer});

        scheduler_.scheduleAt(now() + 200ms, [&](const auto&) {
            //
            const auto result = response_promise->fetchResult();
            ASSERT_THAT(result, Optional(VariantWith<ResponsePromiseFailure>(VariantWith<libcyphal::MemoryError>(_))));
        });
    });
    scheduler_.scheduleAt(5s, [&](const auto&) {
        //
        EXPECT_CALL(state.req_tx_session_mock_, send(_, _)).WillRepeatedly(Return(cetl::nullopt));

        const auto maybe_promise1 = client.request(now() + 100ms, Service::Request{mr_alloc_});
        EXPECT_THAT(maybe_promise1, VariantWith<SvcResPromise>(_));

        const auto maybe_promise2 = client.request(now() + 100ms, Service::Request{mr_alloc_});
        EXPECT_THAT(maybe_promise2, VariantWith<SvcResPromise>(_));

        const auto maybe_promise3 = client.request(now() + 100ms, Service::Request{mr_alloc_});
        EXPECT_THAT(maybe_promise3,
                    VariantWith<ServiceClient<Service>::Failure>(
                        VariantWith<ServiceClient<Service>::TooManyPendingRequestsError>(_)));
    });
    scheduler_.spinFor(10s);
}

TEST_F(TestClient, multiple_requests_responses_expired)
{
    using Service       = uavcan::node::GetInfo_1_0;
    using SvcResPromise = ResponsePromise<Service::Response>;

    Presentation presentation{mr_, scheduler_, transport_mock_};

    constexpr ResponseRxParams rx_params{Service::Response::_traits_::ExtentBytes,
                                         Service::Request::_traits_::FixedPortId,
                                         0x31};

    State state{mr_, transport_mock_, rx_params};

    auto maybe_client = presentation.makeClient<Service>(rx_params.server_node_id);
    ASSERT_THAT(maybe_client, VariantWith<ServiceClient<Service>>(_));
    auto client = cetl::get<ServiceClient<Service>>(std::move(maybe_client));

    constexpr TransferId          transfer_id = 0;
    cetl::optional<SvcResPromise> response_promise1;
    cetl::optional<SvcResPromise> response_promise2;
    cetl::optional<SvcResPromise> response_promise3;
    cetl::optional<SvcResPromise> response_promise4;

    std::vector<std::tuple<std::string, TimePoint, TimePoint>> responses;

    scheduler_.scheduleAt(1s, [&](const auto&) {
        //
        const TransferTxMetadata meta{{transfer_id + 0, Priority::Nominal}, now() + 100ms};
        EXPECT_CALL(state.req_tx_session_mock_, send(TransferTxMetadataEq(meta), _))  //
            .WillOnce(Return(cetl::nullopt));

        auto maybe_promise = client.request(now() + 100ms, Service::Request{mr_alloc_}, now() + 4s);
        EXPECT_THAT(maybe_promise, VariantWith<SvcResPromise>(_));
        response_promise1.emplace(cetl::get<SvcResPromise>(std::move(maybe_promise)));
        response_promise1->setCallback([&responses](const auto& arg) {
            //
            ASSERT_THAT(arg.result, VariantWith<ResponsePromiseFailure>(VariantWith<ResponsePromiseExpired>(_)));
            auto failure = cetl::get<ResponsePromiseFailure>(std::move(arg.result));
            auto expired = cetl::get<ResponsePromiseExpired>(std::move(failure));
            responses.emplace_back("1", expired.deadline, arg.approx_now);
        });
    });
    scheduler_.scheduleAt(2s, [&](const auto&) {
        //
        client.setPriority(Priority::Fast);
        const TransferTxMetadata meta{{transfer_id + 1, client.getPriority()}, now() + 200ms};
        EXPECT_CALL(state.req_tx_session_mock_, send(TransferTxMetadataEq(meta), _))  //
            .WillOnce(Return(cetl::nullopt));

        auto maybe_promise = client.request(now() + 200ms, Service::Request{mr_alloc_}, now() + 2s);
        EXPECT_THAT(maybe_promise, VariantWith<SvcResPromise>(_));
        response_promise2.emplace(cetl::get<SvcResPromise>(std::move(maybe_promise)));
        response_promise2->setCallback([&responses](const auto& arg) {
            //
            ASSERT_THAT(arg.result, VariantWith<ResponsePromiseFailure>(VariantWith<ResponsePromiseExpired>(_)));
            auto failure = cetl::get<ResponsePromiseFailure>(std::move(arg.result));
            auto expired = cetl::get<ResponsePromiseExpired>(std::move(failure));
            responses.emplace_back("2", expired.deadline, arg.approx_now);
        });
    });
    scheduler_.scheduleAt(3s, [&](const auto&) {
        //
        const TransferTxMetadata meta{{transfer_id + 2, client.getPriority()}, now() + 300ms};
        EXPECT_CALL(state.req_tx_session_mock_, send(TransferTxMetadataEq(meta), _))  //
            .WillOnce(Return(cetl::nullopt));

        auto maybe_promise = client.request(now() + 300ms, Service::Request{mr_alloc_}, now() + 1s);
        EXPECT_THAT(maybe_promise, VariantWith<SvcResPromise>(_));
        response_promise3.emplace(cetl::get<SvcResPromise>(std::move(maybe_promise)));
        response_promise3->setCallback([&responses](const auto& arg) {
            //
            ASSERT_THAT(arg.result, VariantWith<ResponsePromiseFailure>(VariantWith<ResponsePromiseExpired>(_)));
            auto failure = cetl::get<ResponsePromiseFailure>(std::move(arg.result));
            auto expired = cetl::get<ResponsePromiseExpired>(std::move(failure));
            responses.emplace_back("3", expired.deadline, arg.approx_now);
        });
    });
    scheduler_.scheduleAt(4s, [&](const auto&) {
        //
        const TransferTxMetadata meta{{transfer_id + 3, client.getPriority()}, now() + 400ms};
        EXPECT_CALL(state.req_tx_session_mock_, send(TransferTxMetadataEq(meta), _))  //
            .WillOnce(Return(cetl::nullopt));

        auto maybe_promise = client.request(now() + 400ms, Service::Request{mr_alloc_}, now() + 2s);
        EXPECT_THAT(maybe_promise, VariantWith<SvcResPromise>(_));
        response_promise4.emplace(cetl::get<SvcResPromise>(std::move(maybe_promise)));
        response_promise4->setCallback([](const auto&) {
            //
            FAIL() << "Unexpected dummy callback for cancelled promise.";
        });
    });
    scheduler_.scheduleAt(5s, [&](const auto&) {
        //
        response_promise4.reset();
    });
    scheduler_.spinFor(10s);

    EXPECT_THAT(responses,
                ElementsAre(FieldsAre("2", TimePoint{4000ms}, TimePoint{4000ms}),
                            FieldsAre("3", TimePoint{4000ms}, TimePoint{4000ms}),
                            FieldsAre("1", TimePoint{5000ms}, TimePoint{5000ms})));
}

TEST_F(TestClient, raw_request_response_via_callabck)
{
    using SvcResPromise = ResponsePromise<void>;

    Presentation presentation{mr_, scheduler_, transport_mock_};

    constexpr ResponseRxParams rx_params{16, 147, 0x31};

    State state{mr_, transport_mock_, rx_params};

    auto maybe_client = presentation.makeClient(rx_params.server_node_id, rx_params.service_id, rx_params.extent_bytes);
    ASSERT_THAT(maybe_client, VariantWith<RawServiceClient>(_));
    auto client = cetl::get<RawServiceClient>(std::move(maybe_client));

    constexpr TransferId transfer_id = 0;
    ASSERT_TRUE(state.res_rx_cb_fn_);
    cetl::optional<SvcResPromise> response_promise;

    NiceMock<ScatteredBufferStorageMock> storage_mock;
    EXPECT_CALL(storage_mock, deinit()).Times(1);
    EXPECT_CALL(storage_mock, size()).WillRepeatedly(Return(7));
    ScatteredBufferStorageMock::Wrapper storage{&storage_mock};

    std::vector<std::tuple<std::size_t, ServiceRxMetadata, TimePoint>> responses;

    scheduler_.scheduleAt(1s, [&](const auto&) {
        //
        EXPECT_CALL(state.req_tx_session_mock_, send(_, _))  //
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
        state.res_rx_cb_fn_({transfer});
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
        state.res_rx_cb_fn_({transfer});

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
}

TEST_F(TestClient, raw_request_response_failures)
{
    using SvcResPromise = ResponsePromise<void>;

    Presentation presentation{mr_, scheduler_, transport_mock_};

    constexpr ResponseRxParams rx_params{4, 147, 0x31};

    State state{mr_, transport_mock_, rx_params};

    // Emulate that transport supports only 2 concurrent transfers by having module equal to 2^1.
    // This will make the client fail to make more than 2 request.
    EXPECT_CALL(transport_mock_, getProtocolParams()).WillRepeatedly(Return(ProtocolParams{2, 0, 0}));

    auto maybe_client = presentation.makeClient(rx_params.server_node_id, rx_params.service_id, rx_params.extent_bytes);
    ASSERT_THAT(maybe_client, VariantWith<RawServiceClient>(_));
    const auto client = cetl::get<RawServiceClient>(std::move(maybe_client));

    scheduler_.scheduleAt(1s, [&](const auto&) {
        //
        // Emulate problem with sending the request.
        EXPECT_CALL(state.req_tx_session_mock_, send(_, _))  //
            .WillOnce(Return(CapacityError{}));

        const auto maybe_promise = client.request(now() + 100ms, {});
        EXPECT_THAT(maybe_promise, VariantWith<RawServiceClient::Failure>(VariantWith<CapacityError>(_)));
    });
    scheduler_.scheduleAt(2s, [&](const auto&) {
        //
        EXPECT_CALL(state.req_tx_session_mock_, send(_, _)).WillRepeatedly(Return(cetl::nullopt));

        const auto maybe_promise1 = client.request(now() + 100ms, {});
        EXPECT_THAT(maybe_promise1, VariantWith<SvcResPromise>(_));

        const auto maybe_promise2 = client.request(now() + 100ms, {});
        EXPECT_THAT(maybe_promise2, VariantWith<SvcResPromise>(_));

        const auto maybe_promise3 = client.request(now() + 100ms, {});
        EXPECT_THAT(maybe_promise3,
                    VariantWith<RawServiceClient::Failure>(
                        VariantWith<RawServiceClient::TooManyPendingRequestsError>(_)));
    });
    scheduler_.spinFor(10s);
}

// NOLINTEND(cppcoreguidelines-avoid-magic-numbers, readability-magic-numbers, *-function-cognitive-complexity)

}  // namespace
