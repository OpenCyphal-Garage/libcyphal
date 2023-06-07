/// @copyright Copyright Amazon.com Inc. and its affiliates. All Rights Reserved.
/// @file
/// Defines the message type (span)

#ifndef LIBCYPHAL_TRANSPORT_MESSAGE_HPP_INCLUDED
#define LIBCYPHAL_TRANSPORT_MESSAGE_HPP_INCLUDED

#include <cstddef>
#include <cstdint>
#include "libcyphal/build_config.hpp"
#include "cetl/pf20/span.hpp"

namespace libcyphal
{

/// @todo Figure out the optimal maximum message size, or make this configurable by the user
constexpr std::size_t MaxMessageSize{LIBCYPHAL_TRANSPORT_MAX_MESSAGE_SIZE_BYTES};

/// Post-serialized Message Container.
/// Serialized messages can't have their members be reliably accessed as they may not align on CPU type
/// boundaries or be in offsets which are readable without causing memory access faults.
using Message = cetl::pf20::span<const std::uint8_t>;

}  // namespace libcyphal

#endif  // LIBCYPHAL_TRANSPORT_MESSAGE_HPP_INCLUDED
