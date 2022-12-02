/*
 * Copyright 2019 Amazon.com, Inc. or its affiliates. All Rights Reserved.
 */

#ifndef LIBCYPHAL_EXAMPLE_SOCKETCANINTERFACEGROUP_HPP_INCLUDED
#define LIBCYPHAL_EXAMPLE_SOCKETCANINTERFACEGROUP_HPP_INCLUDED

#include <memory>
#include <vector>

#include <poll.h>

#include "libcyphal/libcyphal.hpp"
#include "SocketCANInterface.hpp"

namespace libcyphal
{
/**
 * @defgroup examples Examples
 *
 * @{
 * @file
 */
namespace example
{
class SocketCANInterfaceGroup final : public libcyphal::media::InterfaceGroup<SocketCANInterface::FrameType,
                                                                              SocketCANInterface::TxFramesLen,
                                                                              SocketCANInterface::RxFramesLen>
{
public:
    using InterfaceType = SocketCANInterface;

private:
    std::vector<std::unique_ptr<InterfaceType>> interfaces_;
    std::vector<struct ::pollfd>                pollfds_;

public:
    // +----------------------------------------------------------------------+
    // | RULE OF SIX
    // +----------------------------------------------------------------------+
    SocketCANInterfaceGroup(const SocketCANInterfaceGroup&) = delete;
    SocketCANInterfaceGroup(SocketCANInterfaceGroup&&)      = delete;
    SocketCANInterfaceGroup& operator=(const SocketCANInterfaceGroup&)   = delete;
    SocketCANInterfaceGroup& operator&&(const SocketCANInterfaceGroup&&) = delete;

    /**
     * Required constructor.
     * @param  enable_can_fd    If true then the manager will attempt to enable CAN-FD
     *                          for all interfaces opened.
     * @param  receive_own_messages If true then the manager will enable receiving messages
     *                          sent by this process. This is used only for testing.
     */
    SocketCANInterfaceGroup(std::vector<std::unique_ptr<InterfaceType>>&& interfaces);

    virtual ~SocketCANInterfaceGroup() = default;

    const std::string& getInterfaceName(std::uint_fast8_t index) const;

    const SocketCANInterface* getInterface(std::uint_fast8_t index) const;

    static libcyphal::Result configureFilters(const int                      socket_descriptor,
                                              const FrameType::Filter* const filter_configs,
                                              const std::size_t              num_configs);

    // +----------------------------------------------------------------------+
    // | libcyphal::media::InterfaceGroup
    // +----------------------------------------------------------------------+
    virtual std::uint_fast8_t getInterfaceCount() const override;
    virtual libcyphal::Result write(std::uint_fast8_t interface_index,
                                    const InterfaceType::FrameType (&frames)[TxFramesLen],
                                    std::size_t  frames_len,
                                    std::size_t& out_frames_written) override;

    virtual libcyphal::Result read(std::uint_fast8_t interface_index,
                                   InterfaceType::FrameType (&out_frames)[RxFramesLen],
                                   std::size_t& out_frames_read) override;

    virtual libcyphal::Result reconfigureFilters(const typename FrameType::Filter* filter_config,
                                                 std::size_t                       filter_config_length) override;

    virtual libcyphal::Result select(libcyphal::duration::Monotonic timeout, bool ignore_write_available) override;
};

}  // namespace example
/** @} */  // end of examples group
}  // namespace libcyphal

#endif  // LIBCYPHAL_EXAMPLE_SOCKETCANINTERFACEGROUP_HPP_INCLUDED
