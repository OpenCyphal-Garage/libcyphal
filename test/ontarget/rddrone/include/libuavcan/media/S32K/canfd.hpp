/*
 * Copyright (c) 2020, NXP. All rights reserved.
 * Copyright 2020 Amazon.com, Inc. or its affiliates. All Rights Reserved.
 * Distributed under The MIT License.
 * based on work by Abraham Rodriguez <abraham.rodriguez@nxp.com>
 */

/**
 * @file
 * Header driver file for the media layer of Libuavcan v1 targeting
 * the NXP S32K14 family of automotive grade MCU's, running
 * CAN-FD at 4Mbit/s data phase and 1Mbit/s in nominal phase.
 *
 * @par Pre-processing macros that affect driver compilation:
 * @li @c LIBUAVCAN_S32K_RX_FIFO_LENGTH The number of messages per-interface that can be buffered
 *     from the receive ISR before the mesage is read on the main thread. This memory will be allocated
 *     in .bss by most linkers.
 * @li @c LIBUAVCAN_S32K_NO_TIME If defined as 1 then the driver will not provide correct receive timestamps
 *     and the select method will return @code Result::NotImplemented. This allows a firmware to skip
 *     implementing the @c libuavcan_media_s32k_get_monotonic_time_nanos_isr_safe method.
 * @li @c MCU_S32K14X This driver can be compiled for the MCU_S32K142, MCU_S32K146, MCU_S32K148
 * @li @c LIBUAVCAN_S32K_RDDRONE_BOARD_USED If defined as 1 then PORTE pins 10 and 11 will be controlled
 *     internally by the driver to enable the transceiver of the rddrone_uavcan node board.
 * @li @c LIBUAVCAN_S32K_WDREFRESH_WHILE_WAITING_FOR_FREEZE_MODE If defined as 1 then the driver will refresh
 *      the watchdog peripheral while waiting for the peripheral to enter freeze mode. Do not enable this if
 *      you use the window feature of the watchdog and this is not required if your watchdog timeout is >
 *      about 800 nominal CAN bit times. Freeze mode must be entered when starting an interface group.
 */

#ifndef CANFD_HPP_INCLUDED
#define CANFD_HPP_INCLUDED

#include "libuavcan/media/can.hpp"
#include "libuavcan/media/interfaces.hpp"

#if !defined(LIBUAVCAN_S32K_NO_TIME) || !(LIBUAVCAN_S32K_NO_TIME)
/**
 * To use all the features of the S32K libuavcan media layer you must implement this method in your firmware. It
 * is used by interface groups and interfaces for all timekeeping including RX timestamping. It is defined as a C
 * method to allow sharing with other drivers written in C.
 *
 * This implementation expects a mirosecond-resolution timer. If your system cannot provide this simply up-sample
 * the time available to provide the correct units if not the correct resolution.
 *
 * @par Implementation Requirements:
 * @li The time returned must be monotonic. For a true 64-bit timer this is an intrinisic property since
 *     the rollover for 64-bits in microseconds will exceed the lifespan of the silicon the interfaces are running
 *     on. If implemented using a 32-bit timer (or less) other provisions must be made to prevent the returned
 *     value from decreasing.
 * @li The timer must be ISR-safe. The interface groups may invoke this method from CAN peripheral ISRs and from calls
 *     into methods on the  @c InterfaceGroup object.
 * @li For performance reasons, the method should not disable interrupts. Where using a chained timer, a simple strategy
 *     is to use optimistic reads of the high and low channels in a loop where the full 64-bit value is only returned
 *     once both reads can be made without either value changing.
 * @li The timer's accuracy is directly proportional to the immediacy of the returned timer value (latency) and the
 * consistency of the method's timing (jitter). If a cached time value updated by an ISR is used the latency between
 * updates may introduce jitter into the receive timestamps. If these timestamps are used for precision timekeeping then
 * this jitter may become unacceptable to higher layers. It is better to increase the amount of time spent in the call
 * if the value returned is closer to the actual timer value (i.e. more instructions between invoking the call and
 * obtaining the value do not create latency or jitter but variability in the number of instructions between the time
 * the value was captured and the method returning that value introduces jitter).
 *
 * @note
 * The @c LPIT timer in the S32K family of MCUs is, unfortunately, not well-suited for implementing this method
 * since the current value of its counters cannot be reliably read outside of an ISR.
 *
 */
extern "C" uint64_t libuavcan_media_s32k_get_monotonic_time_micros_isr_safe();

#endif

namespace libuavcan
{
namespace media
{
/**
 * @namespace S32K
 * Microcontroller-specific Interface classes, constants, variables and helper functions that make use
 * of the FlexCAN and LPIT peripherals for the current driver.
 */
namespace S32K
{
/**
 * @class
 * Implementation of the methods from libuavcan's media layer abstracct class InterfaceGroup,
 * with the template arguments listed below; for further details of this interface class,
 * refer to the template declaration in libuavcan/media/interface.hpp
 *
 * FrameT      = Frame with MTUBytesParam = MaxFrameSizeBytes (64 bytes for CAN-FD) and
 *                    FlagBitsCompareMask = 0x00 (default)
 * MaxTxFrames = 1 (default)
 * MaxRxFrames = 1 (default)
 */
class InterfaceGroup : public media::InterfaceGroup<media::CAN::Frame<media::CAN::TypeFD::MaxFrameSizeBytes>>
{
protected:
    /**
     * You can't instantiate or delete this object directly. Obtain references from
     * libuavcan::media::S32K::InterfaceGroup::startInterfaceGroup.
     */
    InterfaceGroup(){};
    virtual ~InterfaceGroup() = default;

public:
    // rule of six
    InterfaceGroup(const InterfaceGroup&) = delete;
    InterfaceGroup& operator=(const InterfaceGroup&) = delete;
    InterfaceGroup(const InterfaceGroup&&)           = delete;
    InterfaceGroup& operator=(const InterfaceGroup&&) = delete;

    struct Statistics
    {
        /**
         * The number of time this interface group discarded received messages because internal
         * receive buffers were full. This will result in older data remaining in the internal
         * receive buffers and newer data being discarded.
         */
        std::uint32_t rx_overflows;
    };

    /**
     * Fill out the current statistics for the group.
     * @param  interface_index  The index of the interface in the group to retrieve statistics for.
     * @param  out_statistics   A data-structure of statistics collected by each interface.
     * @return libuavcan::Result::Success if the statistics data structure was updated with valid
     *         data.
     */
    Result get_statistics(std::uint_fast8_t interface_index, Statistics& out_statistics) const;
};

/**
 * @class
 * Implementation of the methods from libuavcan's media layer abstracct class InterfaceManager,
 * with the template arguments listed below; for further details of this interface class,
 * refer to the template declaration in libuavcan/media/interface.hpp
 *
 * InterfaceGroupT    = S32K_InterfaceGroup  (previously declared class in the file)
 * InterfaceGroupPtrT = S32K_InterfaceGroup* (raw pointer)
 */
class InterfaceManager final : public media::InterfaceManager<InterfaceGroup, InterfaceGroup*>
{
public:
    /** @fn
     * Initialize the peripherals needed for the driver in the target MCU, also configures the
     * core clock sources to the Normal RUN profile,
     * @param [in]   filter_config         The filtering to apply equally to all FlexCAN instances.
     * @param [in]   filter_config_length  The length of the @p filter_config argument.
     * @param [out]  out_group             A pointer to set to the started group. This will be nullptr if the start
     * method fails.
     * @return libuavcan::Result::Success     if the group was successfully started and a valid pointer was returned.
     * @return libuavcan::Result::Failure     if the initialization fails at some point.
     *         The caller should assume that @p out_group is an invalid pointer if any failure is returned.
     * @return libuavcan::Result::BadArgument if filter_config_length is out of bound.
     */
    virtual Result startInterfaceGroup(const typename InterfaceGroupType::FrameType::Filter* filter_config,
                                       std::size_t                                           filter_config_length,
                                       InterfaceGroupPtrType&                                out_group) override;

    /** @fn
     * Release and deinitialize the peripherals needed for the current driver, disables all the FlexCAN
     * instances available, waiting for any pending transmission or reception to finish before. Also
     * resets the LPIT timer used for time-stamping, does not deconfigure the core and asynch clock sources.
    ¨* configured from startInterfaceGroup nor the pins.
     * @param [out]  inout_group Pointer that will be set to null
     * @return libuavcan::Result::Success. If the used peripherals were deinitialized properly.
     */
    virtual Result stopInterfaceGroup(InterfaceGroupPtrType& inout_group) override;

    /** @fn
     * Return the number of filters that the current UAVCAN node can support.
     * @return The maximum number of frame filters available for filter groups managed by this object,
     *         i.e. the number of combinations of ID and mask that each FlexCAN instance supports
     */
    virtual std::size_t getMaxFrameFilters() const override;
};

}  // END namespace S32K
}  // END namespace media
}  // END namespace libuavcan

#endif  // CANFD_HPP_INCLUDED
