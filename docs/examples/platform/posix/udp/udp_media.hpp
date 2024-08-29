/// @copyright
/// Copyright (C) OpenCyphal Development Team  <opencyphal.org>
/// Copyright Amazon.com Inc. or its affiliates.
/// SPDX-License-Identifier: MIT
///

#ifndef EXAMPLE_PLATFORM_POSIX_UPD_MEDIA_HPP_INCLUDED
#define EXAMPLE_PLATFORM_POSIX_UPD_MEDIA_HPP_INCLUDED

#include "udp_sockets.hpp"

#include <cetl/pf17/cetlpf.hpp>
#include <libcyphal/executor.hpp>
#include <libcyphal/transport/udp/media.hpp>
#include <libcyphal/transport/udp/tx_rx_sockets.hpp>

#include <string>
#include <utility>
#include <vector>

namespace example
{
namespace platform
{
namespace posix
{

class UdpMedia final : public libcyphal::transport::udp::IMedia
{
public:
    struct Collection
    {
        Collection() = default;

        void make(cetl::pmr::memory_resource& memory,
                  libcyphal::IExecutor&       executor,
                  std::vector<std::string>&   iface_addresses)
        {
            reset();

            for (const auto& iface_address : iface_addresses)
            {
                media_vector_.emplace_back(memory, executor, iface_address);
            }
            for (auto& media : media_vector_)
            {
                media_ifaces_.push_back(&media);
            }
        }

        cetl::span<IMedia*> span()
        {
            return {media_ifaces_.data(), media_ifaces_.size()};
        }

        void reset()
        {
            media_vector_.clear();
            media_ifaces_.clear();
        }

    private:
        std::vector<UdpMedia> media_vector_;
        std::vector<IMedia*>  media_ifaces_;
    };

    UdpMedia(cetl::pmr::memory_resource& memory, libcyphal::IExecutor& executor, std::string iface_address)
        : memory_{memory}
        , executor_{executor}
        , iface_address_{std::move(iface_address)}
    {
    }
    ~UdpMedia() = default;

    UdpMedia(const UdpMedia&)                = delete;
    UdpMedia& operator=(const UdpMedia&)     = delete;
    UdpMedia* operator=(UdpMedia&&) noexcept = delete;

    UdpMedia(UdpMedia&& other) noexcept
        : memory_{other.memory_}
        , executor_{other.executor_}
        , iface_address_{other.iface_address_}
    {
    }

private:
    // MARK: - IMedia

    MakeTxSocketResult::Type makeTxSocket() override
    {
        return UdpTxSocket::make(memory_, executor_, iface_address_);
    }

    MakeRxSocketResult::Type makeRxSocket(const libcyphal::transport::udp::IpEndpoint& multicast_endpoint) override
    {
        return UdpRxSocket::make(memory_, executor_, iface_address_, multicast_endpoint);
    }

    // MARK: Data members:

    cetl::pmr::memory_resource& memory_;
    libcyphal::IExecutor&       executor_;
    std::string                 iface_address_;

};  // UdpMedia

}  // namespace posix
}  // namespace platform
}  // namespace example

#endif  // EXAMPLE_PLATFORM_POSIX_UPD_MEDIA_HPP_INCLUDED
