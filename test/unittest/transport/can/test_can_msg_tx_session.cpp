/// @copyright
/// Copyright (C) OpenCyphal Development Team  <opencyphal.org>
/// Copyright Amazon.com Inc. or its affiliates.
/// SPDX-License-Identifier: MIT

#include <libcyphal/transport/can/transport.hpp>

#include "media_mock.hpp"
#include "../multiplexer_mock.hpp"
#include "../../gtest_helpers.hpp"
#include "../../memory_resource_mock.hpp"
#include "../../tracking_memory_resource.hpp"

#include <gmock/gmock.h>

namespace
{
using namespace libcyphal;
using namespace libcyphal::transport;
using namespace libcyphal::transport::can;

using testing::_;
using testing::Eq;
using testing::Return;
using testing::IsNull;
using testing::IsEmpty;
using testing::NotNull;
using testing::Optional;
using testing::StrictMock;
using testing::VariantWith;

using namespace std::chrono_literals;

class TestCanMsgTxSession : public testing::Test
{
protected:
    void SetUp() override
    {
        EXPECT_CALL(media_mock_, getMtu()).WillRepeatedly(Return(CANARD_MTU_MAX));
    }

    void TearDown() override
    {
        EXPECT_THAT(mr_.allocations, IsEmpty());
        // TODO: Uncomment this when PMR deleter is fixed.
        // EXPECT_EQ(mr_.total_allocated_bytes, mr_.total_deallocated_bytes);
    }

    CETL_NODISCARD UniquePtr<ICanTransport> makeTransport(cetl::pmr::memory_resource& mr,
                                                          const std::size_t           tx_capacity = 16)
    {
        std::array<IMedia*, 1> media_array{&media_mock_};

        auto maybe_transport = can::makeTransport(mr, mux_mock_, media_array, tx_capacity, {});
        EXPECT_THAT(maybe_transport, VariantWith<UniquePtr<ICanTransport>>(NotNull()));
        return cetl::get<UniquePtr<ICanTransport>>(std::move(maybe_transport));
    }

    // MARK: Data members:

    TrackingMemoryResource      mr_;
    StrictMock<MediaMock>       media_mock_{};
    StrictMock<MultiplexerMock> mux_mock_{};
};

// MARK: Tests:

TEST_F(TestCanMsgTxSession, make)
{
    auto transport = makeTransport(mr_);

    auto maybe_session = transport->makeMessageTxSession({123});
    EXPECT_THAT(maybe_session, VariantWith<UniquePtr<IMessageTxSession>>(_));
    auto session = cetl::get<UniquePtr<IMessageTxSession>>(std::move(maybe_session));
    EXPECT_THAT(session, NotNull());

    EXPECT_THAT(session->getParams().subject_id, Eq(123));
}

TEST_F(TestCanMsgTxSession, make_no_memory)
{
    StrictMock<MemoryResourceMock> mr_mock{};
    mr_mock.redirectExpectedCallsTo(mr_);

    // Emulate that there is no memory available for the message session.
    EXPECT_CALL(mr_mock, do_allocate(sizeof(can::detail::MessageTxSession), _)).WillOnce(Return(nullptr));

    auto transport = makeTransport(mr_mock);

    auto maybe_session = transport->makeMessageTxSession({0x23});
    EXPECT_THAT(maybe_session, VariantWith<AnyError>(VariantWith<MemoryError>(_)));
}

TEST_F(TestCanMsgTxSession, send_empty_payload_and_no_transport_run)
{
    StrictMock<MemoryResourceMock> mr_mock{};
    mr_mock.redirectExpectedCallsTo(mr_);

    auto transport = makeTransport(mr_mock);

    auto maybe_session = transport->makeMessageTxSession({123});
    EXPECT_THAT(maybe_session, VariantWith<UniquePtr<IMessageTxSession>>(_));
    auto session = cetl::get<UniquePtr<IMessageTxSession>>(std::move(maybe_session));
    EXPECT_THAT(session, NotNull());

    const PayloadFragments empty_payload{};

    auto maybe_error = session->send({0x1AF52, TimePoint{1s}, Priority::Low}, empty_payload);
    EXPECT_THAT(maybe_error, Eq(cetl::nullopt));
}

}  // namespace
