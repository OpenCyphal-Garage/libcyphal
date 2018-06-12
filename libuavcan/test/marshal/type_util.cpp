/*
 * Copyright (C) 2014 Pavel Kirienko <pavel.kirienko@gmail.com>
 */

#include <gtest/gtest.h>
#include <uavcan/marshal/types.hpp>


TEST(MarshalTypeUtil, IntegerBitLen)
{
    using uavcan::IntegerBitLen;

    ASSERT_EQ(0, IntegerBitLen<0>::Result);
    ASSERT_EQ(1, IntegerBitLen<1>::Result);
    ASSERT_EQ(6, IntegerBitLen<42>::Result);
    ASSERT_EQ(8, IntegerBitLen<232>::Result);
    ASSERT_EQ(32, IntegerBitLen<0x81234567>::Result);
}


TEST(MarshalTypeUtil, BitLenToByteLen)
{
    using uavcan::BitLenToByteLen;

    ASSERT_EQ(2, BitLenToByteLen<16>::Result);
    ASSERT_EQ(1, BitLenToByteLen<8>::Result);
    ASSERT_EQ(1, BitLenToByteLen<7>::Result);
    ASSERT_EQ(1, BitLenToByteLen<1>::Result);
    ASSERT_EQ(2, BitLenToByteLen<9>::Result);
}

TEST(MarshalTypeUtil, BitLenToByteLenWithPadding)
{
    using uavcan::BitLenToByteLenWithPadding;
    if (uavcan::IsSameType<uavcan::CanBusType, uavcan::CanBusType2_0>::Result)
    {
        ASSERT_EQ(2, BitLenToByteLenWithPadding<16>::Result);
        ASSERT_EQ(7, BitLenToByteLenWithPadding<56>::Result);
        ASSERT_EQ(8, BitLenToByteLenWithPadding<64>::Result);
    }
    else
    {
        ASSERT_EQ(2, BitLenToByteLenWithPadding<16>::Result);
        ASSERT_EQ(63, BitLenToByteLenWithPadding<504>::Result);
        ASSERT_EQ(64, BitLenToByteLenWithPadding<512>::Result);
        ASSERT_EQ(92, BitLenToByteLenWithPadding<700>::Result);
    }
}

TEST(MarshalTypeUtil, ByteLenToByteLenWithPadding)
{
    using uavcan::ByteLenToByteLenWithPadding;
    if (uavcan::IsSameType<uavcan::CanBusType, uavcan::CanBusType2_0>::Result)
    {
        ASSERT_EQ(2, ByteLenToByteLenWithPadding<2>::Result);
        ASSERT_EQ(7, ByteLenToByteLenWithPadding<7>::Result);
        ASSERT_EQ(8, ByteLenToByteLenWithPadding<8>::Result);
        ASSERT_EQ(9, ByteLenToByteLenWithPadding<9>::Result);
        ASSERT_EQ(16, ByteLenToByteLenWithPadding<16>::Result);
        ASSERT_EQ(17, ByteLenToByteLenWithPadding<17>::Result);
    }
    else
    {
        ASSERT_EQ(2, ByteLenToByteLenWithPadding<2>::Result);
        ASSERT_EQ(63, ByteLenToByteLenWithPadding<63>::Result);
        ASSERT_EQ(64, ByteLenToByteLenWithPadding<64>::Result);

        ASSERT_EQ(72, ByteLenToByteLenWithPadding<69>::Result); // 61, 8
        ASSERT_EQ(72, ByteLenToByteLenWithPadding<70>::Result); // 61, 9
        ASSERT_EQ(72, ByteLenToByteLenWithPadding<71>::Result); // 61, 10
        ASSERT_EQ(72, ByteLenToByteLenWithPadding<72>::Result); // 61, 11

        ASSERT_EQ(76, ByteLenToByteLenWithPadding<73>::Result); // 61, 12
        ASSERT_EQ(76, ByteLenToByteLenWithPadding<74>::Result); // 61, 13
        ASSERT_EQ(76, ByteLenToByteLenWithPadding<75>::Result); // 61, 14
        ASSERT_EQ(76, ByteLenToByteLenWithPadding<76>::Result); // 61, 15

        ASSERT_EQ(80, ByteLenToByteLenWithPadding<77>::Result); // 61, 16
        ASSERT_EQ(80, ByteLenToByteLenWithPadding<78>::Result); // 61, 17
        ASSERT_EQ(80, ByteLenToByteLenWithPadding<79>::Result); // 61, 18
        ASSERT_EQ(80, ByteLenToByteLenWithPadding<80>::Result); // 61, 19

        ASSERT_EQ(84, ByteLenToByteLenWithPadding<81>::Result); // 61, 20
        ASSERT_EQ(84, ByteLenToByteLenWithPadding<82>::Result); // 61, 21
        ASSERT_EQ(84, ByteLenToByteLenWithPadding<83>::Result); // 61, 22
        ASSERT_EQ(84, ByteLenToByteLenWithPadding<84>::Result); // 61, 23

        ASSERT_EQ(92, ByteLenToByteLenWithPadding<85>::Result); // 61, 24
        ASSERT_EQ(92, ByteLenToByteLenWithPadding<86>::Result); // 61, 25
        ASSERT_EQ(92, ByteLenToByteLenWithPadding<87>::Result); // 61, 26
        ASSERT_EQ(92, ByteLenToByteLenWithPadding<88>::Result); // 61, 27
        ASSERT_EQ(92, ByteLenToByteLenWithPadding<89>::Result); // 61, 28
        ASSERT_EQ(92, ByteLenToByteLenWithPadding<90>::Result); // 61, 29
        ASSERT_EQ(92, ByteLenToByteLenWithPadding<91>::Result); // 61, 30
        ASSERT_EQ(92, ByteLenToByteLenWithPadding<92>::Result); // 61, 31

        ASSERT_EQ(108, ByteLenToByteLenWithPadding<93>::Result); // 61, 32
        ASSERT_EQ(108, ByteLenToByteLenWithPadding<94>::Result); // 61, 33
        ASSERT_EQ(108, ByteLenToByteLenWithPadding<95>::Result); // 61, 34
        ASSERT_EQ(108, ByteLenToByteLenWithPadding<96>::Result); // 61, 35
        ASSERT_EQ(108, ByteLenToByteLenWithPadding<97>::Result); // 61, 36
        ASSERT_EQ(108, ByteLenToByteLenWithPadding<98>::Result); // 61, 37
        ASSERT_EQ(108, ByteLenToByteLenWithPadding<99>::Result); // 61, 38
        ASSERT_EQ(108, ByteLenToByteLenWithPadding<100>::Result); // 61, 39
        ASSERT_EQ(108, ByteLenToByteLenWithPadding<101>::Result); // 61, 40
        ASSERT_EQ(108, ByteLenToByteLenWithPadding<102>::Result); // 61, 41
        ASSERT_EQ(108, ByteLenToByteLenWithPadding<103>::Result); // 61, 42
        ASSERT_EQ(108, ByteLenToByteLenWithPadding<104>::Result); // 61, 43
        ASSERT_EQ(108, ByteLenToByteLenWithPadding<105>::Result); // 61, 44
        ASSERT_EQ(108, ByteLenToByteLenWithPadding<106>::Result); // 61, 45
        ASSERT_EQ(108, ByteLenToByteLenWithPadding<107>::Result); // 61, 46
        ASSERT_EQ(108, ByteLenToByteLenWithPadding<108>::Result); // 61, 47

        ASSERT_EQ(124, ByteLenToByteLenWithPadding<109>::Result); // 61, 48
        ASSERT_EQ(124, ByteLenToByteLenWithPadding<110>::Result); // 61, 49
        ASSERT_EQ(124, ByteLenToByteLenWithPadding<111>::Result); // 61, 50
        ASSERT_EQ(124, ByteLenToByteLenWithPadding<112>::Result); // 61, 51
        ASSERT_EQ(124, ByteLenToByteLenWithPadding<113>::Result); // 61, 52
        ASSERT_EQ(124, ByteLenToByteLenWithPadding<114>::Result); // 61, 53
        ASSERT_EQ(124, ByteLenToByteLenWithPadding<115>::Result); // 61, 54
        ASSERT_EQ(124, ByteLenToByteLenWithPadding<116>::Result); // 61, 55
        ASSERT_EQ(124, ByteLenToByteLenWithPadding<117>::Result); // 61, 56
        ASSERT_EQ(124, ByteLenToByteLenWithPadding<118>::Result); // 61, 57
        ASSERT_EQ(124, ByteLenToByteLenWithPadding<119>::Result); // 61, 58
        ASSERT_EQ(124, ByteLenToByteLenWithPadding<120>::Result); // 61, 59
        ASSERT_EQ(124, ByteLenToByteLenWithPadding<121>::Result); // 61, 60
        ASSERT_EQ(124, ByteLenToByteLenWithPadding<122>::Result); // 61, 61
        ASSERT_EQ(124, ByteLenToByteLenWithPadding<123>::Result); // 61, 62
        ASSERT_EQ(124, ByteLenToByteLenWithPadding<124>::Result); // 61, 63

        ASSERT_EQ(125, ByteLenToByteLenWithPadding<125>::Result); // 61, 63, 1
        ASSERT_EQ(135, ByteLenToByteLenWithPadding<132>::Result); // 61, 63, 8
        ASSERT_EQ(135, ByteLenToByteLenWithPadding<135>::Result); // 61, 63, 11

        ASSERT_EQ(139, ByteLenToByteLenWithPadding<136>::Result); // 61, 63, 12
    }
}

TEST(MarshalTypeUtil, calculatePaddingBytes)
{
    using uavcan::calculatePaddingBytes;
    if (uavcan::IsSameType<uavcan::CanBusType, uavcan::CanBusType2_0>::Result)
    {
        ASSERT_EQ(0, calculatePaddingBytes(2));
        ASSERT_EQ(0, calculatePaddingBytes(7));
        ASSERT_EQ(0, calculatePaddingBytes(8));
        ASSERT_EQ(0, calculatePaddingBytes(9));
        ASSERT_EQ(0, calculatePaddingBytes(16));
        ASSERT_EQ(0, calculatePaddingBytes(17));
    }
    else
    {
        ASSERT_EQ(0, calculatePaddingBytes(2));
        ASSERT_EQ(0, calculatePaddingBytes(63));
        ASSERT_EQ(0, calculatePaddingBytes(64));

        ASSERT_EQ(3, calculatePaddingBytes(69)); // 61, 8
        ASSERT_EQ(0, calculatePaddingBytes(72)); // 61, 11

        ASSERT_EQ(3, calculatePaddingBytes(73)); // 61, 12
        ASSERT_EQ(0, calculatePaddingBytes(76)); // 61, 15

        ASSERT_EQ(3, calculatePaddingBytes(77)); // 61, 16
        ASSERT_EQ(0, calculatePaddingBytes(80)); // 61, 19

        ASSERT_EQ(3, calculatePaddingBytes(81)); // 61, 20
        ASSERT_EQ(0, calculatePaddingBytes(84)); // 61, 23

        ASSERT_EQ(7, calculatePaddingBytes(85)); // 61, 24
        ASSERT_EQ(0, calculatePaddingBytes(92)); // 61, 31

        ASSERT_EQ(15, calculatePaddingBytes(93)); // 61, 32
        ASSERT_EQ(0, calculatePaddingBytes(108)); // 61, 47

        ASSERT_EQ(15, calculatePaddingBytes(109)); // 61, 48
        ASSERT_EQ(0, calculatePaddingBytes(124)); // 61, 63

        ASSERT_EQ(0, calculatePaddingBytes(125)); // 61, 63, 1
        ASSERT_EQ(3, calculatePaddingBytes(132)); // 61, 63, 8
        ASSERT_EQ(0, calculatePaddingBytes(135)); // 61, 63, 11

        ASSERT_EQ(3, calculatePaddingBytes(136)); // 61, 63, 12
    }
}
