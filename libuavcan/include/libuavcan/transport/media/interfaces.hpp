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
 * @tparam  FrameType    The media-specific frame type to be exchanged across this interface.
 */
template <typename FrameType>
class LIBUAVCAN_EXPORT Interface
{
public:
    virtual ~Interface() = default;
    /**
     * Return the index for this interface. The interface index is the canonical identifier used by libuavcan
     * to open, close, and access a given interface. Per the v1 specification, lower indicies are preferred
     * when receiving messages through redundant groups.
     *
     * @return This interface's index.
     */
    virtual std::uint_fast16_t getInterfaceIndex() const = 0;

    /**
     * Non-blocking transmission.
     *
     * If the frame wasn't transmitted by the TX deadline, the interface shall discard it.
     *
     * @note There are no requirements on a given media layer implementation to limit the size of the
     * internal buffers. As such, enqueuing large numbers of messages may use an arbitrarily large
     * amount of system resources. It is up to the media layer integrator to decide how to handle
     * this. The simplest strategy is to put a hard upper limit and return -1 (i.e. buffer full) when
     * this limit is reached. Other strategies could include application-specific interfaces implemented
     * by the same underlying object implementing this Interface that allowed a component to monitor
     * memory usage, peripheral state, and application activity to coordinate an optimal response
     * (e.g. If this were an interface to a CAN driver and the driver was off-bus the coordinating
     * component could choose to change the application state so it would no longer try to send
     * messages. etc).
     *
     * @image html html/media_interface_fig1.png width=100%
     * @image latex latex/media_interface_fig1.eps
     *
     * <h3>Media Layer Requirements</h3>
     *
     * 1. Valid @ref MediaDevGuide "media layer" implementations must guarantee a +- 1 second
     * resolution when evaluating this value. This allows for messages to be discarded 1 up to one second
     * before they expire or be transmitted 1 second after they expire. Where an implementation can measure
     * this time more accurately, it should.
     *
     * 2. Because of (1) and because this timer is monotonic, clock-skew in the media layer's internal monotonic
     * clock is not acceptable and could lead to data loss.
     *
     * 3. When rounding time values implementations should prefer to retain a message rather than discard it.
     *
     * @param   tx_deadline The time, measured from when this call completes, within which the frame
     *                      should be transmitted. If this time expires the frame should be discarded.
     *
     * @return
     *          - 0 = one frame enqueued for transmission.
     *          - -1 = TX buffer full.
     *          - All other negative values are errors.
     */
    virtual libuavcan::Result enqueue(const FrameType& frame, libuavcan::time::Monotonic tx_deadline) = 0;

    /**
     * Non-blocking transmission. Messages enqueued for transmission using this method will remain enqueued
     * until they are transmitted.
     *
     * @note See note on the enqueue(const FrameType&, libuavcan::time::Monotonic) override for a discussion
     * of memory utilization by objects implementing this interface.
     *
     * @return
     *          - 0 = one frame enqueued for transmission.
     *          - -1 = TX buffer full.
     *          - All other negative values are errors.
     */
    virtual libuavcan::Result enqueue(const FrameType& frame) = 0;

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
    virtual libuavcan::Result popBack(FrameType& out_frame) = 0;

    /**
     * Send one message and receive one message.
     */
    virtual libuavcan::Result exchange() = 0;
};

/**
 * Manages access to and the lifecycle of media interfaces to a single bus type. A given InterfaceManager
 * shall only expose interfaces to busses that are used as redundant media for the same transport (i.e. An
 * interface manager should define a single logical bus). How manager objects are exposed to an application
 * is not specified by libuavcan.
 *
 * @tparam  FrameType       The media-specific frame type to be exchanged on interfaces opened through
 *                          this manager.
 * @tparam  InterfaceType   The type to use for interfaces. Must implement Interface.
 */
template <typename FrameType, typename InterfaceType = Interface<FrameType>>
class LIBUAVCAN_EXPORT InterfaceManager
{
public:
    static_assert(std::is_base_of<Interface<FrameType>, InterfaceType>::value,
                  "InterfaceType must implement libuavcan::transport::media::Interface.");

    virtual ~InterfaceManager() = default;

    /**
     * Opens an interface for receiveing and transmitting.
     *
     * @param       interface_index         The index of the interface to open. See getHardwareInterfaceCount()
     *                                      for the valid range of interface indicies. The behaviour of this method
     *                                      given an index > getHardwareInterfaceCount() is implementation defined.
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
    virtual libuavcan::Result openInterface(std::uint_fast16_t                interface_index,
                                            const typename FrameType::Filter* filter_config,
                                            std::size_t                       filter_config_length,
                                            InterfaceType*&                   out_interface) = 0;

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
    virtual libuavcan::Result closeInterface(InterfaceType*& inout_interface) = 0;

    /**
     * The total number of available hardware interfaces. On some systems additional virtual interfaces can be
     * created but libuavcan requires interfaces backed by hardware to allow redundancy guarantees to be met.
     *
     * @return The number of interfaces backed by hardware interfaces to a bus. The set of unsigned integers
     *         [0, getHardwareInterfaceCount()) are always valid interface indicies where the hardware interface
     *         count is > 0. Values > the hardware interface count may or may not be valid depending on the
     *         specific implementation of this interface.
     */
    virtual std::size_t getHardwareInterfaceCount() const = 0;

    /**
     * The number of hardware filters available for a given interface.
     * @param  interface_index  The index of the interface to get a hardware filter count for.
     *
     * @return Available hardware Frame filters for interface_index.
     */
    virtual std::size_t getMaxHardwareFrameFilters(std::uint_fast16_t interface_index) const = 0;

    /**
     * The number of filters that an interface will accept where some or all of the frame
     * filtering may be performed in software (i.e. this is a super-set that includes any available
     * hardware filters). For some implementations this value may be limited only by available system
     * resources. On such systems configuring large numbers of filters may degrade performance.
     *
     * @param  interface_index  The index of the interface to get a total filter count for.
     * @return Available Frame filters for interface_index.
     */
    virtual std::size_t getMaxFrameFilters(std::uint_fast16_t interface_index) const = 0;
};

}  // namespace media
}  // namespace transport
}  // namespace libuavcan

#endif  // LIBUAVCAN_TRANSPORT_MEDIA_INTERFACES_HPP_INCLUDED
