/// @file
/// Unittest of libcyphal::network::frame type.
///
/// @copyright
/// Copyright (C) OpenCyphal Development Team  <opencyphal.org>
/// Copyright Amazon.com Inc. or its affiliates.
/// SPDX-License-Identifier: MIT
///

#include "libcyphal/network/frame.hpp"
#include "cetl/pf17/byte.hpp"

#include "libcyphal/unittest.hpp"

namespace libcyphal
{
namespace network
{

template <typename ByteType>
class NetworkFrameTest : public ::testing::Test
{
};

///
/// Since we don't enable RTTI we supply this name generator to make these tests more legible.
///
struct ByteTypeNameGenerator
{
    ///
    /// Supports the "NameGenerator" concept in gtest.
    ///
    /// @tparam T The typename TypeParam of the parameterized test.
    /// @param  i The index number of the  TYPED_TEST
    /// @return A string which identifies the test case.
    ///
    template <typename T>
    static std::string GetName(int i)
    {
        std::stringstream out;
        out << i;
        out << " ";
        T::to_string(out);
        return out.str();
    }
};

using ByteTypes = ::testing::Types<cetl::pf17::byte>;

TYPED_TEST_SUITE(NetworkFrameTest, ByteTypes, ByteTypeNameGenerator);

TEST(NetworkFrame, HelloWorld)
{
    Frame<24> subject;
}

}  // namespace network
}  // namespace libcyphal
