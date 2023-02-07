/*
 * Copyright 2023 Amazon.com, Inc. or its affiliates. All Rights Reserved.
 */

#ifndef LIBCYPHAL_EXAMPLE_SOCKETCANINTERFACEMANAGER_HPP_INCLUDED
#define LIBCYPHAL_EXAMPLE_SOCKETCANINTERFACEMANAGER_HPP_INCLUDED

#include <string>
#include <memory>
#include <vector>

#include "libcyphal/libcyphal.hpp"
#include "SocketCANInterfaceGroup.hpp"

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
/**
 * For higher-level systems where interfaces may come and go the manager pattern allows a central
 * authority to monitor hardware availability and to provide a single location for interface lifecycle
 * management. For low-level systems this could be a very simple and static object that assumes interfaces
 * are never closed.
 */
class SocketCANInterfaceManager final
    : public libcyphal::media::InterfaceManager<SocketCANInterfaceGroup, std::shared_ptr<SocketCANInterfaceGroup>>
{
    const std::vector<std::string> required_interfaces_;
    bool                           enable_can_fd_;
    bool                           receive_own_messages_;

public:
    // +----------------------------------------------------------------------+
    // | RULE OF SIX
    // +----------------------------------------------------------------------+

    SocketCANInterfaceManager(const SocketCANInterfaceManager&) = delete;
    SocketCANInterfaceManager(SocketCANInterfaceManager&&)      = delete;
    SocketCANInterfaceManager& operator=(const SocketCANInterfaceManager&)   = delete;
    SocketCANInterfaceManager& operator&&(const SocketCANInterfaceManager&&) = delete;

    /**
     * Required constructor.
     * @param  enable_can_fd    If true then the manager will attempt to enable CAN-FD
     *                          for all interfaces opened.
     * @param  receive_own_messages If true then the manager will enable receiving messages
     *                          sent by this process. This is used only for testing.
     */
    SocketCANInterfaceManager(const std::vector<std::string>&& required_interfaces,
                              bool                             enable_can_fd,
                              bool                             receive_own_messages);

    virtual ~SocketCANInterfaceManager() = default;

    bool doesReceiveOwnMessages() const;

    // +----------------------------------------------------------------------+
    // | InterfaceManager
    // +----------------------------------------------------------------------+

    virtual libcyphal::Result startInterfaceGroup(const InterfaceGroupType::FrameType::Filter* filter_config,
                                                  std::size_t                                  filter_config_length,
                                                  InterfaceGroupPtrType&                       out_group) override;

    virtual libcyphal::Result stopInterfaceGroup(std::shared_ptr<SocketCANInterfaceGroup>& inout_group) override;

    virtual std::size_t getMaxFrameFilters() const override;

    // +----------------------------------------------------------------------+
private:
    /**
     * Opens an interface for receiveing and transmitting.
     *
     * @param       interface_index         The index to assign to the interface. This is the index used in the
     *                                      libcyphal::media::InterfaceGroup interface.
     * @param       interface_name          The name of the interface to open on the system. This is the primary
     *                                      identifier used by the example to open a socket.
     * @param       filter_config           An array of frame filtering parameters. The contents and behaviour of
     *                                      filters is dependant on the interface in use and the Frame type.
     * @param       filter_config_length    The number of filter configurations in the filter_config array.
     * @param[out]  out_interface           If successful, the pointer is populated with a new interface object. The
     *                                      caller owns this memory after the method exits.
     * @return libcyphal::Result::Success if the interface was successfully opened and returned,
     */
    libcyphal::Result createInterface(std::uint_fast8_t                                          interface_index,
                                      const std::string&                                         interface_name,
                                      const typename SocketCANInterfaceGroup::FrameType::Filter* filter_config,
                                      std::size_t                                                filter_config_length,
                                      std::unique_ptr<SocketCANInterfaceGroup::InterfaceType>&   out_interface);
};
}  // namespace example
/** @} */  // end of examples group
}  // namespace libcyphal

#endif  // LIBCYPHAL_EXAMPLE_SOCKETCANINTERFACEMANAGER_HPP_INCLUDED
