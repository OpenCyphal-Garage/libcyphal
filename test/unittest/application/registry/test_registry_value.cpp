/// @copyright
/// Copyright (C) OpenCyphal Development Team  <opencyphal.org>
/// Copyright Amazon.com Inc. or its affiliates.
/// SPDX-License-Identifier: MIT

#include "tracking_memory_resource.hpp"

#include <cetl/pf17/cetlpf.hpp>
#include <libcyphal/application/registry/value.hpp>

#include <gmock/gmock.h>
#include <gtest/gtest.h>

namespace
{
using namespace libcyphal::application::registry;  // NOLINT This our main concern here in the unit tests.

using testing::IsEmpty;

// NOLINTBEGIN(cppcoreguidelines-avoid-magic-numbers, readability-magic-numbers)

class TestRegistryValue : public testing::Test
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

TEST_F(TestRegistryValue, AssignmentBasic)
{
    Value::allocator_type alloc{&mr_};

    Value v = makeValue(alloc, true, false, true, false, false, false, false, true, false);
    EXPECT_TRUE(v.is_bit());
    EXPECT_THAT(v.get_bit().value.size(), 9);
    EXPECT_TRUE(v.get_bit().value[0]);
    EXPECT_FALSE(v.get_bit().value[1]);
    EXPECT_TRUE(v.get_bit().value[2]);
    EXPECT_FALSE(v.get_bit().value[3]);
    EXPECT_FALSE(v.get_bit().value[4]);
    EXPECT_FALSE(v.get_bit().value[5]);
    EXPECT_FALSE(v.get_bit().value[6]);
    EXPECT_TRUE(v.get_bit().value[7]);
    EXPECT_FALSE(v.get_bit().value[8]);
}

TEST_F(TestRegistryValue, isVariableSize)
{
    using detail::isVariableSize;

    Value v;

    v.set_empty();
    EXPECT_FALSE(isVariableSize(v));

    v.set_string();
    EXPECT_TRUE(isVariableSize(v));

    v.set_unstructured();
    EXPECT_TRUE(isVariableSize(v));

    v.set_bit();
    EXPECT_FALSE(isVariableSize(v));

    v.set_integer64();
    EXPECT_FALSE(isVariableSize(v));

    v.set_real64();
    EXPECT_FALSE(isVariableSize(v));
}

// NOLINTEND(cppcoreguidelines-avoid-magic-numbers, readability-magic-numbers)

}  // namespace
