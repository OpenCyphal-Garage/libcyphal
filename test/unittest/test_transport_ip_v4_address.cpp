/// @file
/// Test of crap in the IP v4 address class.
///
/// @copyright
/// Copyright (C) OpenCyphal Development Team  <opencyphal.org>
/// Copyright Amazon.com Inc. or its affiliates.
/// SPDX-License-Identifier: MIT
///
// cSpell: words cted

#ifndef __clang__
// The octetFromBase10String tests intentionally truncate the nullptr in several
// variations to ensure this is handled correctly. You shouldn't need to disable
// this warning in real code and should just include the null terminator in your
// length values passed into this method.
// This is found in GCC 8 and newer:
// https://gcc.gnu.org/git/?p=gcc.git&a=commit;h=025d57f037ad13eb479818b677ef4be4d97b639c
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wpragmas"
#pragma GCC diagnostic ignored "-Wstringop-truncation"
#endif
#include "libcyphal/transport/ip/v4/address.hpp"
#ifndef __clang__
#pragma GCC diagnostic pop
#endif

#include "gtest/gtest.h"
#include "gmock/gmock.h"

namespace
{


TEST(TransportsIpV4AddressTest, DefaultCtor)
{
    libcyphal::transport::ip::v4::Address subject{};
    ASSERT_EQ(subject.asInteger(), 0u);
    ASSERT_FALSE(subject.isLocal());
    ASSERT_FALSE(subject.isMulticast());
    ASSERT_EQ(static_cast<libcyphal::transport::ip::v4::AddressType>(subject), 0U);
    ASSERT_FALSE(subject.isValid());
}

TEST(TransportsIpV4AddressTest, IntegerCtor)
{
    libcyphal::transport::ip::v4::Address int_cted{167772162u};
    libcyphal::transport::ip::v4::Address oct_cted{10u, 0u, 0u, 2u};
    ASSERT_EQ(int_cted, oct_cted);
}

TEST(TransportsIpV4AddressTest, IsValid)
{
    libcyphal::transport::ip::v4::Address subject_0{0u, 0u, 0u, 0u};
    ASSERT_FALSE(subject_0.isValid());
    libcyphal::transport::ip::v4::Address subject_max{255u, 255u, 255u, 255u};
    ASSERT_FALSE(subject_max.isValid());
    libcyphal::transport::ip::v4::Address subject_home{192u, 168u, 0u, 10u};
    ASSERT_TRUE(subject_home.isValid());
}

TEST(TransportsIpV4AddressTest, MoveOps)
{
    libcyphal::transport::ip::v4::Address subject_moved{libcyphal::transport::ip::v4::Address{192u, 168u, 10u, 8u}};
    ASSERT_EQ(subject_moved.asInteger(), 3232238088u);
    libcyphal::transport::ip::v4::Address subject_moved_assigned{};
    subject_moved_assigned = libcyphal::transport::ip::v4::Address{192u, 168u, 10u, 8u};
    ASSERT_EQ(subject_moved_assigned.asInteger(), 3232238088u);
}

TEST(TransportsIpV4AddressTest, CopyOps)
{
    libcyphal::transport::ip::v4::Address subject0{192u, 168u, 10u, 8u};
    libcyphal::transport::ip::v4::Address subject_copied{subject0};
    ASSERT_EQ(subject_copied.asInteger(), 3232238088u);
    ASSERT_EQ(subject0.asInteger(), 3232238088u);

    libcyphal::transport::ip::v4::Address subject1{10u, 0u, 0u, 1u};
    libcyphal::transport::ip::v4::Address subject_copied_assigned{};
    subject_copied_assigned = subject1;
    ASSERT_EQ(subject_copied_assigned.asInteger(), 167772161u);
    ASSERT_EQ(subject1.asInteger(), 167772161u);
}

TEST(TransportsIpV4AddressTest, octetFromBase10String)
{
    ASSERT_EQ(0, libcyphal::transport::ip::v4::Address::octetFromBase10String(nullptr, 4));
    ASSERT_EQ(0, libcyphal::transport::ip::v4::Address::octetFromBase10String("", 0));
    ASSERT_EQ(0, libcyphal::transport::ip::v4::Address::octetFromBase10String("", 1));
    ASSERT_EQ(2, libcyphal::transport::ip::v4::Address::octetFromBase10String("2", 1));

    // Don't do anything insane if the input buffer is too large.
    ASSERT_EQ(0, libcyphal::transport::ip::v4::Address::octetFromBase10String("Hi there. How ya doin'?", 23));
    ASSERT_EQ(255, libcyphal::transport::ip::v4::Address::octetFromBase10String("99999999999999999999999999", 26));
    ASSERT_EQ(0, libcyphal::transport::ip::v4::Address::octetFromBase10String("00000000000000000000000000", 26));
    ASSERT_EQ(1, libcyphal::transport::ip::v4::Address::octetFromBase10String("00000000000000000000000001", 26));
    ASSERT_EQ(255, libcyphal::transport::ip::v4::Address::octetFromBase10String("-2", 2));

    // base 10
    ASSERT_EQ(255, libcyphal::transport::ip::v4::Address::octetFromBase10String("1024", 4));
    ASSERT_EQ(255, libcyphal::transport::ip::v4::Address::octetFromBase10String("1024", 5));
    ASSERT_EQ(255, libcyphal::transport::ip::v4::Address::octetFromBase10String("255", 3));
    ASSERT_EQ(255, libcyphal::transport::ip::v4::Address::octetFromBase10String("255", 4));
    ASSERT_EQ(0, libcyphal::transport::ip::v4::Address::octetFromBase10String("0", 1));
    ASSERT_EQ(0, libcyphal::transport::ip::v4::Address::octetFromBase10String("0", 2));
    ASSERT_EQ(127, libcyphal::transport::ip::v4::Address::octetFromBase10String("127", 3));
}


TEST(TransportsIpV4AddressTest, AddressFromString)
{
    ASSERT_EQ(0, libcyphal::transport::ip::v4::Address::addressFromString(nullptr).asInteger());
    ASSERT_EQ(0, libcyphal::transport::ip::v4::Address::addressFromString("").asInteger());

    // VALID
    ASSERT_EQ(3232238088u, libcyphal::transport::ip::v4::Address::addressFromString("192.168.10.8").asInteger());
    ASSERT_TRUE(libcyphal::transport::ip::v4::Address::addressFromString("127.0.0.1").isLocal());

    // all zeros
    ASSERT_EQ(0u, libcyphal::transport::ip::v4::Address::addressFromString("0").asInteger());
    ASSERT_EQ(0u, libcyphal::transport::ip::v4::Address::addressFromString("0.0").asInteger());
    ASSERT_EQ(0u, libcyphal::transport::ip::v4::Address::addressFromString("0.0.0").asInteger());
    ASSERT_EQ(0u, libcyphal::transport::ip::v4::Address::addressFromString("0.0.0.0").asInteger());
    ASSERT_EQ(0u, libcyphal::transport::ip::v4::Address::addressFromString("0.0.0.0.0").asInteger());

    // all 0xFF
    ASSERT_EQ(4278190080u, libcyphal::transport::ip::v4::Address::addressFromString("255").asInteger());
    ASSERT_EQ(4294901760u, libcyphal::transport::ip::v4::Address::addressFromString("255.255").asInteger());
    ASSERT_EQ(4294967040u, libcyphal::transport::ip::v4::Address::addressFromString("255.255.255").asInteger());
    ASSERT_EQ(4294967295u, libcyphal::transport::ip::v4::Address::addressFromString("255.255.255.255").asInteger());
    ASSERT_EQ(4294967295u, libcyphal::transport::ip::v4::Address::addressFromString("255.255.255.255.255").asInteger());

    // weirdness
    ASSERT_EQ(4278190090u, libcyphal::transport::ip::v4::Address::addressFromString("255...10").asInteger());
    ASSERT_EQ(3232238088u, libcyphal::transport::ip::v4::Address::addressFromString("192.168.010.008").asInteger());
    ASSERT_EQ(192u << 24, libcyphal::transport::ip::v4::Address::addressFromString("192,168,10,8").asInteger());
    //              0         10        20        30        40        50
    //              01234567890123456789012345678901234567890123456789012345
    char mem[56] = "xxx 192.168.10.82the brown fox jumped over the red cow.";
    ASSERT_EQ(3232238088u, libcyphal::transport::ip::v4::Address::addressFromString(&mem[4], 16 - 4).asInteger());
    mem[16] = 0;
    ASSERT_EQ(3232238088u, libcyphal::transport::ip::v4::Address::addressFromString(&mem[4]).asInteger());
}

}  // namespace
