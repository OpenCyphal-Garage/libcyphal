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
 * Non-blocking connection to a UAVCAN bus with isolated rx and tx queues. While libuavcan may share
 * hardware peripherals with other components and/or processes for a given system a media layer interface
 * group object shall be the sole access to hardware connections to a bus for this library.
 *
 * When a group has more than one interface the group shall be managed as a set of redundant connections to
 * a single, physical bus. Libuavcan shall use the first N interfaces it is capable of useing for redundancy.
 * It may not use all available intefaces where the group is larger than the library's built-in support for
 * a fixed number of redundant channels. Because of this, media layer implementations should use constants in
 * higher layers to determine the optimal number of interfaces to provide to interface groups. Because this is
 * also a hardware constraint system designers should consider the trade off in redundancy, complexity, and
 * memory resources that are affected by the number of redundant interfaces in each group. See the
 * @ref TransportGuide for full documentation.
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
class LIBUAVCAN_EXPORT InterfaceGroup
{
public:
    virtual ~InterfaceGroup() = default;

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
     * Get the number of interfaces in this group. Valid indicies where
     * used as an input argument are [0..getInterfaceCount()).
     * @return 1-* representing the number of interfaces in this group.
     *         If 0 is returned then the object itself is invalid and
     *         the library entered undefined behaviour sometime before
     *         or while this method was called.
     */
    virtual std::uint_fast8_t getInterfaceCount() const = 0;

    /**
     * Non-blocking transmission. All implementations will have some intermediate buffer this method
     * writes to since it does not block on actual transmission of the frame. For some implementations,
     * this method may borrow CPU time to move another, higher priority frame into a lower-level queue
     * after enqueueing the given one in an intermediate queue.
     *
     * This inteface does not provide a "write to all interfaces in group" because of the complexity in
     * handling partial failures. Higher layers must handle the logic of dispatching messages accross
     * redundant interfaces in groups and handle individual interface failures appropriately.
     *
     * @note Implementations are allowed to provide queues based on message priority. Because of this,
     * if a given message cannot be written the media layer should keep trying to write other messages
     * with a different priority.
     *
     * @param  interface_index  The index of the interface in the group to write the frames to.
     * @param  frames           1..* frames to write into the system queues for immediate transmission.
     * @param  frames_len       The number of frames in the frames array that should be sent
     *                          (starting from frame 0).
     * @param  out_frames_written
     *                          The number of frames written. If this is less than frames_len then frames
     *                          [0 - out_frames_written) were enqueued for transmission. Frames
     *                          [out_frames_written - frames_len) were not able to be sent. Nominally this is
     *                          due to the internal queues being full.
     * @return libuavcan::Result::SuccessPartial if some but not all of the frames were written.
     *         libuavcan::Result::BufferFull if no frames could be written because the buffer was
     *         full.
     *         libuavcan::Result::Success if all frames were written.
     */
    virtual libuavcan::Result write(std::uint_fast8_t interface_index,
                                    const FrameT (&frames)[MaxTxFrames],
                                    std::size_t  frames_len,
                                    std::size_t& out_frames_written) = 0;

    /**
     * Non-blocking reception.
     *
     * Timestamps should be provided by the media driver, ideally by the hardware peripheral.
     *
     * While the monotonic timestamp is required the UAVCAN protocol can tolerate imprecision since
     * these timestamps are used only for protocol timing validation  (i.e. transfer timeouts and
     * inter-transfer intervals).
     *
     * @param  interface_index  The index of the interface in the group to read the frames from.
     * @param  out_frames       A buffer of frames to read.
     * @param  out_frames_read  On output the number of frames read into the out_frames array.
     * @return libuavcan::Result::Success If no errors occurred.
     */
    virtual libuavcan::Result read(std::uint_fast8_t interface_index,
                                   FrameT (&out_frames)[MaxRxFrames],
                                   std::size_t& out_frames_read) = 0;

    /**
     * Reconfigure the filters for all interfaces in the group. This is an optional method that is only
     * required if applications wish to dynamically subscribe and unsubscribe to messages after the media
     * layer has been initialized. For less dynamic applications this method can simply return
     * libuavcan::Result::NotImplemented.
     *
     * @param  filter_config         The filtering to apply equally to all members of the group.
     * @param  filter_config_length  The length of the @p filter_config argument.
     * @return libuavcan::Result::Success if the group's receive filtering was successfully reconfigured.
     *         libuavcan::Result::NotImplemented if this media layer implementation does not support
     *         dynamic filter reconfiguration.
     *         Errors shall be returned if one or more interfaces in this group are not configured since
     *         this will leave the group in an unspecified state.
     */
    virtual libuavcan::Result reconfigureFilters(const typename FrameType::Filter* filter_config,
                                                 std::size_t                       filter_config_length) = 0;

    /**
     * Block for a specified amount of time or until any interface in the group becomes ready to read or write.
     *
     * Note that this method may return even if no requested events actually occurred.
     *
     * @param [in]     timeout                  The amount of time to wait for an event.
     * @param [in]     ignore_write_available   If true then this method will not return if interfaces become
     *                                          available only for write.
     *
     * @return  libuavcan::Result::SuccessTimeout if no events ocurred but the select operation timedout.
     *          libuavcan::Result::Success if one or more of the provided interfaces are ready for read, and if
     *          @p ignore_write_available is false, or write.
     *          libuavcan::Result::SuccessPartial if one or more errors were reported for one or more interfaces.
     */
    virtual libuavcan::Result select(libuavcan::duration::Monotonic timeout, bool ignore_write_available) = 0;
};

/**
 * Manages the lifecycle of groups of media interfaces for a single bus type. A given InterfaceManager
 * shall only expose interfaces to busses that are used as redundant media for the same transport (i.e. An
 * interface manager should define a single logical bus). How manager objects are exposed to an application
 * is not specified by libuavcan. Manager object shall remain valid for the as long as any other object in
 * libuavcan and all memory owned by the manager shall remain valid after startInterfaceGroup has been called
 * and until/unless stopInterfaceGroup is called. It is undefined behaviour for the InterfaceManager to become
 * invalid before or during a call to stopInterfaceGroup (unless startInterfaceGroup was never called).
 *
 * @tparam  InterfaceGroupT         The type to use for interface groups. Must implement InterfaceGroup.
 * @tparam  InterfaceGroupPtrT      The pointer type to use for InterfaceGroupPtrT pointers in this class. On
 *                                  systems without dynamic memory this can be a raw pointer
 *                                  <code>InterfaceGroupT*</code> but where dynamic memory is used some sort of
 *                                  smart pointer is recommended.
 */
template <typename InterfaceGroupT, typename InterfaceGroupPtrT>
class LIBUAVCAN_EXPORT InterfaceManager
{
public:
    virtual ~InterfaceManager() = default;

    /**
     * The media-specific interface group type managed by this object.
     */
    using InterfaceGroupType = InterfaceGroupT;

    /**
     * The pointer type used for pointers to InterfaceGroupType. This can be a raw pointer or a smart pointer.
     * This template parameter is provided to allow either based on how a given media layer is implemented.
     */
    using InterfaceGroupPtrType = InterfaceGroupPtrT;

    /**
     * Called by libuavcan when it is starting to use the group of interfaces managed by this object.
     * Libuavcan shall only invoke this once initially and only ever again if stopInterfaceGroup was first successfully
     * invoked.
     * @param  filter_config         The filtering to apply equally to all members of the group.
     * @param  filter_config_length  The length of the @p filter_config argument.
     * @param  out_group             A pointer to set to the started group. This will be nullptr if the start method
     *                               fails.
     * @return libuavcan::Result::Success if the group was successfully started and a valid pointer was returned.
     *         libuavcan::Result::SuccessParital can be returned to signal that a desired level of redundancy was not
     *         achieved but libuavcan may not modify its behavior based on this signal.
     *         The caller should assume that @p out_group is an invalid pointer if any failure is returned.
     */
    virtual libuavcan::Result startInterfaceGroup(const typename InterfaceGroupType::FrameType::Filter* filter_config,
                                                  std::size_t            filter_config_length,
                                                  InterfaceGroupPtrType& out_group) = 0;

    /**
     * Called by libuavcan when it is finished with the interface group. Libuavcan shall not invoke any further methods
     * on the interfaces nor use any memory obtained by or given to interface groups after this call in entered. Media
     * layer implementations do not need to actually delete the interface group or perform any specific action in this
     * method. It is provided solely to create a "hands-off" contract between libuavcan and a media layer
     * implementation.
     * @return libuavcan::Result::Success. Libuavcan will not respond to any failures reported by this method.
     */
    virtual libuavcan::Result stopInterfaceGroup(InterfaceGroupPtrType& inout_group) = 0;

    /**
     * The number of filters that an interface will accept where some or all of the frame
     * filtering may be performed in software (i.e. this is a super-set that includes any available
     * hardware filters). For some implementations this value may be limited only by available system
     * resources. On such systems configuring large numbers of filters may degrade performance.
     * If for some reason a group of interfaces support a different number of filters this method shall return
     * the smallest maximum supported.
     *
     * @return The maximum number of frame filters available for filter groups managed by this object.
     */
    virtual std::size_t getMaxFrameFilters() const = 0;
};

}  // namespace media
}  // namespace libuavcan

#endif  // LIBUAVCAN_TRANSPORT_MEDIA_INTERFACES_HPP_INCLUDED
