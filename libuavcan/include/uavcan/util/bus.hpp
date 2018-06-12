/*
 * Copyright (C) 2014 Pavel Kirienko <pavel.kirienko@gmail.com>
 */

#ifndef UAVCAN_UTIL_BUS_HPP_INCLUDED
#define UAVCAN_UTIL_BUS_HPP_INCLUDED

#include <array>
#include <cstdint>
#include <uavcan/util/templates.hpp>

namespace uavcan
{

/**
 * Properties of an ISO compliant CAN-FD bus.
 */
struct UAVCAN_EXPORT CanBusTypeFd {

    constexpr static size_t max_frame_size = 64;

    /**
     * Lookup table to find the data length that would be used to
     * store a given payload.
     */
    constexpr static std::array<uint8_t, max_frame_size> payload_length_to_frame_length = { {
            0,1,2,3,4,5,6,7,
            11,11,11,11,
            15,15,15,15,
            19,19,19,19,
            23,23,23,23,
            31,31,31,31,31,31,31,31,
            47,47,47,47,47,47,47,47,47,47,47,47,47,47,47,47,
            63,63,63,63,63,63,63,63,63,63,63,63,63,63,63,63
        }
    };
};

/**
 * Properties of a CAN-2.0 compliant bus.
 */
struct UAVCAN_EXPORT CanBusType2_0 {

    constexpr static size_t max_frame_size = 8;

    /**
     * Lookup table to find the data length that would be used to
     * store a given payload.
     */
    constexpr static std::array<uint8_t, max_frame_size> payload_length_to_frame_length = { {
            0,1,2,3,4,5,6,7
        }
    };
};

#ifdef UAVCAN_USE_FD
/**
 * The type of CAN bus this version of libuavcan was compiled for.
 */
using CanBusType = CanBusTypeFd;
#else
/**
 * The type of CAN bus this version of libuavcan was compiled for.
 */
using CanBusType = CanBusType2_0;
#endif

/**
 * Bit pattern to fill padding bytes with.
 */
constexpr uint8_t BytePaddingPattern = 0x55;

} // end namespace uavcan

#endif // UAVCAN_UTIL_BUS_HPP_INCLUDED
