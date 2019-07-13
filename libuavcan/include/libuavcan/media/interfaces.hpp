/*
 * Copyright (C) 2014 Pavel Kirienko <pavel.kirienko@gmail.com>
 * Copyright 2019 Amazon.com, Inc. or its affiliates. All Rights Reserved.
 */
/** @file
 * Contains types and C++ interfaces required to fully implement hardware interface
 * support for the media layer. See the @ref MediaDevGuide for more details.
 */

#ifndef LIBUAVCAN_TRANSPORT_MEDIA_INTERFACES_HPP_INCLUDED
#define LIBUAVCAN_TRANSPORT_MEDIA_INTERFACES_HPP_INCLUDED

#include <algorithm>

#include "libuavcan/libuavcan.hpp"
#include "libuavcan/introspection.hpp"
#include "libuavcan/time.hpp"

namespace libuavcan
{
/**
 * @namespace media
 * Contains media-specific types and namespaces.
 *
 * See the @ref MediaDevGuide for details
 * on porting the media layer to a given platform.
 */
namespace media
{
/**
 * Single non-blocking connection to a UAVCAN bus with isolated rx and tx queues. While libuavcan may share
 * hardware peripherals with other components and/or processes for a given system a media layer interface
 * object shall be the sole access to a single hardware connection to a bus for this library.
 *
 * MaxTxFrames and MaxRxFrames are template parameters to allow an implementation to allocate adequate buffers
 * as part of their type. As such, these values may affect the amount of heap, bss, or stack RAM used depending
 * on where the media layer objects are placed for a given system or if an implementation chooses to use dynamic
 * memory internally. Media layer implementations should document this and all other parameters that affect
 * the amount and type of memory used for a particular system and provide guidance for tuning performance versus
 * memory size to the user.
 *
 * @tparam  FrameT          The media-specific frame type to be exchanged across this interface.
 * @tparam  MaxTxFrames     The maximum number of frames that can be written in a single operation using
 *                          this interface. This value must be > 0. If set to 1 then specializations may
 *                          use different system APIs than if set to > 1.
 * @tparam  MaxRxFrames     The maximum number of frames that can be read in a single operation using
 *                          this interface. This value must be > 0. If set to 1 then specializations may
 *                          use different system APIs then if set to > 1.
 */
template <typename FrameT, std::size_t MaxTxFrames = 1, std::size_t MaxRxFrames = 1>
class LIBUAVCAN_EXPORT Interface
{
public:
    virtual ~Interface() = default;

    static_assert(MaxTxFrames > 0, "MaxTxFrames must be > 0");
    static_assert(MaxRxFrames > 0, "MaxRxFrames must be > 0");

    /**
     * The media-specific frame type exchanged across this interface.
     */
    using FrameType = FrameT;

    /**
     * The length of arrays used to read frames through this interface.
     */
    static constexpr std::size_t RxFramesLen = MaxRxFrames;

    /**
     * The length of arrays used to read frames from this interface.
     */
    static constexpr std::size_t TxFramesLen = MaxTxFrames;

    /**
     * Return the index for this interface. The interface index is the canonical identifier used by libuavcan
     * to open, close, and access a given interface. Per the v1 specification, lower indicies are preferred
     * when receiving messages through redundant groups.
     *
     * @return This interface's index.
     */
    virtual std::uint_fast8_t getInterfaceIndex() const = 0;

    /**
     * Non-blocking transmission. All implementations will have some intermediate buffer this method
     * writes to since it does not block on actual transmission of the frame. For some implementations,
     * this method may borrow CPU time to move another, higher priority frame into a lower-level queue
     * after enqueueing the given one in an intermediate queue.
     *
     * @note Implementations are allowed to provide queues based on message priority. Because of this,
     * if a given message cannot be written the media layer should keep trying to write other messages
     * with a different priority.
     *
     * @param  frames       1..* frames to write into the system queues for immediate transmission.
     * @param  frames_len   The number of frames in the frames array that should be sent
     *                      (starting from frame 0).
     * @param  out_frames_written
     *                      The number of frames written. If this is less than frames_len then frames
     *                      [0 - out_frames_written) were enqueued for transmission. Frames
     *                      [out_frames_written - frames_len) were not able to be sent. Nominally this is
     *                      due to the internal queues being full.
     * @return libuavcan::Result::success_partial if some but not all of the frames were written.
     *         libuavcan::Result::buffer_full if no frames could be written because the buffer was
     *         full.
     *         libuavcan::Result::success if all frames were written.
     */
    virtual libuavcan::Result write(const FrameT (&frames)[MaxTxFrames],
                                    std::size_t  frames_len,
                                    std::size_t& out_frames_written) = 0;

    /**
     * Non-blocking reception.
     *
     * Timestamps should be provided by the media driver, ideally by the hardware peripheral.
     *
     * While the monotonic timestamp is required the UAVCAN protoocol can tolerate imprecision since
     * these timestamps are used only for protocol timing validation  (i.e. transfer timeouts and
     * inter-transfer intervals).
     *
     * @return libuavcan::Result::success If no errors occurred.
     */
    virtual libuavcan::Result read(FrameT (&out_frames)[MaxRxFrames], std::size_t& out_frames_read) = 0;
};

/**
 * Manages access to and the lifecycle of media interfaces to a single bus type. A given InterfaceManager
 * shall only expose interfaces to busses that are used as redundant media for the same transport (i.e. An
 * interface manager should define a single logical bus). How manager objects are exposed to an application
 * is not specified by libuavcan.
 *
 * @tparam  InterfaceT              The type to use for interfaces. Must implement Interface.
 * @tparam  InterfacePtrT           The pointer type to use for InterfaceT pointers in this class. On
 *                                  systems without dynamic memory this can probably be simply <code>InterfaceT*</code>
 *                                  but where dynamic memory is used some sort of smart pointer is recommended.
 * @tparam  MaxSelectInterfacesVal  The maximum number of interfaces the manager's select method can support.
 *                                  This is required since the media layer interfaces do not require dynamic memory.
 *                                  It's also not expected that more than two or three interfaces would ever be
 *                                  required (i.e. it is expected that the manager's select will be used to wait
 *                                  on a redundant set of interfaces to a single bus).
 */
template <typename InterfaceT, typename InterfacePtrT, std::size_t MaxSelectInterfacesVal = 2>
class LIBUAVCAN_EXPORT InterfaceManager
{
public:
    virtual ~InterfaceManager() = default;

    /**
     * The media-specific interface managed by this object.
     */
    using InterfaceType = InterfaceT;

    /**
     * The pointer type used for pointers to InterfaceType. This can be a raw pointer or a smart pointer.
     * The temporization is provided to allow either based on how a given media layer is implemented.
     */
    using InterfacePtrType = InterfacePtrT;

    /**
     * The maximum number of interfaces the select method must support in a single invocation.
     */
    static constexpr std::size_t MaxSelectInterfaces = MaxSelectInterfacesVal;

    /**
     * Opens an interface for receiveing and transmitting.
     *
     * @param       interface_index         The index of the interface to open. See getHardwareInterfaceCount()
     *                                      for the valid range of interface indicies. The behaviour of this method
     *                                      given an index >= getHardwareInterfaceCount() is implementation defined.
     * @param       filter_config           An array of frame filtering parameters. The contents and behaviour of
     *                                      filters is dependant on the interface in use and the Frame type.
     * @param       filter_config_length    The number of filter configurations in the filter_config array.
     * @param[out]  out_interface           If successful, the pointer is set to an open interface abstraction
     *                                      owned by this manager instance. This memory shall remain valid while
     *                                      the manager object is valid. It is undefined behavior to destroy a manager
     *                                      with open interfaces still allocated.
     *                                      Implementations must define the semantics for calling this method multiple
     *                                      times with the same interface_index and for calling closeInterface on
     *                                      a shared pointer to an interface.
     * @return libuavcan::Result::success if the interface was successfully opened and returned,
     */
    virtual libuavcan::Result openInterface(std::uint_fast8_t                                interface_index,
                                            const typename InterfaceType::FrameType::Filter* filter_config,
                                            std::size_t                                      filter_config_length,
                                            InterfacePtrT&                                   out_interface) = 0;

    /**
     * Block for a specified amount of time or until an interface becomes ready to read or write.
     *
     * Note that it is allowed to return from this method even if no requested events actually happened, or if
     * there are events that were not requested by the library.
     *
     * @param [in]     inout_interfaces   The interfaces to wait on.
     * @param [in]     interfaces_length  The number of interfaces in the @p interfaces array. Note that the
     *                                    interfaces array is not sparse. Any null pointers found before reaching
     *                                    @p interfaces_length will cause libuavcan::Result::bad_argument to be
     *                                    returned.
     * @param [in]     timeout            The amount of time to wait for an event.
     * @param [in]     ignore_write_available If true then this method will not return if interfaces become available
     *                                    only for write.
     *
     * @return  libuavcan::Result::success_timeout if no events ocurred but the select operation timedout.
     *          libuavcan::Result::success if one or more of the provided interfaces are ready for read, and if
     *          @p ignore_write_available is false, or write.
     *          libuavcan::Result::success_partial if one or more errors were reported for one or more interfaces.
     */
    virtual libuavcan::Result select(const InterfaceType* const (&interfaces)[MaxSelectInterfacesVal],
                                     std::size_t                    interfaces_length,
                                     libuavcan::duration::Monotonic timeout,
                                     bool                           ignore_write_available) = 0;

    /**
     * Closes an interface.
     *
     * @param[in,out] inout_interface    On input this is a pointer to an interface to close. On output
     *                                   this pointer will be reset (set to `nullptr` for raw pointers).
     *
     * @return libuavcan::Result::success if the interface was closed and inout_interface is now invalid.
     *         Implementations must define the semantics for calling this method with a shared_ptr.
     */
    virtual libuavcan::Result closeInterface(InterfacePtrT& inout_interface) = 0;

    /**
     * The total number of available hardware interfaces. On some systems additional virtual interfaces can be
     * created but libuavcan requires interfaces backed by hardware to allow redundancy guarantees to be met.
     *
     * @return The number of interfaces backed by hardware interfaces to a bus. The set of unsigned integers
     *         [0, getHardwareInterfaceCount()) are always valid interface indicies where the hardware interface
     *         count is > 0. Values > the hardware interface count may or may not be valid depending on the
     *         specific implementation of this interface.
     */
    virtual std::uint_fast8_t getHardwareInterfaceCount() const = 0;

    /**
     * The number of hardware filters available for a given interface.
     * @param  interface_index  The index of the interface to get a hardware filter count for.
     *
     * @return Available hardware Frame filters for interface_index.
     */
    virtual std::size_t getMaxHardwareFrameFilters(std::uint_fast8_t interface_index) const = 0;

    /**
     * The number of filters that an interface will accept where some or all of the frame
     * filtering may be performed in software (i.e. this is a super-set that includes any available
     * hardware filters). For some implementations this value may be limited only by available system
     * resources. On such systems configuring large numbers of filters may degrade performance.
     *
     * @param  interface_index  The index of the interface to get a total filter count for.
     * @return Available Frame filters for interface_index.
     */
    virtual std::size_t getMaxFrameFilters(std::uint_fast8_t interface_index) const = 0;
};

}  // namespace media
}  // namespace libuavcan

#endif  // LIBUAVCAN_TRANSPORT_MEDIA_INTERFACES_HPP_INCLUDED
