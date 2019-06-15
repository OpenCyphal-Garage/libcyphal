/*
 * Copyright 2019 Amazon.com, Inc. or its affiliates. All Rights Reserved.
 */

#include <queue>
#include <memory>
#include <string>

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
using CanFrame = libuavcan::transport::media::CAN::Frame<libuavcan::transport::media::CAN::Type2_0::MaxFrameSizeBytes>;
using CanInterface = libuavcan::transport::media::Interface<CanFrame>;

using CanInterfaceManager = libuavcan::transport::media::InterfaceManager<libuavcan::example::CanFrame>;
using CanFilterConfig     = libuavcan::example::CanFrame::Filter;

/**
 * This datastruture is part of the SocketCANInterfaceManager's internal
 * interface management storage.
 */
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

    const std::string             name;
    std::unique_ptr<CanInterface> connected_interface;
};

/**
 * For higher-level systems where interfaces may come and go the manager pattern allows a central
 * authority to monitor hardware availability and to provide a single location for interface lifecycle
 * management. For low-level systems this could be a very simple and static object that assumes interfaces
 * are never closed.
 */
class SocketCANInterfaceManager : public CanInterfaceManager
{
private:
    std::vector<InterfaceRecord> interface_list_;

public:
    // +----------------------------------------------------------------------+
    // | RULE OF SIX
    // +----------------------------------------------------------------------+

    SocketCANInterfaceManager(const SocketCANInterfaceManager&) = delete;
    SocketCANInterfaceManager(SocketCANInterfaceManager&&)      = delete;
    SocketCANInterfaceManager& operator=(const SocketCANInterfaceManager&)   = delete;
    SocketCANInterfaceManager& operator&&(const SocketCANInterfaceManager&&) = delete;

    SocketCANInterfaceManager();

    virtual ~SocketCANInterfaceManager();

    // +----------------------------------------------------------------------+
    // | InterfaceManager
    // +----------------------------------------------------------------------+

    virtual libuavcan::Result openInterface(std::uint_fast8_t      interface_index,
                                            const CanFilterConfig* filter_config,
                                            std::size_t            filter_config_length,
                                            CanInterface*&         out_interface) override;

    virtual libuavcan::Result closeInterface(CanInterface*& inout_interface) override;

    virtual std::uint_fast8_t getHardwareInterfaceCount() const override;

    virtual std::size_t getMaxHardwareFrameFilters(std::uint_fast8_t interface_index) const override;

    virtual std::size_t getMaxFrameFilters(std::uint_fast8_t interface_index) const override;

    // +----------------------------------------------------------------------+

    const std::string& getInterfaceName(std::size_t interface_index) const;
    const std::string& getInterfaceName(CanInterface& interface) const;
    std::size_t        reenumerateInterfaces();

private:
    std::int16_t configureFilters(const int                    fd,
                                  const CanFilterConfig* const filter_configs,
                                  const std::size_t            num_configs);
    /**
     * Open and configure a CAN socket on iface specified by name.
     * @param iface_name String containing iface name, e.g. "can0", "vcan1", "slcan0"
     * @return Socket descriptor or negative number on error.
     */
    static int openSocket(const std::string& iface_name, bool enable_canfd);
};
}  // namespace example
/** @} */  // end of examples group
}  // namespace libuavcan
