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

#include <array>
#include <algorithm>

#include "libuavcan/libuavcan.hpp"
#include "libuavcan/introspection.hpp"
#include "libuavcan/time.hpp"

namespace libuavcan
{
namespace transport
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
 * memory internally. Media layer implementations should document this and all other paramters that affect
 * the amount and type of memory used for a particular system and provide guidance for tuning performance versus
 * memory size to the user.
 *
 * @tparam  FrameT          The media-specific frame type to be exchanged across this interface.
 * @tparam  MaxTxFrames     The maximum number of frames that can be written in a single operation using
 *                          this interface. This value must be > 0. If set to 1 then specializations may
 *                          use different system APIs then if set to > 1.
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
     * The length of arrays used to write frames through this interface.
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
     * @return
     *          - 0 = one frame enqueued for transmission.
     *          - -1 = TX buffer full for messages of this type.
     *          - All other negative values are errors.
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
     * @return
     *          - 1 = one frame received
     *          - 0 = RX buffer empty
     *          - negative for error
     */
    virtual libuavcan::Result read(FrameT (&out_frames)[MaxRxFrames], std::size_t& out_frames_read) = 0;
};

/**
 * Manages access to and the lifecycle of media interfaces to a single bus type. A given InterfaceManager
 * shall only expose interfaces to busses that are used as redundant media for the same transport (i.e. An
 * interface manager should define a single logical bus). How manager objects are exposed to an application
 * is not specified by libuavcan.
 *
 * @tparam  InterfaceT   The type to use for interfaces. Must implement Interface.
 */
template <typename InterfaceT>
class LIBUAVCAN_EXPORT InterfaceManager
{
public:
    virtual ~InterfaceManager() = default;

    /**
     * The media-specific interface managed by this object.
     */
    using InterfaceType = InterfaceT;

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
     *                                      owned by this manager instance. This memory will remain valid while
     *                                      the manager object is valid.
     * @return
     *         - 0 if a new interface was opened and returned.
     *         - 1 is returned if the interface was already opened and out_interface was given a reference to it.
     *             The behaviour in this case is implementation defined since the pointers vended by this class
     *             are not valid after closeInterface is called on any one of them. For embedded systems where
     *             interfaces are not closed this never becomes an issue. For higher-level systems the media
     *             layer implementation should provide stronger memory ownership semantics perhaps wrapping this
     *             interface in another that vends std::shared_ptr instances.
     *         - -1 if the interface_index was invalid.
     *         - < -1 for all other errors.
     */
    virtual libuavcan::Result openInterface(std::uint_fast8_t                                interface_index,
                                            const typename InterfaceType::FrameType::Filter* filter_config,
                                            std::size_t                                      filter_config_length,
                                            InterfaceT*&                                     out_interface) = 0;

    /**
     * Closes an interface.
     *
     * @param[inout] inout_interface    On input this is a pointer to an interface to close. On output
     *                                  this pointer will be set to `nullptr`.
     *
     * @return
     *          - 0 if the interface was closed. inout_interface will be set to `nullptr`.
     *          - -1 if inout_interface was already `nullptr`.
     *          - < -1 for other errors that prevented normal closing of the interface. The state
     *            of the interface is undefined when this value returns.
     */
    virtual libuavcan::Result closeInterface(InterfaceT*& inout_interface) = 0;

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
}  // namespace transport
}  // namespace libuavcan

#endif  // LIBUAVCAN_TRANSPORT_MEDIA_INTERFACES_HPP_INCLUDED
