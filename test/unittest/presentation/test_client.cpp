/// @copyright
/// Copyright (C) OpenCyphal Development Team  <opencyphal.org>
/// Copyright Amazon.com Inc. or its affiliates.
/// SPDX-License-Identifier: MIT

// NOLINTBEGIN(misc-include-cleaner, misc-unused-using-decls)

#include "cetl_gtest_helpers.hpp"  // NOLINT(misc-include-cleaner)
#include "gtest_helpers.hpp"       // NOLINT(misc-include-cleaner)
#include "tracking_memory_resource.hpp"
#include "transport/svc_sessions_mock.hpp"
#include "transport/transport_gtest_helpers.hpp"
#include "transport/transport_mock.hpp"
#include "virtual_time_scheduler.hpp"

#include <cetl/pf17/cetlpf.hpp>
#include <libcyphal/presentation/client.hpp>
#include <libcyphal/presentation/presentation.hpp>
#include <libcyphal/transport/errors.hpp>
#include <libcyphal/transport/svc_sessions.hpp>
#include <libcyphal/transport/types.hpp>
#include <libcyphal/types.hpp>

#include <uavcan/node/GetInfo_1_0.hpp>

#include <gmock/gmock.h>
#include <gtest/gtest.h>

#include <cstring>
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
using testing::StrictMock;
using testing::VariantWith;

// https://github.com/llvm/llvm-project/issues/53444
// NOLINTBEGIN(misc-unused-using-decls, misc-include-cleaner)
using std::literals::chrono_literals::operator""s;
using std::literals::chrono_literals::operator""ms;
// NOLINTEND(misc-unused-using-decls, misc-include-cleaner)

// NOLINTBEGIN(cppcoreguidelines-avoid-magic-numbers, readability-magic-numbers)

class TestClient : public testing::Test
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
        .WillOnce(Invoke([&](const auto&) {
            return libcyphal::detail::makeUniquePtr<RequestTxSessionMock::RefWrapper::Spec>(mr_, req_tx_session_mock);
        }));
    EXPECT_CALL(transport_mock_, makeResponseRxSession(ResponseRxParamsEq(rx_params)))  //
        .WillOnce(Invoke([&](const auto&) {
            return libcyphal::detail::makeUniquePtr<ResponseRxSessionMock::RefWrapper::Spec>(mr_, res_rx_session_mock);
        }));

    auto maybe_client1 = presentation.makeClient<Service>(0x31);
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

// NOLINTEND(cppcoreguidelines-avoid-magic-numbers, readability-magic-numbers)
// NOLINTEND(misc-include-cleaner, misc-unused-using-decls)

}  // namespace
