/// @copyright Copyright Amazon.com Inc. and its affiliates. All Rights Reserved.
/// @file
/// A high level POSIX API interface for exposing application facing APIs and wrappers

#ifndef LIBCYPHAL_CAN_HPP_INCLUDED
#define LIBCYPHAL_CAN_HPP_INCLUDED

#include "posix/libcyphal/wrappers/can/broadcaster.hpp"
#include "posix/libcyphal/wrappers/can/subscriber.hpp"

namespace libcyphal
{

using CANBroadcaster = ::libcyphal::wrappers::can::Broadcaster;
using CANSubscriber  = ::libcyphal::wrappers::can::Subscriber;

}  // namespace libcyphal

#endif  // LIBCYPHAL_CAN_HPP_INCLUDED
