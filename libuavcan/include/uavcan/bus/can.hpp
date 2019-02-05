/*
 * Copyright (C) 2014 Pavel Kirienko <pavel.kirienko@gmail.com>
 * Copyright 2019 Amazon.com, Inc. or its affiliates. All Rights Reserved.
 */
/** @file */

#ifndef UAVCAN_BUS_CAN_HPP_INCLUDED
#define UAVCAN_BUS_CAN_HPP_INCLUDED

#include <array>

#include "uavcan/uavcan.hpp"

namespace uavcan
{
/**
 * @namespace bus
 * Contains bus-specific namespaces.
 */
namespace bus
{
/**
 * @namespace CAN
 * Types for working with UAVCAN on a Controller Area Network.
 */
namespace CAN
{
/**
 * @namespace TypeFd
 * Properties of an ISO compliant CAN-FD bus.
 */
namespace TypeFd
{
/**
 * The maximum size of a data frame for this bus.
 */
constexpr static size_t max_frame_size_bytes = 64;

/** The size of the tail byte, in bytes. */
constexpr static size_t tail_byte_size_bytes = 1;

/** The number of bytes in the transfer CRC. */
constexpr static size_t transfer_crc_size_bytes = 2;

/**
 * Lookup table to find the data length that would be used to
 * store a given payload.
 */
constexpr static std::array<uint8_t, max_frame_size_bytes> payload_length_to_frame_length = {
    {0,  1,  2,  3,  4,  5,  6,  7,  11, 11, 11, 11, 15, 15, 15, 15, 19, 19, 19, 19, 23, 23,
     23, 23, 31, 31, 31, 31, 31, 31, 31, 31, 47, 47, 47, 47, 47, 47, 47, 47, 47, 47, 47, 47,
     47, 47, 47, 47, 63, 63, 63, 63, 63, 63, 63, 63, 63, 63, 63, 63, 63, 63, 63, 63}};

}  // end namespace TypeFd

/**
 * @namespace Type2_0
 * Properties of a CAN-2.0 compliant bus.
 */
namespace Type2_0
{
/**
 * The maximum size of a data frame for this bus.
 */
constexpr static size_t max_frame_size_bytes = 8;

/** The size of the tail byte, in bytes. */
constexpr static size_t tail_byte_size_bytes = 1;

/** The number of bytes in the transfer CRC. */
constexpr static size_t transfer_crc_size_bytes = 2;

/**
 * Lookup table to find the data length that would be used to
 * store a given payload.
 */
constexpr static std::array<uint8_t, max_frame_size_bytes> payload_length_to_frame_length = {{0, 1, 2, 3, 4, 5, 6, 7}};

}  // end namespace Type2_0

/**
 * Bit pattern to fill padding bytes with. The UAVCAN specification does not mandate this value
 * and the actual value of padding bytes must be ignored when receiving messages.
 * When transmitting use this pattern to minimize the number of stuff bits added by the
 * CAN hardware.
 */
constexpr uint8_t bytePaddingPattern = 0x55;

#if (UAVCAN_ENABLE_FD)

/**
 * The MTU based on the current build configuration.
 */
constexpr uint16_t MTU = TypeFd::max_frame_size_bytes;

#else

/**
 * The MTU based on the current build configuration.
 */
constexpr uint16_t MTU = Type2_0::max_frame_size_bytes;

#endif

}  // end namespace CAN
}  // end namespace bus
}  // end namespace uavcan

#endif  // UAVCAN_BUS_CAN_HPP_INCLUDED
