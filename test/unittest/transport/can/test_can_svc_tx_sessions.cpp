/// @copyright
/// Copyright (C) OpenCyphal Development Team  <opencyphal.org>
/// Copyright Amazon.com Inc. or its affiliates.
/// SPDX-License-Identifier: MIT

#include <libcyphal/transport/can/transport.hpp>

#include "media_mock.hpp"
#include "../multiplexer_mock.hpp"
#include "../../gtest_helpers.hpp"
#include "../../test_scheduler.hpp"
#include "../../test_utilities.hpp"
#include "../../memory_resource_mock.hpp"
#include "../../tracking_memory_resource.hpp"

#include <gmock/gmock.h>

namespace
{
using byte = cetl::byte;

using namespace libcyphal;
using namespace libcyphal::transport;
using namespace libcyphal::transport::can;
using namespace libcyphal::test_utilities;

using testing::_;
using testing::Eq;
using testing::Return;
using testing::IsNull;
using testing::IsEmpty;
using testing::NotNull;
using testing::Optional;
using testing::InSequence;
using testing::StrictMock;
using testing::ElementsAre;
using testing::VariantWith;

using namespace std::chrono_literals;

class TestCanSvcTxSessions : public testing::Test
{
protected:
    void SetUp() override
    {
        EXPECT_CALL(media_mock_, getMtu()).WillRepeatedly(Return(CANARD_MTU_CAN_CLASSIC));
    }

    void TearDown() override
    {
        EXPECT_THAT(mr_.allocations, IsEmpty());
        // TODO: Uncomment this when PMR deleter is fixed.
        // EXPECT_EQ(mr_.total_allocated_bytes, mr_.total_deallocated_bytes);
    }

    TimePoint now() const
    {
        return scheduler_.now();
    }

    CETL_NODISCARD UniquePtr<ICanTransport> makeTransport(cetl::pmr::memory_resource& mr,
                                                          const NodeId                local_node_id,
                                                          const std::size_t           tx_capacity = 16)
    {
        std::array<IMedia*, 1> media_array{&media_mock_};

        // TODO: `local_node_id` could be just passed to `can::makeTransport` as an argument,
        // but it's not possible due to CETL issue https://github.com/OpenCyphal/CETL/issues/119.
        const auto opt_local_node_id = cetl::optional<NodeId>{local_node_id};

        auto maybe_transport = can::makeTransport(mr, mux_mock_, media_array, tx_capacity, opt_local_node_id);
        EXPECT_THAT(maybe_transport, VariantWith<UniquePtr<ICanTransport>>(NotNull()));
        return cetl::get<UniquePtr<ICanTransport>>(std::move(maybe_transport));
    }

    // MARK: Data members:

    TestScheduler               scheduler_{};
    TrackingMemoryResource      mr_;
    StrictMock<MultiplexerMock> mux_mock_{};
    StrictMock<MediaMock>       media_mock_{};
};

// MARK: Tests:

TEST_F(TestCanSvcTxSessions, make_request_session)
{
    auto transport = makeTransport(mr_, 0);

    auto maybe_session = transport->makeRequestTxSession({123, CANARD_NODE_ID_MAX});
    ASSERT_THAT(maybe_session, VariantWith<UniquePtr<IRequestTxSession>>(_));
    auto session = cetl::get<UniquePtr<IRequestTxSession>>(std::move(maybe_session));
    EXPECT_THAT(session, NotNull());

    EXPECT_THAT(session->getParams().service_id, 123);
    EXPECT_THAT(session->getParams().server_node_id, CANARD_NODE_ID_MAX);

    session->run(now());
}

TEST_F(TestCanSvcTxSessions, make_request_fails_due_to_argument_error)
{
    auto transport = makeTransport(mr_, 0);

    // Try invalid service id
    {
        auto maybe_session = transport->makeRequestTxSession({CANARD_SERVICE_ID_MAX + 1, 0});
        EXPECT_THAT(maybe_session, VariantWith<AnyError>(VariantWith<ArgumentError>(_)));
    }

    // Try invalid server node id
    {
        auto maybe_session = transport->makeRequestTxSession({0, CANARD_NODE_ID_MAX + 1});
        EXPECT_THAT(maybe_session, VariantWith<AnyError>(VariantWith<ArgumentError>(_)));
    }
}

TEST_F(TestCanSvcTxSessions, make_response_session)
{
    auto transport = makeTransport(mr_, CANARD_NODE_ID_MAX, 2);

    auto maybe_session = transport->makeResponseTxSession({123});
    ASSERT_THAT(maybe_session, VariantWith<UniquePtr<IResponseTxSession>>(_));
    auto session = cetl::get<UniquePtr<IResponseTxSession>>(std::move(maybe_session));
    EXPECT_THAT(session, NotNull());

    EXPECT_THAT(session->getParams().service_id, 123);

    session->run(now());
}

TEST_F(TestCanSvcTxSessions, make_response_fails_due_to_argument_error)
{
    auto transport = makeTransport(mr_, 0);

    // Try invalid service id
    auto maybe_session = transport->makeResponseTxSession({CANARD_SERVICE_ID_MAX + 1});
    EXPECT_THAT(maybe_session, VariantWith<AnyError>(VariantWith<ArgumentError>(_)));
}

}  // namespace
