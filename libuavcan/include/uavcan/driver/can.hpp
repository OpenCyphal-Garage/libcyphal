/*
 * CAN bus driver interface.
 * Copyright (C) 2014 Pavel Kirienko <pavel.kirienko@gmail.com>
 */

#ifndef UAVCAN_DRIVER_CAN_HPP_INCLUDED
#define UAVCAN_DRIVER_CAN_HPP_INCLUDED

#include <cassert>
#include <type_traits>
#include <uavcan/std.hpp>
#include <uavcan/build_config.hpp>
#include <uavcan/driver/system_clock.hpp>
#include <uavcan/util/bus.hpp>

namespace uavcan
{
/**
 * This limit is defined by the specification.
 */
enum { MaxCanIfaces = 3 };

/**
 * Valid message data codes.
 *
 * CAN DLCs are only 4-bits long so FD had to use 9-15 to encode
 * various lengths up to 64 bits.
 */
enum class CanFrameDLC : uint_fast8_t {
    CodeForLength0 = 0,       /**< Data Length Code: 0 bytes */
    CodeForLength1 = 1,       /**< Data Length Code: 1 bytes */
    CodeForLength2 = 2,       /**< Data Length Code: 2 bytes */
    CodeForLength3 = 3,       /**< Data Length Code: 3 bytes */
    CodeForLength4 = 4,       /**< Data Length Code: 4 bytes */
    CodeForLength5 = 5,       /**< Data Length Code: 5 bytes */
    CodeForLength6 = 6,       /**< Data Length Code: 6 bytes */
    CodeForLength7 = 7,       /**< Data Length Code: 7 bytes */
    CodeForLength8 = 8,       /**< Data Length Code: 8 bytes */
    CodeForLength12 = 9,   /**< Data Length Code: 12 bytes */
    CodeForLength16 = 10,  /**< Data Length Code: 16 bytes */
    CodeForLength20 = 11,  /**< Data Length Code: 20 bytes */
    CodeForLength24 = 12,  /**< Data Length Code: 24 bytes */
    CodeForLength32 = 13,  /**< Data Length Code: 32 bytes */
    CodeForLength48 = 14,  /**< Data Length Code: 48 bytes */
    CodeForLength64 = 15,  /**< Data Length Code: 64 bytes */
    invalid_code = 16            /**< Not a valid DLC. */
};

/**
 * Raw CAN frame, as passed to/from the CAN driver.
 */
struct UAVCAN_EXPORT CanFrame
{
    static const uint32_t MaskStdID = 0x000007FFU;
    static const uint32_t MaskExtID = 0x1FFFFFFFU;
    static const uint32_t FlagEFF = 1U << 31;                  ///< Extended frame format
    static const uint32_t FlagRTR = 1U << 30;                  ///< Remote transmission request
    static const uint32_t FlagERR = 1U << 29;                  ///< Error frame

    static const uint8_t MaxDataLen = CanBusType::max_frame_size;

    static CanFrameDLC lengthToDlc(uint_fast8_t length) 
    {
        /**
         * Lookup table to map a CAN frame length to a DLC value
         * that will accommodate the frame.
         */
        constexpr uint8_t length_to_dlc_lookup[] = 
        {
            0,1,2,3,4,5,6,7,8,
            9, 9, 9, 9,
            10,10,10,10,
            11,11,11,11,
            12,12,12,12,
            13,13,13,13,13,13,13,13,
            14,14,14,14,14,14,14,14,14,14,14,14,14,14,14,14,
            15,15,15,15,15,15,15,15,15,15,15,15,15,15,15,15
        };

        static_assert(std::extent<decltype(length_to_dlc_lookup)>::value == 65, "length_to_dlc_lookup table is malformed.");

        if (length < std::extent<decltype(length_to_dlc_lookup)>::value) 
        {
            return CanFrameDLC(length_to_dlc_lookup[length]);
        } 
        else 
        {
            return CanFrameDLC::invalid_code;
        }
    }

    static uint_fast8_t dlcToLength(CanFrameDLC dlc) 
    {
        /**
         * Lookup table to map a DLC value to the maximum data
         * payload length supported for the DLC.
         */
        constexpr uint8_t dlc_to_length_lookup[] = 
        {
            0,1,2,3,4,5,6,7,8,12,16,20,24,32,48,64
        };

        static_assert(std::extent<decltype(dlc_to_length_lookup)>::value == 16, "dlc_to_length_lookup table is malformed.");

        const auto dlc_value = static_cast<std::underlying_type<CanFrameDLC>::type>(dlc);
        if (dlc_value < std::extent<decltype(dlc_to_length_lookup)>::value) 
        {
            return dlc_to_length_lookup[dlc_value];
        } 
        else 
        {
            return 0;
        }
    }

    uint32_t id;                ///< CAN ID with flags (above)
    uint8_t data[MaxDataLen];

private:
    CanFrameDLC dlc;                ///< Data Length Code
public:

    CanFrame() :
        id(0),
        dlc(CanFrameDLC::CodeForLength0)
    {
        fill(data, data + MaxDataLen, uint8_t(0));
    }

    CanFrame(uint32_t can_id, const uint8_t* can_data, CanFrameDLC in_dlc) :
        id(can_id),
        dlc(in_dlc)
    {
        const uint_fast8_t data_len = dlcToLength(dlc);
        UAVCAN_ASSERT(can_data != UAVCAN_NULLPTR);
        UAVCAN_ASSERT(data_len <= MaxDataLen);
        (void)copy(can_data, can_data + static_cast<uint8_t>(data_len), this->data);
    }

    CanFrameDLC getDLC() const {
        return dlc;
    }

    void setDataLength(uint_fast8_t data_length) {
        dlc = lengthToDlc(data_length);
    }

    uint_fast8_t getDataLength() const {
        return dlcToLength(dlc);
    }

    bool operator!=(const CanFrame& rhs) const { return !operator==(rhs); }
    bool operator==(const CanFrame& rhs) const
    {
        return (id == rhs.id) && (dlc == rhs.dlc) && equal(data, data + dlcToLength(dlc), rhs.data);
    }

    bool isExtended()                  const { return id & FlagEFF; }
    bool isRemoteTransmissionRequest() const { return id & FlagRTR; }
    bool isErrorFrame()                const { return id & FlagERR; }

#if UAVCAN_TOSTRING
    enum StringRepresentation
    {
        StrTight,   ///< Minimum string length (default)
        StrAligned  ///< Fixed formatting for any frame
    };

    std::string toString(StringRepresentation mode = StrTight) const;

#endif

    /**
     * CAN frame arbitration rules, particularly STD vs EXT:
     *     Marco Di Natale - "Understanding and using the Controller Area Network"
     *     http://www6.in.tum.de/pub/Main/TeachingWs2013MSE/CANbus.pdf
     */
    bool priorityHigherThan(const CanFrame& rhs) const;
    bool priorityLowerThan(const CanFrame& rhs) const { return rhs.priorityHigherThan(*this); }
};

/**
 * CAN hardware filter config struct.
 * Flags from @ref CanFrame can be applied to define frame type (EFF, EXT, etc.).
 * @ref ICanIface::configureFilters().
 */
struct UAVCAN_EXPORT CanFilterConfig
{
    uint32_t id;
    uint32_t mask;

    bool operator==(const CanFilterConfig& rhs) const
    {
        return rhs.id == id && rhs.mask == mask;
    }

    CanFilterConfig() :
        id(0),
        mask(0)
    { }
};

/**
 * Events to look for during @ref ICanDriver::select() call.
 * Bit position defines iface index, e.g. read = 1 << 2 to read from the third iface.
 */
struct UAVCAN_EXPORT CanSelectMasks
{
    uint8_t read;
    uint8_t write;

    CanSelectMasks() :
        read(0),
        write(0)
    { }
};

/**
 * Special IO flags.
 *
 * @ref CanIOFlagLoopback       - Send the frame back to RX with true TX timestamps.
 *
 * @ref CanIOFlagAbortOnError   - Abort transmission on first bus error instead of retransmitting. This does not
 *                                affect the case of arbitration loss, in which case the retransmission will work
 *                                as usual. This flag is used together with anonymous messages which allows to
 *                                implement CSMA bus access. Read the spec for details.
 */
typedef uint16_t CanIOFlags;
static const CanIOFlags CanIOFlagLoopback = 1;
static const CanIOFlags CanIOFlagAbortOnError = 2;

/**
 * Single non-blocking CAN interface.
 */
class UAVCAN_EXPORT ICanIface
{
public:
    virtual ~ICanIface() { }

    /**
     * Non-blocking transmission.
     *
     * If the frame wasn't transmitted upon TX deadline, the driver should discard it.
     *
     * Note that it is LIKELY that the library will want to send the frames that were passed into the select()
     * method as the next ones to transmit, but it is NOT guaranteed. The library can replace those with new
     * frames between the calls.
     *
     * @return 1 = one frame transmitted, 0 = TX buffer full, negative for error.
     */
    virtual int16_t send(const CanFrame& frame, MonotonicTime tx_deadline, CanIOFlags flags) = 0;

    /**
     * Non-blocking reception.
     *
     * Timestamps should be provided by the CAN driver, ideally by the hardware CAN controller.
     *
     * Monotonic timestamp is required and can be not precise since it is needed only for
     * protocol timing validation (transfer timeouts and inter-transfer intervals).
     *
     * UTC timestamp is optional, if available it will be used for precise time synchronization;
     * must be set to zero if not available.
     *
     * Refer to @ref ISystemClock to learn more about timestamps.
     *
     * @param [out] out_ts_monotonic Monotonic timestamp, mandatory.
     * @param [out] out_ts_utc       UTC timestamp, optional, zero if unknown.
     * @return 1 = one frame received, 0 = RX buffer empty, negative for error.
     */
    virtual int16_t receive(CanFrame& out_frame, MonotonicTime& out_ts_monotonic, UtcTime& out_ts_utc,
                            CanIOFlags& out_flags) = 0;

    /**
     * Configure the hardware CAN filters. @ref CanFilterConfig.
     *
     * @return 0 = success, negative for error.
     */
    virtual int16_t configureFilters(const CanFilterConfig* filter_configs, uint16_t num_configs) = 0;

    /**
     * Number of available hardware filters.
     */
    virtual uint16_t getNumFilters() const = 0;

    /**
     * Continuously incrementing counter of hardware errors.
     * Arbitration lost should not be treated as a hardware error.
     */
    virtual uint64_t getErrorCount() const = 0;
};

/**
 * Generic CAN driver.
 */
class UAVCAN_EXPORT ICanDriver
{
public:
    virtual ~ICanDriver() { }

    /**
     * Returns an interface by index, or null pointer if the index is out of range.
     */
    virtual ICanIface* getIface(uint8_t iface_index) = 0;

    /**
     * Default implementation of this method calls the non-const overload of getIface().
     * Can be overriden by the application if necessary.
     */
    virtual const ICanIface* getIface(uint8_t iface_index) const
    {
        return const_cast<ICanDriver*>(this)->getIface(iface_index);
    }

    /**
     * Total number of available CAN interfaces.
     * This value shall not change after initialization.
     */
    virtual uint8_t getNumIfaces() const = 0;

    /**
     * Block until the deadline, or one of the specified interfaces becomes available for read or write.
     *
     * Iface masks will be modified by the driver to indicate which exactly interfaces are available for IO.
     *
     * Bit position in the masks defines interface index.
     *
     * Note that it is allowed to return from this method even if no requested events actually happened, or if
     * there are events that were not requested by the library.
     *
     * The pending TX argument contains an array of pointers to CAN frames that the library wants to transmit
     * next, per interface. This is intended to allow the driver to properly prioritize transmissions; many
     * drivers will not need to use it. If a write flag for the given interface is set to one in the select mask
     * structure, then the corresponding pointer is guaranteed to be valid (not UAVCAN_NULLPTR).
     *
     * @param [in,out] inout_masks        Masks indicating which interfaces are needed/available for IO.
     * @param [in]     pending_tx         Array of frames, per interface, that are likely to be transmitted next.
     * @param [in]     blocking_deadline  Zero means non-blocking operation.
     * @return Positive number of ready interfaces or negative error code.
     */
    virtual int16_t select(CanSelectMasks& inout_masks,
                           const CanFrame* (& pending_tx)[MaxCanIfaces],
                           MonotonicTime blocking_deadline) = 0;
};

}

#endif // UAVCAN_DRIVER_CAN_HPP_INCLUDED
