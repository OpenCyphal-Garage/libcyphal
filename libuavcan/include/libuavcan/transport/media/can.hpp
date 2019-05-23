/*
 * Copyright (C) 2014 Pavel Kirienko <pavel.kirienko@gmail.com>
 * Copyright 2019 Amazon.com, Inc. or its affiliates. All Rights Reserved.
 */
/** @file */

#ifndef LIBUAVCAN_TRANSPORT_MEDIA_CAN_HPP_INCLUDED
#define LIBUAVCAN_TRANSPORT_MEDIA_CAN_HPP_INCLUDED

#include <array>
#include <algorithm>

#include "libuavcan/libuavcan.hpp"
#include "libuavcan/introspection.hpp"

namespace libuavcan
{
/**
 * @namespace transport
 * Contains transport-specific types and namespaces.
 */
namespace transport
{
/**
 * @namespace media
 * Contains media-specific types and namespaces.
 */
namespace media
{
/**
 * @namespace CAN
 * Types for working with UAVCAN on a Controller Area Network.
 */
namespace CAN
{
/** The size of the tail byte, in bytes. */
constexpr static std::size_t TailByteSizeBytes = 1;

/** The number of bytes in the transfer CRC. */
constexpr static std::size_t TransferCrcSizeBytes = 2;

/**
 * @namespace TypeFD
 * Properties of an ISO compliant CAN-FD bus.
 */
namespace TypeFD
{
/**
 * The maximum size of a data frame for this bus.
 */
constexpr static std::size_t MaxFrameSizeBytes = 64;

/**
 * Lookup table to find the data length that would be used to
 * store a given payload.
 */
constexpr static std::array<std::uint8_t, MaxFrameSizeBytes> PayloadLengthToFrameLength = {
    {0,  1,  2,  3,  4,  5,  6,  7,  11, 11, 11, 11, 15, 15, 15, 15, 19, 19, 19, 19, 23, 23,
     23, 23, 31, 31, 31, 31, 31, 31, 31, 31, 47, 47, 47, 47, 47, 47, 47, 47, 47, 47, 47, 47,
     47, 47, 47, 47, 63, 63, 63, 63, 63, 63, 63, 63, 63, 63, 63, 63, 63, 63, 63, 63}};

}  // end namespace TypeFD

/**
 * @namespace Type2_0
 * Properties of a CAN-2.0 compliant bus.
 */
namespace Type2_0
{
/**
 * The maximum size of a data frame for this bus.
 */
constexpr static std::size_t MaxFrameSizeBytes = 8;

/**
 * Lookup table to find the data length that would be used to
 * store a given payload.
 */
constexpr static std::array<std::uint8_t, MaxFrameSizeBytes> PayloadLengthToFrameLength = {{0, 1, 2, 3, 4, 5, 6, 7}};

}  // end namespace Type2_0

/**
 * Bit pattern to fill padding bytes with. The UAVCAN specification does not mandate this value
 * and the actual value of padding bytes must be ignored when receiving messages.
 * When transmitting use this pattern to minimize the number of stuff bits added by the
 * CAN hardware.
 */
constexpr std::uint8_t BytePaddingPattern = 0x55;

#if (LIBUAVCAN_FEATURE_ENABLE_FD)

/**
 * The MTU based on the current build configuration.
 */
constexpr std::uint16_t MTU = TypeFD::MaxFrameSizeBytes;

#else

/**
 * The MTU based on the current build configuration.
 */
constexpr std::uint16_t MTU = Type2_0::MaxFrameSizeBytes;

#endif

/**
 * Valid message data codes.
 *
 * CAN DLCs are only 4-bits long so FD had to use 9-15 to encode
 * various lengths up to 64 bits.
 */
enum class FrameDLC : std::uint_fast8_t
{
    CodeForLength0  = 0,  /**< Data Length Code: 0 bytes */
    CodeForLength1  = 1,  /**< Data Length Code: 1 bytes */
    CodeForLength2  = 2,  /**< Data Length Code: 2 bytes */
    CodeForLength3  = 3,  /**< Data Length Code: 3 bytes */
    CodeForLength4  = 4,  /**< Data Length Code: 4 bytes */
    CodeForLength5  = 5,  /**< Data Length Code: 5 bytes */
    CodeForLength6  = 6,  /**< Data Length Code: 6 bytes */
    CodeForLength7  = 7,  /**< Data Length Code: 7 bytes */
    CodeForLength8  = 8,  /**< Data Length Code: 8 bytes */
    CodeForLength12 = 9,  /**< Data Length Code: 12 bytes */
    CodeForLength16 = 10, /**< Data Length Code: 16 bytes */
    CodeForLength20 = 11, /**< Data Length Code: 20 bytes */
    CodeForLength24 = 12, /**< Data Length Code: 24 bytes */
    CodeForLength32 = 13, /**< Data Length Code: 32 bytes */
    CodeForLength48 = 14, /**< Data Length Code: 48 bytes */
    CodeForLength64 = 15, /**< Data Length Code: 64 bytes */
    invalid_code    = 16  /**< Not a valid DLC. */
};

/**
 * Raw CAN frame, as passed to/from the CAN driver.
 */
template <std::uint16_t MtuBytesParam>
struct LIBUAVCAN_EXPORT Frame
{
    static constexpr std::uint32_t MaskStdID = 0x000007FFU;
    static constexpr std::uint32_t MaskExtID = 0x1FFFFFFFU;
    static constexpr std::uint32_t FlagEFF   = 1U << 31;  ///< Extended frame format
    static constexpr std::uint32_t FlagRTR   = 1U << 30;  ///< Remote transmission request
    static constexpr std::uint32_t FlagERR   = 1U << 29;  ///< Error frame

    static constexpr std::uint16_t MtuBytes = MtuBytesParam;

    // TODO: allow additional meta-data like ts valid flag
    // and "after rx overflow".
    // This should be extensible.

    static FrameDLC lengthToDlc(std::uint_fast8_t length)
    {
        /**
         * Lookup table to map a CAN frame length to a DLC value
         * that will accommodate the frame.
         */
        constexpr std::uint8_t length_to_dlc_lookup[] = {0,  1,  2,  3,  4,  5,  6,  7,  8,  9,  9,  9,  9,
                                                         10, 10, 10, 10, 11, 11, 11, 11, 12, 12, 12, 12, 13,
                                                         13, 13, 13, 13, 13, 13, 13, 14, 14, 14, 14, 14, 14,
                                                         14, 14, 14, 14, 14, 14, 14, 14, 14, 14, 15, 15, 15,
                                                         15, 15, 15, 15, 15, 15, 15, 15, 15, 15, 15, 15, 15};

        static_assert(std::extent<decltype(length_to_dlc_lookup)>::value == 65,
                      "length_to_dlc_lookup table is malformed.");
        static_assert(std::extent<decltype(length_to_dlc_lookup)>::value > MtuBytesParam,
                      "CAN MTU cannot exceed the length of the dlc lookup table.");

        FrameDLC result;
        if (length > MtuBytesParam)
        {
            result = FrameDLC(length_to_dlc_lookup[MtuBytesParam]);
        }
        else
        {
            // Because MtuBytesParam cannot exceed the length of the lookup table
            // (see static assert) it is safe to use length as an index in the branch
            // without further checks.
            result = FrameDLC(length_to_dlc_lookup[length]);
        }
        return result;
    }

    static std::uint_fast8_t dlcToLength(FrameDLC dlc)
    {
        /**
         * Lookup table to map a DLC value to the maximum data
         * payload length supported for the DLC.
         */
        constexpr std::uint8_t dlc_to_length_lookup[] = {0, 1, 2, 3, 4, 5, 6, 7, 8, 12, 16, 20, 24, 32, 48, 64};

        static_assert(std::extent<decltype(dlc_to_length_lookup)>::value == 16,
                      "dlc_to_length_lookup table is malformed.");

        const auto dlc_value = static_cast<std::underlying_type<FrameDLC>::type>(dlc);
        if (dlc_value < std::extent<decltype(dlc_to_length_lookup)>::value)
        {
            return dlc_to_length_lookup[dlc_value];
        }
        else
        {
            return 0;
        }
    }

    std::uint32_t id;  ///< CAN ID with flags (above)
    std::uint8_t  data[MtuBytesParam];

private:
    FrameDLC dlc;  ///< Data Length Code

public:
    Frame()
        : id(0)
        , dlc(FrameDLC::CodeForLength0)
    {
        std::fill(data, data + MtuBytesParam, std::uint8_t(0));
    }

    Frame(std::uint32_t can_id, const std::uint8_t* can_data, FrameDLC in_dlc)
        : id(can_id)
        , dlc(in_dlc)
    {
        const std::uint_fast8_t data_len = dlcToLength(dlc);
        LIBUAVCAN_ASSERT(nullptr_t != can_data);
        LIBUAVCAN_ASSERT(data_len <= MtuBytesParam);
        std::copy(can_data, can_data + static_cast<std::uint8_t>(data_len), this->data);
    }

    FrameDLC getDLC() const
    {
        return dlc;
    }

    void setDataLength(std::uint_fast8_t data_length)
    {
        dlc = lengthToDlc(data_length);
    }

    std::uint_fast8_t getDataLength() const
    {
        return dlcToLength(dlc);
    }

    bool operator!=(const Frame& rhs) const
    {
        return !operator==(rhs);
    }

    bool operator==(const Frame& rhs) const
    {
        return (id == rhs.id) && (dlc == rhs.dlc) && equal(data, data + dlcToLength(dlc), rhs.data);
    }

    bool operator<(const Frame& other) const
    {
        return this->priorityLowerThan(other);
    }

    bool operator>(const Frame& other) const
    {
        return this->priorityHigherThan(other);
    }

    bool isExtended() const
    {
        return id & FlagEFF;
    }

    bool isRemoteTransmissionRequest() const
    {
        return id & FlagRTR;
    }

    bool isErrorFrame() const
    {
        return id & FlagERR;
    }

    /**
     * CAN frame arbitration rules, particularly STD vs EXT:
     *     Marco Di Natale - "Understanding and using the Controller Area Network"
     *     https://www.amazon.com/dp/B00F5USEOE
     */
    bool priorityHigherThan(const Frame& rhs) const
    {
        const std::uint32_t clean_id     = id & MaskExtID;
        const std::uint32_t rhs_clean_id = rhs.id & MaskExtID;

        /*
         * STD vs EXT - if 11 most significant bits are the same, EXT loses.
         */
        const bool ext     = id & FlagEFF;
        const bool rhs_ext = rhs.id & FlagEFF;
        if (ext != rhs_ext)
        {
            const std::uint32_t arb11     = ext ? (clean_id >> 18) : clean_id;
            const std::uint32_t rhs_arb11 = rhs_ext ? (rhs_clean_id >> 18) : rhs_clean_id;
            if (arb11 != rhs_arb11)
            {
                return arb11 < rhs_arb11;
            }
            else
            {
                return rhs_ext;
            }
        }

        /*
         * RTR vs Data frame - if frame identifiers and frame types are the same, RTR loses.
         */
        const bool rtr     = id & FlagRTR;
        const bool rhs_rtr = rhs.id & FlagRTR;
        if (clean_id == rhs_clean_id && rtr != rhs_rtr)
        {
            return rhs_rtr;
        }

        /*
         * Plain ID arbitration - greater value loses.
         */
        return clean_id < rhs_clean_id;
    }

    bool priorityLowerThan(const Frame& rhs) const
    {
        return rhs.priorityHigherThan(*this);
    }
};

}  // end namespace CAN
}  // end namespace media
}  // end namespace transport
}  // end namespace libuavcan

#endif  // LIBUAVCAN_TRANSPORT_MEDIA_CAN_HPP_INCLUDED
