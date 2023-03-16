/// @copyright Copyright Amazon.com Inc. and its affiliates. All Rights Reserved.
/// @file
/// Defines the transport metadat information
/// @todo Need to determine if this file is needed or if libcyphal should just use
///   the redundant definitions of these types defined in libUdpard and libCanard

#ifndef LIBCYPHAL_TRANSPORT_METADATA_HPP_INCLUDED
#define LIBCYPHAL_TRANSPORT_METADATA_HPP_INCLUDED

#include "libcyphal/transport/id_types.hpp"
#include "libcyphal/types/time.hpp"
#include "libcyphal/types/common.hpp"

namespace libcyphal
{
namespace transport
{

/// These correspond to the libudpard/libcanard TransferPriority.
/// @todo Make these common amongst libcyphal, libudpard, libcanard
typedef enum
{
    PriorityExceptional = 0,
    PriorityImmediate   = 1,
    PriorityFast        = 2,
    PriorityHigh        = 3,
    PriorityNominal     = 4,  ///< Nominal priority level should be the default.
    PriorityLow         = 5,
    PrioritySlow        = 6,
    PriorityOptional    = 7,
} TransferPriority;

/// Transfer kinds as defined by the Cyphal Specification.
/// @todo Make these common amongst libcyphal, libudpard, libcanard
typedef enum
{
    TransferKindMessage  = 0,  ///< Multicast, from publisher to all subscribers.
    TransferKindResponse = 1,  ///< Point-to-point, from server to client.
    TransferKindRequest  = 2,  ///< Point-to-point, from client to server.
} TransferKind;

/// A structure to hold metadata for message being transmitted
/// @todo Make this common amongst libcyphal, libudpard, libcanard
struct TxMetadata
{
    TransferKind     kind;
    TransferPriority priority;
    PortID           port_id;
    NodeID           remote_node_id;
};

/// A structure to hold metadata for message being received
/// @todo Make this common amongst libcyphal, libudpard, libcanard
struct RxMetadata
{
    TransferKind                     kind;
    TransferPriority                 priority;
    PortID                           port_id;
    NodeID                           remote_node_id;
    TransferID                       transfer_id;
    time::Monotonic::MicrosecondType timestamp_us;
};

}  // namespace transport
}  // namespace libcyphal

#endif  // LIBCYPHAL_TRANSPORT_METADATA_HPP_INCLUDED
