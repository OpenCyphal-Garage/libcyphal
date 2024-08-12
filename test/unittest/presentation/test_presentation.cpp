/// @copyright
/// Copyright (C) OpenCyphal Development Team  <opencyphal.org>
/// Copyright Amazon.com Inc. or its affiliates.
/// SPDX-License-Identifier: MIT

#include "../tracking_memory_resource.hpp"
#include "../transport/msg_sessions_mock.hpp"
#include "../transport/svc_sessions_mock.hpp"
#include "../transport/transport_mock.hpp"
#include "../virtual_time_scheduler.hpp"

#include <libcyphal/presentation/presentation.hpp>
#include <libcyphal/transport/msg_sessions.hpp>
#include <libcyphal/types.hpp>

#include <gmock/gmock.h>
#include <gtest/gtest.h>

namespace
{

using libcyphal::TimePoint;
using libcyphal::UniquePtr;
using libcyphal::detail::makeUniquePtr;
using namespace libcyphal::presentation;  // NOLINT This our main concern here in the unit tests.
using namespace libcyphal::transport;     // NOLINT This our main concern here in the unit tests.

using testing::_;
using testing::Invoke;
using testing::IsEmpty;
using testing::NotNull;
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
    TransportMock                   transport_mock_;
    // NOLINTEND
};

// MARK: - Tests:

TEST_F(TestPresentation, xxx)
{
    const Presentation    presentation{mr_, transport_mock_};
    MessageRxSessionMock  msg_rx_session_mock;
    MessageTxSessionMock  msg_tx_session_mock;
    RequestRxSessionMock  svc_req_rx_session_mock;
    RequestTxSessionMock  svc_req_tx_session_mock;
    ResponseRxSessionMock svc_res_rx_session_mock;
    ResponseTxSessionMock svc_res_tx_session_mock;

    EXPECT_CALL(transport_mock_, makeMessageRxSession(_))  //
        .WillRepeatedly(Invoke([&](const auto&) {
            return makeUniquePtr<MessageRxSessionMock::RefWrapper::Spec>(mr_, msg_rx_session_mock);
        }));
    EXPECT_CALL(transport_mock_, makeMessageTxSession(_))  //
        .WillRepeatedly(Invoke([&](const auto&) {
            return makeUniquePtr<MessageTxSessionMock::RefWrapper::Spec>(mr_, msg_tx_session_mock);
        }));
    EXPECT_CALL(transport_mock_, makeRequestRxSession(_))  //
        .WillRepeatedly(Invoke([&](const auto&) {
            return makeUniquePtr<RequestRxSessionMock::RefWrapper::Spec>(mr_, svc_req_rx_session_mock);
        }));
    EXPECT_CALL(transport_mock_, makeRequestTxSession(_))  //
        .WillRepeatedly(Invoke([&](const auto&) {
            return makeUniquePtr<RequestTxSessionMock::RefWrapper::Spec>(mr_, svc_req_tx_session_mock);
        }));
    EXPECT_CALL(transport_mock_, makeResponseRxSession(_))  //
        .WillRepeatedly(Invoke([&](const auto&) {
            return makeUniquePtr<ResponseRxSessionMock::RefWrapper::Spec>(mr_, svc_res_rx_session_mock);
        }));
    EXPECT_CALL(transport_mock_, makeResponseTxSession(_))  //
        .WillRepeatedly(Invoke([&](const auto&) {
            return makeUniquePtr<ResponseTxSessionMock::RefWrapper::Spec>(mr_, svc_res_tx_session_mock);
        }));

    auto maybe_msg_rx_session = transport_mock_.makeMessageRxSession({16, 0x7B});
    ASSERT_THAT(maybe_msg_rx_session, VariantWith<UniquePtr<IMessageRxSession>>(NotNull()));
}

// NOLINTEND(cppcoreguidelines-avoid-magic-numbers, readability-magic-numbers)

}  // namespace
