/*
 * Copyright (C) 2014 Pavel Kirienko <pavel.kirienko@gmail.com>
 * Copyright 2019 Amazon.com, Inc. or its affiliates. All Rights Reserved.
 */
/** @file */

#ifndef LIBUAVCAN_TRANSPORT_MEDIA_CAN_HPP_INCLUDED
#define LIBUAVCAN_TRANSPORT_MEDIA_CAN_HPP_INCLUDED

#include <array>

#include "libuavcan/libuavcan.hpp"
#include "libuavcan/introspection.hpp"
#include "libuavcan/time.hpp"

namespace libuavcan
{
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
 * Properties of an ISO compliant CAN FD bus.
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
 * Bit pattern to fill padding bytes with. The UAVCAN specification mandates this value
 * but the actual value of padding bytes must be ignored when receiving messages.
 *
 * When transmitting use this pattern to minimize the number of stuff bits added by the
 * CAN hardware.
 */
constexpr std::uint8_t BytePaddingPattern = 0x55;

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
};

/**
 * A raw CAN frame, as passed to/from a CAN peripheral or subsystem. This is the datastructure used by
 * the media layer of libuavcan to buffer incoming data that is "interesting" before the transport
 * layer parses it into the high-level types defined by DSDL. Interesting data is defined as CAN
 * frames that are compatible with the UAVCAN protocol. For CAN bus, this omits error frames,
 * remote frames, and any frame using 11-bit identifiers. Such uninteresting frames are not compatible
 * with UAVCAN and it is undefined behaviour to attempt to load such data into a Frame instance.
 *
 * For systems which consume unsupported CAN frames it is recommended that another data path is
 * established that does not involve libuavcan. For example, a "statistics" interface might
 * be supported by a driver on a system to handle bus error rate at an application level.
 *
 * @tparam  MTUBytesParam       The maximum number of bytes that can be stored in this frame.
 * @tparam  FlagBitsCompareMask A mask of the upper thee bits of this class's id field. By
 *                              default these bits are ignored by equality operators in the
 *                              class. If an implemenation wants to use these three bits as
 *                              meta-data it should provide a different compare mask to include
 *                              the bits when comparing identifiers.
 *
 * <h4>Data Domains and Filtering</h4>
 *
 * Libuavcan will introduce two copies of data received on a CAN bus into and then across system
 * memory before this data becomes available to an application. Because of this the media layer
 * should be implemented as close to the incoming data as possible. For embedded systems it is ideal
 * if a Frame is the first location in system memory the received data occupies after being read
 * out of peripheral memory. For higher-level systems it is ideal if a Frame is the first location
 * in user-space the data occupies after being received from a kernel.
 *
 * @image html html/data_domains.png width=100%
 * @image latex latex/data_domains.eps
 *
 * As demonstrated by the above diagram, careful configuration of hardware filters and proper elision
 * of unsupported data will minimize the amount of CPU used by libuavcan to copy data through system
 * memory.
 *
 */
template <std::uint16_t MTUBytesParam, std::uint8_t FlagBitsCompareMask = 0x00>
struct LIBUAVCAN_EXPORT Frame
{
    static_assert(MTUBytesParam <= TypeFD::MaxFrameSizeBytes,
                  "CAN::Frame cannot hold anything larger than an CAN FD frame.");

    /**
     * 29-bit mask for extended frame identifiers.
     */
    static constexpr std::uint32_t MaskExtID = 0x1FFFFFFFU;

    /**
     * The mask to use when comparing two Frame::id fields.
     */
    static constexpr std::uint32_t MaskIdWithFlags =
        MaskExtID | (static_cast<std::uint32_t>(FlagBitsCompareMask) << 29);

    /**
     * The maximum number of bytes this frame can hold. This
     * value also effects the largest DLC the instance will
     * report and the largest DLC the instance will accept.
     */
    static constexpr std::uint16_t MTUBytes = MTUBytesParam;

    /**
     * Converts a given frame length value into a Data Length Code.
     *
     * @param  length   The data length value to convert.
     * @return Returns the appropriate DLC value but saturates to the DLC
     *         for the MTUBytesParam parameter.
     */
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

        static_assert(std::extent<decltype(length_to_dlc_lookup)>::value == TypeFD::MaxFrameSizeBytes + 1,
                      "length_to_dlc_lookup table is malformed.");

        FrameDLC result;
        if (length > MTUBytesParam)
        {
            result = FrameDLC(length_to_dlc_lookup[MTUBytesParam]);
        }
        else
        {
            // Because MTUBytesParam cannot exceed the length of the lookup table
            // (see static assert) it is safe to use length as an index in the branch
            // without further checks.
            result = FrameDLC(length_to_dlc_lookup[length]);
        }
        return result;
    }

    /**
     * Converts a Data Length Code into a frame length.
     *
     * @param  dlc   The DLC to convert.
     * @return The maximum number of bytes the frame can occupy for the given
     *         DLC.
     */
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

    /**
     * The 29-bit CAN identifier. The upper three bits is ignored by default but
     * applications can use these bits either opaquely or by enabling in frame
     * comparisons them using the @p FlagBitsCompareMask template parameter.
     *
     * @code
     * // Enable comparison of the 30th bit in the identifier.
     * CAN::Frame<CAN::TypeFD::MaxFrameSizeBytes,0x01> frame;
     *
     * // Use the 30th bit.
     * frame.id = (can_id | (MY_FLAG << 29);
     * @endcode
     */
    std::uint32_t id;

    /**
     * System memory buffer of a CAN frame.
     *
     */
    std::uint8_t data[MTUBytesParam];

    /**
     * Filter type for CAN frames.
     */
    struct Filter
    {
        /**
         * The id filter.
         */
        std::uint32_t id;
        /**
         * A mask for the id field.
         */
        std::uint32_t mask;

        Filter()
            : id(0)
            , mask(0)
        {}

        Filter(std::uint32_t id, std::uint32_t mask)
            : id(id)
            , mask(mask)
        {}

        Filter(const Filter& rhs)
            : id(rhs.id)
            , mask(rhs.mask)
        {}

        bool operator==(const Filter& rhs) const
        {
            return rhs.id == id && rhs.mask == mask;
        }
    };

private:
    FrameDLC dlc_;  ///< Data Length Code.

public:
    /**
     * A monotonic timestamp. Libuavcan operates optimally when this value is a
     * hardware supplied timestamp recorded at the start-of-frame.
     */
    libuavcan::time::Monotonic timestamp;

    Frame()
        : id(0)
        , data{}
        , dlc_(FrameDLC::CodeForLength0)
        , timestamp()
    {}

    /**
     * Copy constructor for frames.
     *
     * @param  rhs  The frame to copy from.
     */
    Frame(const Frame& rhs)
        : id(rhs.id)
        , data{}
        , dlc_(rhs.dlc_)
        , timestamp(rhs.timestamp)
    {
        if (nullptr != rhs.data)
        {
            std::copy(rhs.data, rhs.data + MTUBytesParam, data);
        }
    }

    /**
     * Constructs a new Frame object with timestamp that copies data into this instance.
     *
     * @param can_id        The 29-bit CAN id.
     * @param can_data      The data to copy into this instance.
     * @param in_dlc        The data length code for the can_data.
     * @param can_timestamp A monotonic timestamp that should be as close to the time the start-of-frame was
     *                      received (for rx frames) or put-on-bus (for tx frames) as possible.
     */
    Frame(std::uint32_t                can_id,
          volatile const std::uint8_t* can_data,
          FrameDLC                     in_dlc,
          libuavcan::time::Monotonic   can_timestamp)
        : id(can_id)
        , data{}
        , dlc_(in_dlc)
        , timestamp(can_timestamp)
    {
        if (nullptr == can_data)
        {
            dlc_ = FrameDLC::CodeForLength0;
        }
        else
        {
            const std::uint_fast8_t data_len = dlcToLength(dlc_);
            std::copy(can_data, can_data + static_cast<std::uint8_t>(data_len), this->data);
        }
    }

    /**
     * Constructs a new Frame object that copies data into this instance.
     *
     * @param can_id        The 29-bit CAN id.
     * @param can_data      The data to copy into this instance.
     * @param in_dlc        The data length code for the can_data.
     */
    Frame(std::uint32_t can_id, const std::uint8_t* can_data, FrameDLC in_dlc)
        : Frame(can_id, can_data, in_dlc, libuavcan::time::Monotonic::fromMicrosecond(0))
    {}

    /**
     * Get the Data Length Code set for this instance.
     *
     * @return The DLC set for this instance.
     */
    FrameDLC getDLC() const
    {
        return dlc_;
    }

    /**
     * Set the Data Length for this Frame. This value can be quantized
     * for some lengths as only the Data Length Code is stored internally.
     * So setDataLength(some_value) may not be equal to getDataLength().
     * after it is called.
     *
     * @param data_length  The data length in bytes.
     */
    void setDataLength(std::uint_fast8_t data_length)
    {
        dlc_ = lengthToDlc(data_length);
    }

    /**
     * Get length of the frame data in bytes.
     *
     * @return std::uint_fast8_t  The number of bytes this Frame object contains.
     */
    std::uint_fast8_t getDataLength() const
    {
        return dlcToLength(dlc_);
    }

    /**
     * Logical inverse of @ref libuavcan::media::CAN::Frame::operator==(const
     * libuavcan::media::CAN::Frame&) const
     *
     * @return true if @ref libuavcan::media::CAN::Frame::operator==(const
     * libuavcan::media::CAN::Frame&) const returns false.
     */
    bool operator!=(const Frame& rhs) const
    {
        return !operator==(rhs);
    }

    /**
     * Compares equality for the CAN Frame identifier, DLC (Data Length Code), and data.
     *
     * @return true if both identifiers masked by MaskIdWithFlags are equal, both DLCs are
     *         equal, and the data in both frames are equal.
     */
    bool operator==(const Frame& rhs) const
    {
        return ((id & MaskIdWithFlags) == (rhs.id & MaskIdWithFlags)) && (dlc_ == rhs.dlc_) &&
               std::equal(data, data + dlcToLength(dlc_), rhs.data);
    }

    /**
     * CAN Frame comparisons compare the priority of the frames not the values.
     *
     * @see Frame::priorityHigherThan for more details on CAN bus arbitration.
     */
    bool operator<(const Frame& other) const
    {
        return this->priorityLowerThan(other);
    }

    /**
     * CAN Frame comparisons compare the priority of the frames not the values.
     *
     * @see Frame::priorityHigherThan for more details on CAN bus arbitration.
     */
    bool operator>(const Frame& other) const
    {
        return this->priorityHigherThan(other);
    }

    /**
     * Assignment operator. This will copy all the data from rhs into this
     * instance.
     *
     * @param rhs   The frame to copy data from.
     */
    Frame& operator=(const Frame& rhs)
    {
        id        = rhs.id;
        timestamp = rhs.timestamp;
        dlc_      = rhs.dlc_;
        if (nullptr != rhs.data)
        {
            std::copy(rhs.data, rhs.data + MTUBytesParam, data);
        }
        return *this;
    }

    /**
     * Simple comparison of CAN identifiers. Since it is illegal to populate a Frame with any
     * message type not supported by UAVCAN (e.g. error frames or frames with 11-bit identifiers)
     * this method does a trivial comparison between two can identifiers.
     *
     * @param  rhs  A frame to compare with.
     * @return true if the rhs identifier is < this frame's id.
     *
     * @note See Marco Di Natale - <a href="http://inst.cs.berkeley.edu/~ee249/fa08/Lectures/handout_canbus2.pdf">
     * "Understanding and using the Controller Area Network"</a> for information on how CAN message identifiers are
     * used in CAN arbitration.
     */
    bool priorityHigherThan(const Frame& rhs) const
    {
        /*
         * Plain ID arbitration - greater value loses.
         */
        return (id & MaskExtID) < (rhs.id & MaskExtID);
    }

    /**
     * Simple comparison of CAN identifiers.
     * @see Frame::priorityHigherThan for more details.
     *
     * @param  rhs  A frame to compare with.
     * @return true if the rhs identifier is > this frame's id.
     */
    bool priorityLowerThan(const Frame& rhs) const
    {
        return rhs.priorityHigherThan(*this);
    }
};

}  // end namespace CAN
}  // end namespace media
}  // end namespace libuavcan

#endif  // LIBUAVCAN_TRANSPORT_MEDIA_CAN_HPP_INCLUDED
