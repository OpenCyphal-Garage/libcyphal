/// @copyright Copyright Amazon.com Inc. and its affiliates. All Rights Reserved.
/// @file
/// Cyphal Transport ID types

#ifndef LIBCYPHAL_TYPES_TRANSPORT_ID_TYPES_HPP_INCLUDED
#define LIBCYPHAL_TYPES_TRANSPORT_ID_TYPES_HPP_INCLUDED

#include <cstdint>
#include "libcyphal/types/common.hpp"

namespace libcyphal
{

using NodeID     = std::uint16_t;
using PortID     = std::uint16_t;
using TransferID = std::uint64_t;

}  // namespace libcyphal

#endif  // LIBCYPHAL_TYPES_TRANSPORT_ID_TYPES_HPP_INCLUDED
