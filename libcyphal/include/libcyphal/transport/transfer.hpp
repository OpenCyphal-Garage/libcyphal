 /// @file
/// Defines the transfer objects and types.
///
/// @copyright
/// Copyright (C) OpenCyphal Development Team  <opencyphal.org>
/// Copyright Amazon.com Inc. or its affiliates.
/// SPDX-License-Identifier: MIT
///

#ifndef LIBCYPHAL_TRANSPORT_TRANSFER_HPP_INCLUDED
#define LIBCYPHAL_TRANSPORT_TRANSFER_HPP_INCLUDED

#include <chrono>
#include "cetl/pf20/span.hpp"
#include "cetl/pf17/byte.hpp"

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


/// Cyphal transfer representation.
using Transfer = cetl::pf20::span<const cetl::pf17::byte>;

class TransferFrom : public Transfer
{
public:
    /// None indicates anonymous transfers.
    janky::optional<NodeID> source_node_id;
};

}  // namespace transport
}  // namespace libcyphal

#endif  // LIBCYPHAL_TRANSPORT_TRANSFER_HPP_INCLUDED
