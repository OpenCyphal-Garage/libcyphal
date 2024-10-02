/// @copyright
/// Copyright (C) OpenCyphal Development Team  <opencyphal.org>
/// Copyright Amazon.com Inc. or its affiliates.
/// SPDX-License-Identifier: MIT

#include "tracking_memory_resource.hpp"

#include <cetl/pf17/cetlpf.hpp>
#include <libcyphal/application/registry/register_impl.hpp>

#include <gmock/gmock.h>
#include <gtest/gtest.h>

namespace
{

using namespace libcyphal::application::registry;  // NOLINT This our main concern here in the unit tests.

using testing::IsEmpty;

// NOLINTBEGIN(cppcoreguidelines-avoid-magic-numbers, readability-magic-numbers)

class TestRegister : public testing::Test
{
protected:
    void SetUp() override
    {
        cetl::pmr::set_default_resource(&mr_default_);
    }

    void TearDown() override
    {
        EXPECT_THAT(mr_.allocations, IsEmpty());
        EXPECT_THAT(mr_.total_allocated_bytes, mr_.total_deallocated_bytes);

        EXPECT_THAT(mr_default_.allocations, IsEmpty());
        EXPECT_THAT(mr_default_.total_allocated_bytes, 0);
        EXPECT_THAT(mr_default_.total_allocated_bytes, mr_default_.total_deallocated_bytes);
    }

    // MARK: Data members:

    // NOLINTBEGIN
    TrackingMemoryResource mr_;
    TrackingMemoryResource mr_default_;
    // NOLINTEND

};  // TestRegistry

// MARK: - Tests:

TEST_F(TestRegister, makeParam)
{
    Registry rgy{mr_};

    ParamRegister bool_param{rgy, "bool_param", true};
}

// NOLINTEND(cppcoreguidelines-avoid-magic-numbers, readability-magic-numbers)

}  // namespace
