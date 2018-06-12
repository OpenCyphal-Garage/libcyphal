/*
 * Copyright (C) 2014 Pavel Kirienko <pavel.kirienko@gmail.com>
 */

#ifndef UAVCAN_MARSHAL_TYPE_UTIL_HPP_INCLUDED
#define UAVCAN_MARSHAL_TYPE_UTIL_HPP_INCLUDED

#include <uavcan/build_config.hpp>
#include <uavcan/util/bus.hpp>
#include <uavcan/util/templates.hpp>
#include <uavcan/util/comparison.hpp>
#include <array>

namespace uavcan
{
/**
 * Read the specs to learn more about cast modes.
 */
enum CastMode { CastModeSaturate, CastModeTruncate };

/**
 * Read the specs to learn more about tail array optimizations.
 */
enum TailArrayOptimizationMode { TailArrayOptDisabled, TailArrayOptEnabled };

/**
 * Compile-time: Returns the number of bits needed to represent an integer value.
 */
template <unsigned long Num>
struct IntegerBitLen
{
    enum { Result = 1 + IntegerBitLen<(Num >> 1)>::Result };
};
template <>
struct IntegerBitLen<0>
{
    enum { Result = 0 };
};

// +--------------------------------------------------------------------------+
// | ByteLenToByteLenWithPadding
// +--------------------------------------------------------------------------+
/*
 * DO NOT USE. Use ByteLenToByteLenWithPadding instead.
 */
template <unsigned ByteLen, bool isMultiFrame>
struct ByteLenToByteLenWithPaddingImpl;

/*
 * DO NOT USE. Use ByteLenToByteLenWithPadding instead.
 */
template <unsigned ByteLen>
struct ByteLenToByteLenWithPaddingImpl<ByteLen, false>
{
    enum { Result = ByteLen };
};

/*
 * DO NOT USE. Use ByteLenToByteLenWithPadding instead.
 */
template <unsigned ByteLen>
struct ByteLenToByteLenWithPaddingImpl<ByteLen, true>
{
    constexpr static unsigned amount_of_data_in_first_frame = uavcan::CanBusType::max_frame_size - 3;
    constexpr static unsigned max_data_in_frames = (uavcan::CanBusType::max_frame_size - 1);
    constexpr static unsigned amount_of_data_in_last_frame = (ByteLen - amount_of_data_in_first_frame) % max_data_in_frames;

    static_assert(amount_of_data_in_last_frame >= 0, "SFINAE should have prevented us from getting here?");
    static_assert(amount_of_data_in_last_frame <= 0xFF, "Tried to calculate padding for a frame that does not fit in a uint8_t");
    static_assert(uavcan::CanBusType::max_frame_size <= CanBusType::max_frame_size, "Lookup table would overflow.");
    static_assert(uavcan::CanBusType::max_frame_size >= 3, "Expected a canbus that has at least 3 bytes in a frame.");

    enum {
        Result = ByteLen + static_cast<unsigned>((CanBusType::payload_length_to_frame_length[amount_of_data_in_last_frame] - static_cast<uint8_t>(amount_of_data_in_last_frame)))
    };
};

/**
 * Compile-time: Returns the number of bytes needed to contain the given number of bytes
 * where transfer padding must be accounted for.
 */
template <unsigned ByteLen>
struct ByteLenToByteLenWithPadding
{
    enum {
        Result = uavcan::ByteLenToByteLenWithPaddingImpl<ByteLen, (ByteLen > (uavcan::CanBusType::max_frame_size - 1))>::Result
    };
};

/**
 * Run-time: Returns the number of padding bytes that would be added to a buffer
 * for a given data payload length.
 */
unsigned calculatePaddingBytes(size_t payload_length);

// +--------------------------------------------------------------------------+

template <unsigned long BitLen>
struct BitLenToByteLen
{
    enum { Result = (BitLen + 7) / 8 };
};

template <unsigned long BitLen>
struct BitLenToByteLenWithPadding
{
    enum {
        Result = uavcan::ByteLenToByteLenWithPadding<uavcan::BitLenToByteLen<BitLen>::Result>::Result
    };
};

/**
 * Compile-time: Helper for integer and float specialization classes. Returns the platform-specific storage type.
 */
template <typename T, typename Enable = void>
struct StorageType
{
    typedef T Type;
};
template <typename T>
struct StorageType<T, typename EnableIfType<typename T::StorageType>::Type>
{
    typedef typename T::StorageType Type;
};

/**
 * Compile-time: Whether T is a primitive type on this platform.
 */
template <typename T>
class IsPrimitiveType
{
    typedef char Yes;
    struct No { Yes dummy[8]; };

    template <typename U>
    static typename EnableIf<U::IsPrimitive, Yes>::Type test(int);

    template <typename>
    static No test(...);

public:
    enum { Result = sizeof(test<T>(0)) == sizeof(Yes) };
};

/**
 * Streams a given value into YAML string. Please see the specializations.
 */
template <typename T>
class UAVCAN_EXPORT YamlStreamer;

}

#endif // UAVCAN_MARSHAL_TYPE_UTIL_HPP_INCLUDED
