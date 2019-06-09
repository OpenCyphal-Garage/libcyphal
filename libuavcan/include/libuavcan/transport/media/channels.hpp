/*
 * Copyright (C) 2014 Pavel Kirienko <pavel.kirienko@gmail.com>
 * Copyright 2019 Amazon.com, Inc. or its affiliates. All Rights Reserved.
 */
/** @file */

#ifndef LIBUAVCAN_TRANSPORT_MEDIA_CHANNELS_HPP_INCLUDED
#define LIBUAVCAN_TRANSPORT_MEDIA_CHANNELS_HPP_INCLUDED

#include <array>
#include <algorithm>

#include "libuavcan/libuavcan.hpp"
#include "libuavcan/introspection.hpp"
#include "libuavcan/time.hpp"

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
 *
 * See the @ref MediaDevGuide for details
 * on porting the media layer to a given platform.
 */
namespace media
{
/**
 * Filter config struct.
 * TODO: document and examples
 */
struct LIBUAVCAN_EXPORT FilterConfig
{
    std::uint32_t id;
    std::uint32_t mask;

    bool operator==(const FilterConfig& rhs) const
    {
        return rhs.id == id && rhs.mask == mask;
    }

    FilterConfig()
        : id(0)
        , mask(0)
    {}
};

/**
 * Single non-blocking connection to a UAVCAN bus with isolated rx and tx queues. While channels
 * logically operate independently they may share physical peripherals for some platforms.
 *
 * @tparm  FrameType    The media-specific frame type to be exchanged on this channel.
 */
template <typename FrameType>
class LIBUAVCAN_EXPORT Channel
{
public:
    virtual ~Channel() = default;

    /**
     * Non-blocking transmission.
     *
     * If the frame wasn't transmitted upon TX deadline, the channel should discard it.
     *
     * @note
     * It is likely that the library will want to send frames passed into a ChannelCluster::send()
     * method as the next ones to transmit, but it is <em>not</em> guaranteed. The library can replace
     * those with new frames between the calls.
     *
     *
     * @return 1 = one frame transmitted, 0 = TX buffer full, negative for error.
     */
    virtual libuavcan::Result send(const FrameType& frame, libuavcan::time::Monotonic tx_deadline) = 0;

    /**
     * Non-blocking reception.
     *
     * Timestamps should be provided by the media driver, ideally by the hardware peripheral.
     *
     * While the monotonic timestamp is required the UAVCAN protoocol can tolerate imprecision since
     * these timestamps are used only for protocol timing validation  (i.e. transfer timeouts and
     * inter-transfer intervals).
     *
     * TODO: synchronized time will be designed back in later.
     *
     * @param [out] out_ts_monotonic Monotonic timestamp, mandatory.
     * @return 1 = one frame received, 0 = RX buffer empty, negative for error.
     */
    virtual libuavcan::Result receive(FrameType& out_frame, libuavcan::time::Monotonic& out_ts_monotonic) = 0;

    /**
     * Configure message filters.
     *
     * @return 0 = success, negative for error.
     */
    virtual libuavcan::Result configureFilters(const std::array<FilterConfig>& config) = 0;

    /**
     * The number of filters that configureFilters will accept where all Frame filtering will
     * be performed by hardware.
     */
    virtual std::size_t getMaxHardwareFilters() const = 0;

    /**
     * The number of filters that configureFilters will accept where some or all of the frame
     * filtering may be performed in software (i.e. this is a super-set that includes any available
     * hardware filters). For some implementations this value may be limited only by available system
     * resources. On such systems configuring large numbers of filters may degrade performance.
     */
    virtual std::size_t getMaxFilters() const = 0;
};

/**
 * A group of 1 to 3 channels used as a single channel with redundancy.
 *
 * @tparm  FrameType    The media-specific frame type to be exchanged on this cluster.
 */
template <typename FrameType>
class LIBUAVCAN_EXPORT ChannelCluster : public Channel
{
public:
    virtual ~ChannelCluster() = default;

    /**
     * Returns the number of channels in this cluster.
     */
    virtual std::uint8_t getChannelCount() const = 0;

    /**
     * Get an individual channel in the cluster.
     */
    virtual Channel& getChannel(std::uint8_t channel_index) = 0;
};

/**
 * Manages access to and the lifecycle of media Channels. A given media layer implementation
 * should have a single ChannelManager for a given FrameType. How this object is exposed to
 * an application is not specified by libuavcan.
 *
 * @tparm  FrameType    The media-specific frame type to be exchanged on channels opened through
 *                      this manager.
 */
template <typename FrameType>
class LIBUAVCAN_EXPORT ChannelManager
{
public:
    virtual ~ChannelManager() = default;

    /**
     * Opens a channel.
     */
    virtual libuavcan::Result openChannel(std::uint8_t channel_index, Channel*& out_channel) = 0;

    /**
     * Closes a channel.
     */
    virtual libuavcan::Result closeChannel(std::uint8_t channel_index, Channel*& inout_channel) = 0;

    /**
     * Opens a set of channels acting as a redundant cluster.
     */
    virtual libuavcan::Result createCluster(std::array<std::uint8_t> channel_indices, ChannelCluster*& out_cluster) = 0;

    /**
     * Creates a redundant cluster of channels out of a set that were already opened.
     */
    virtual libuavcan::Result createCluster(std::array<Channel&> channels, ChannelCluster*& out_cluster) = 0;

    /**
     * Closes all the channels in a cluster and releases any resources held by the cluster itself.
     */
    virtual libuavcan::Result destroyCluster(ChannelCluster*& inout_cluster) = 0;

    /**
     * Total number of available channels. For some systems where channels are software-defined
     * this value may be limited only by available system resources. For such systems opening
     * large numbers of channels may degrade performance.
     *
     * @note
     * This value shall not change after a system has been initalized.
     */
    virtual std::size_t getChannelCount() const = 0;
};

}  // namespace media
}  // namespace transport
}  // namespace libuavcan

#endif LIBUAVCAN_TRANSPORT_MEDIA_CHANNELS_HPP_INCLUDED
