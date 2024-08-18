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

TEST_F(TestPresentation, xxx) {}

// NOLINTEND(cppcoreguidelines-avoid-magic-numbers, readability-magic-numbers)

}  // namespace
