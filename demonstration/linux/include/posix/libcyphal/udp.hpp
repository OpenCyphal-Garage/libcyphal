/// @copyright Copyright Amazon.com Inc. and its affiliates. All Rights Reserved.
/// @file
/// A high level POSIX API interface for exposing application facing APIs and wrappers

#ifndef LIBCYPHAL_UDP_HPP_INCLUDED
#define LIBCYPHAL_UDP_HPP_INCLUDED

#include "posix/libcyphal/wrappers/udp/broadcaster.hpp"
#include "posix/libcyphal/wrappers/udp/subscriber.hpp"

namespace libcyphal
{

using UDPBroadcaster = ::libcyphal::wrappers::udp::Broadcaster;
using UDPSubscriber  = ::libcyphal::wrappers::udp::Subscriber;

}  // namespace libcyphal

#endif  // LIBCYPHAL_UDP_HPP_INCLUDED
