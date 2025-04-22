/// @copyright
/// Copyright (C) OpenCyphal Development Team  <opencyphal.org>
/// Copyright Amazon.com Inc. or its affiliates.
/// SPDX-License-Identifier: MIT

// The main purpose of this test file is make sure that CAN transport
// could be compiled with custom time representation (32-bit milliseconds) instead of the default one.
// Milliseconds are chosen b/c there is no implicit conversion from native lizard's microseconds
// to lower precision units like milliseconds, so proper explicit `std::chrono::duration_cast` is needed.
// For details also see https://github.com/OpenCyphal-Garage/libcyphal/issues/431.
#include "custom_libcyphal_config.hpp"  // NOLINT(misc-include-cleaner)

#include "media_mock.hpp"
#include "tracking_memory_resource.hpp"
#include "virtual_time_scheduler.hpp"

#include <canard.h>
#include <cetl/pf17/cetlpf.hpp>
#include <libcyphal/transport/can/can_transport.hpp>
#include <libcyphal/transport/can/can_transport_impl.hpp>
#include <libcyphal/transport/can/media.hpp>
#include <libcyphal/types.hpp>

#include <gmock/gmock.h>
#include <gtest/gtest.h>

#include <array>
#include <utility>

namespace
{

using libcyphal::UniquePtr;
using namespace libcyphal::transport::can;  // NOLINT This our main concern here in the unit tests.

using testing::Eq;
using testing::Return;
using testing::IsEmpty;
using testing::NotNull;
using testing::ReturnRef;
using testing::StrictMock;
using testing::VariantWith;

class TestCanTransportCustomConfigMilliseconds : public testing::Test
{
protected:
    void SetUp() override
    {
        cetl::pmr::set_default_resource(&mr_);

        EXPECT_CALL(media_mock_, getMtu()).WillRepeatedly(Return(CANARD_MTU_CAN_CLASSIC));
        EXPECT_CALL(media_mock_, getTxMemoryResource()).WillRepeatedly(ReturnRef(mr_));
    }

    void TearDown() override
    {
        EXPECT_THAT(mr_.allocations, IsEmpty());
        EXPECT_THAT(mr_.total_allocated_bytes, mr_.total_deallocated_bytes);
    }

    // MARK: Data members:

    // NOLINTBEGIN
    libcyphal::VirtualTimeScheduler scheduler_{};
    TrackingMemoryResource          mr_;
    StrictMock<MediaMock>           media_mock_{};
    // NOLINTEND
};

// MARK: - Tests:

TEST_F(TestCanTransportCustomConfigMilliseconds, makeTransport_getLocalNodeId)
{
    std::array<IMedia*, 1> media_array{&media_mock_};
    auto                   maybe_transport = makeTransport(mr_, scheduler_, media_array, 0);
    ASSERT_THAT(maybe_transport, VariantWith<UniquePtr<ICanTransport>>(NotNull()));

    const auto transport = cetl::get<UniquePtr<ICanTransport>>(std::move(maybe_transport));
    EXPECT_THAT(transport->getLocalNodeId(), Eq(cetl::nullopt));
}

}  // namespace
