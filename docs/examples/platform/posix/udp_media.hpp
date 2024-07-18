/// @copyright
/// Copyright (C) OpenCyphal Development Team  <opencyphal.org>
/// Copyright Amazon.com Inc. or its affiliates.
/// SPDX-License-Identifier: MIT
///

#ifndef EXAMPLE_PLATFORM_POSIX_UPD_MEDIA_HPP_INCLUDED
#define EXAMPLE_PLATFORM_POSIX_UPD_MEDIA_HPP_INCLUDED

#include "udp_sockets.hpp"

#include <cetl/pf17/cetlpf.hpp>
#include <libcyphal/transport/errors.hpp>
#include <libcyphal/transport/udp/media.hpp>
#include <libcyphal/transport/udp/tx_rx_sockets.hpp>

namespace example
{
namespace platform
{
namespace posix
{

class UdpMedia final : public libcyphal::transport::udp::IMedia
{
public:
    explicit UdpMedia(cetl::pmr::memory_resource& memory)
        : memory_{memory}
    {
    }

private:
    // MARK: - IMedia

    MakeTxSocketResult::Type makeTxSocket() override
    {
        return UdpTxSocket::make(memory_, "127.0.0.1");
    }

    MakeRxSocketResult::Type makeRxSocket(const libcyphal::transport::udp::IpEndpoint& multicast_endpoint) override
    {
        return UdpRxSocket::make(memory_, "127.0.0.1", multicast_endpoint);
    }

    // MARK: Data members:

    cetl::pmr::memory_resource& memory_;

};  // UdpMedia

}  // namespace posix
}  // namespace platform
}  // namespace example

#endif  // EXAMPLE_PLATFORM_POSIX_UPD_MEDIA_HPP_INCLUDED
