/// @copyright Copyright Amazon.com Inc. and its affiliates. All Rights Reserved.
/// @file
/// Commonly used types for a CAN Connection

#ifndef LIBCYPHAL_TRANSPORT_CAN_TYPES_HPP_INCLUDED
#define LIBCYPHAL_TRANSPORT_CAN_TYPES_HPP_INCLUDED

#include <cstdint>

namespace libcyphal
{
namespace transport
{
namespace can
{

using Socket                               = std::int16_t;
constexpr Socket       ClosedSocket        = -1;
constexpr std::int16_t SocketFunctionError = -1;

}  // namespace can
}  // namespace transport
}  // namespace libcyphal

#endif  // LIBCYPHAL_TRANSPORT_CAN_TYPES_HPP_INCLUDED
