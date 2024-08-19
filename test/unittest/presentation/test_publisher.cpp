/// @copyright
/// Copyright (C) OpenCyphal Development Team  <opencyphal.org>
/// Copyright Amazon.com Inc. or its affiliates.
/// SPDX-License-Identifier: MIT

#include "../tracking_memory_resource.hpp"
#include "../transport/msg_sessions_mock.hpp"
#include "../transport/transport_mock.hpp"
#include "../virtual_time_scheduler.hpp"

#include <cetl/pf17/cetlpf.hpp>
#include <libcyphal/presentation/presentation.hpp>
#include <libcyphal/presentation/publisher.hpp>
#include <libcyphal/transport/msg_sessions.hpp>
#include <libcyphal/transport/types.hpp>
#include <libcyphal/types.hpp>

#include <uavcan/node/Heartbeat_1_0.hpp>

#include <gmock/gmock.h>
#include <gtest/gtest.h>

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

// NOLINTBEGIN(cppcoreguidelines-avoid-magic-numbers, readability-magic-numbers)

class TestPublisher : public testing::Test
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

TEST_F(TestPublisher, move_copy_getSetPriority)
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
    ASSERT_THAT(maybe_pub1, VariantWith<Publisher<Message>>(_));
    auto pub1a = cetl::get<Publisher<Message>>(std::move(maybe_pub1));
    EXPECT_THAT(pub1a.getPriority(), Priority::Nominal);

    pub1a.setPriority(Priority::Immediate);
    EXPECT_THAT(pub1a.getPriority(), Priority::Immediate);

    auto pub1b = std::move(pub1a);
    EXPECT_THAT(pub1b.getPriority(), Priority::Immediate);

    auto pub2 = pub1b;
    EXPECT_THAT(pub2.getPriority(), Priority::Immediate);
    pub2.setPriority(Priority::Slow);
    EXPECT_THAT(pub2.getPriority(), Priority::Slow);
    EXPECT_THAT(pub1b.getPriority(), Priority::Immediate);

    pub1b = pub2;
    EXPECT_THAT(pub1b.getPriority(), Priority::Slow);

    // Verify self-assignment.
    auto& pub1c = pub1b;
    pub1c = pub1b;

    pub2.setPriority(Priority::Optional);
    pub1c = std::move(pub2);
    EXPECT_THAT(pub1c.getPriority(), Priority::Optional);

    EXPECT_CALL(msg_tx_session_mock, deinit()).Times(1);
}

// NOLINTEND(cppcoreguidelines-avoid-magic-numbers, readability-magic-numbers)

}  // namespace
