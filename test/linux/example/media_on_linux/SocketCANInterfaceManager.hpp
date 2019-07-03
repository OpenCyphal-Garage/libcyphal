/*
 * Copyright 2019 Amazon.com, Inc. or its affiliates. All Rights Reserved.
 */

#ifndef LIBUAVCAN_EXAMPLE_SOCKETCANINTERFACEMANAGER_HPP_INCLUDED
#define LIBUAVCAN_EXAMPLE_SOCKETCANINTERFACEMANAGER_HPP_INCLUDED

#include <queue>
#include <memory>
#include <string>
#include <poll.h>

#include "libuavcan/libuavcan.hpp"
#include "SocketCANInterface.hpp"

namespace libuavcan
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
 * This datastruture is part of the SocketCANInterfaceManager's internal
 * interface management storage.
 *
 * @tparam InterfaceT   The interface type for the manager.
 */
template <typename InterfaceT>
struct InterfaceRecord final
{
    InterfaceRecord(const InterfaceRecord&) = delete;
    const InterfaceRecord& operator=(const InterfaceRecord&) = delete;

    InterfaceRecord()
        : name()
        , connected_interface()
    {}

    InterfaceRecord(const char* cname)
        : name(cname)
        , connected_interface()
    {}

    InterfaceRecord(InterfaceRecord&& rhs)
        : name(std::move(rhs.name))
        , connected_interface(std::move(rhs.connected_interface))
    {}

    ~InterfaceRecord() = default;

    const std::string           name;
    std::unique_ptr<InterfaceT> connected_interface;
};

/**
 * For higher-level systems where interfaces may come and go the manager pattern allows a central
 * authority to monitor hardware availability and to provide a single location for interface lifecycle
 * management. For low-level systems this could be a very simple and static object that assumes interfaces
 * are never closed.
 */
class SocketCANInterfaceManager : public libuavcan::media::InterfaceManager<SocketCANInterface>
{
private:
    std::vector<InterfaceRecord<InterfaceType>> interface_list_;
    struct ::pollfd                             pollfds_[MaxSelectInterfaces];
    bool                                        enable_can_fd_;
    bool                                        receive_own_messages_;

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
    SocketCANInterfaceManager(bool enable_can_fd, bool receive_own_messages);

    virtual ~SocketCANInterfaceManager();

    // +----------------------------------------------------------------------+
    // | InterfaceManager
    // +----------------------------------------------------------------------+

    virtual libuavcan::Result openInterface(std::uint_fast8_t                       interface_index,
                                            const InterfaceType::FrameType::Filter* filter_config,
                                            std::size_t                             filter_config_length,
                                            InterfaceType*&                         out_interface) override;

    virtual libuavcan::Result select(const InterfaceType* const (&interfaces)[MaxSelectInterfaces],
                                     std::size_t                    interfaces_length,
                                     libuavcan::duration::Monotonic timeout,
                                     bool                           ignore_write_available) override;

    virtual libuavcan::Result closeInterface(InterfaceType*& inout_interface) override;

    virtual std::uint_fast8_t getHardwareInterfaceCount() const override;

    virtual std::size_t getMaxHardwareFrameFilters(std::uint_fast8_t interface_index) const override;

    virtual std::size_t getMaxFrameFilters(std::uint_fast8_t interface_index) const override;

    // +----------------------------------------------------------------------+

    const std::string& getInterfaceName(std::size_t interface_index) const;
    const std::string& getInterfaceName(const InterfaceType& interface) const;
    libuavcan::Result  getInterfaceIndex(const std::string& interface_name, std::uint_fast8_t& out_index) const;
    libuavcan::Result  reenumerateInterfaces();

private:
    libuavcan::Result configureFilters(const int                                     fd,
                                       const InterfaceType::FrameType::Filter* const filter_configs,
                                       const std::size_t                             num_configs);
    /**
     * Open and configure a CAN socket on iface specified by name.
     * @param  iface_name String containing iface name, e.g. "can0", "vcan1", "slcan0"
     * @param  enable_canfd If true then the method will attempt to enable can-fd for the interface.
     * @param  enable_receive_own_messages  If true then the socket will also receive any messages sent
     *         from this process. This is normally only useful for testing.
     * @return Socket descriptor or negative number on error.
     */
    static int openSocket(const std::string& iface_name, bool enable_canfd, bool enable_receive_own_messages);
};
}  // namespace example
/** @} */  // end of examples group
}  // namespace libuavcan

#endif  // LIBUAVCAN_EXAMPLE_SOCKETCANINTERFACEMANAGER_HPP_INCLUDED
