/// @copyright Copyright Amazon.com Inc. and its affiliates. All Rights Reserved.
/// @file
/// The internal implementation of the Cyphal Node

#ifndef LIBCYPHAL_NODE_HPP_INCLUDED
#define LIBCYPHAL_NODE_HPP_INCLUDED

#include <cstdint>
#include "libcyphal/types/common.hpp"
#include "libcyphal/node/discovery.hpp"  // Used by Node to fetch its Node ID. This should be implemented by the user
#include "libcyphal/node/informant.hpp"  // Used by Node to fetch the various GetInfo or Heartbeat info. This should be implemented by the user
#include "libcyphal/transport.hpp"
#include "libcyphal/transport/listener.hpp"
#include "libcyphal/transport/metadata.hpp"
#include "libcyphal/types/status.hpp"
#include "libcyphal/types/time.hpp"

namespace libcyphal
{

constexpr std::uint8_t MaxListeners = 8;

/// @brief The Cyphal Node Interface which publishes the "uavcan.node.HeartBeat" and services the "uavcan.node.GetInfo"
class Node : transport::Listener
{
public:
    enum class Health : EnumType
    {
        Nominal  = 0,  //!< The Node functioning properly..
        Advisory = 1,  //!< The Node is able to perform it's function but some critical parameter is out of range or
                       //!< minor failure occurred.
        Caution = 2,  //!< A major failure occurred and the Node is in a degraded operational mode or performing outside
                      //!< it's limitations.
        Warning = 3,  //!< The most critical. The Node is unable to perform it's function.

        _max
    };

    // TODO: Mode probably shouldn't be here
    enum class Mode : EnumType
    {
        Operational    = 0,  //!< The node in the mode to perform it's intended function.
        Initialization = 1,  //!< The default mode of the Node
        Maintenance    = 2,  //!< The mode given when the Node is undergoing operations which prevent normal operations
        SoftwareUpdate = 3,  //!< The mode given when the Node is able to update it's software.

        _max
    };

    using VendorSpecificStatusCode = std::uint8_t;

    /// @brief Initializes the Cyphal Node. The default mode will be 'Initializing' and in Warning until initialized
    /// correctly.
    /// @param[in] informant The reference to the informant which can tell the Node what it's various attributes are.
    /// @param[in] discovery Reference to a Discovery instance for providing node ID information to the Transports
    /// @param[in] transport Reference to a Transport instance for providing interface to the bus
    /// @return Result::NOT_READY The node can not start yet. The internal state of the Node was not configured during
    /// boot. (check for call to init()).
    /// @return Result::ALREADY_INITIALIZED The node has already been initialized.
    /// @return Result::SUCCESS The node started.
    /// @post GetMode - Once the Mode is Mode::Operational then the Node is ready.
    virtual Status Initialize(node::Informant& informant, node::Discovery& discovery, Transport& transport);

    inline Node::Mode GetMode() const
    {
        return mode_;
    }
    Status SetTargetMode(Node::Mode mode);

    inline Node::Health GetHealth() const
    {
        return health_;
    }
    Status SetHealth(Node::Health h);

    inline std::uint8_t GetVendorSpecificStatusCode() const
    {
        return vssc_;
    }
    Status SetVendorSpecificStatusCode(std::uint8_t vssc)
    {
        vssc_ = vssc;
    }

    // Listener
    virtual void onReceive(const transport::RxMetadata rx_metadata, const Message& msg) noexcept override;

    /// Allows a node to publish a serialized broadcast on its transport
    /// @param [in] tx_metadata the tx_metadata for this message which should be filled out by the user
    /// @param [in] msg The read only reference to the message information.
    /// @retval result_e::SUCCESS Message transmitted
    /// @retval result_e::BUSY The underlying transport is busy can can not transmit
    /// @retval result_e::FAILURE Could not transmit the message.
    virtual Status Publish(transport::TxMetadata tx_metadata, const Message& msg);

    virtual Status AddListener(transport::Listener& listener);

    // Task
    void Execute();

protected:
    ~Node() = default;

    virtual void PerformDiscovery();
    virtual void ProcessOutgoing();
    virtual void ProcessIncoming();

    const time::Timer& timer_;  // TODO: Use generic Timer interface which user will do implementation for

    bool is_ready_for_initialization_;
    bool is_initialized_;

    // HeartBeat and GetInfo required by the Cyphal spec
    node::Informant*      informant_;
    bool                  is_informed_;
    NodeID                node_id_;
    Node::Mode            mode_;
    Node::Health          health_;
    std::uint8_t          vssc_;
    node::Version         sw_version_;
    node::Version         hw_version_;
    std::uint64_t         sw_revision_;
    std::uint64_t         crc_64_we_;
    UID                   node_uid_;
    node::Informant::Name node_name_;
    node::Informant::COA  certificate_;

    // Node ID discovery
    node::Discovery::Type node_id_type_;
    node::Discovery*      discovery_;

    List<Listener, MaxListeners> listener_list_;
    // Implement
    // Transport transport_;
};

}  // namespace libcyphal

#endif  // LIBCYPHAL_NODE_HPP_INCLUDED
