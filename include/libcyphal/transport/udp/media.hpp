/// @copyright
/// Copyright (C) OpenCyphal Development Team  <opencyphal.org>
/// Copyright Amazon.com Inc. or its affiliates.
/// SPDX-License-Identifier: MIT

#ifndef LIBCYPHAL_TRANSPORT_UDP_MEDIA_HPP_INCLUDED
#define LIBCYPHAL_TRANSPORT_UDP_MEDIA_HPP_INCLUDED

#include "libcyphal/transport/errors.hpp"
#include "libcyphal/types.hpp"
#include "tx_rx_sockets.hpp"

#include <cetl/pf17/cetlpf.hpp>

#include <cstddef>

namespace libcyphal
{
namespace transport
{
namespace udp
{

/// @brief Defines interface to a custom UDP media implementation.
///
/// Implementation is supposed to be provided by an user of the library.
///
class IMedia
{
public:
    IMedia(const IMedia&)                = delete;
    IMedia(IMedia&&) noexcept            = delete;
    IMedia& operator=(const IMedia&)     = delete;
    IMedia& operator=(IMedia&&) noexcept = delete;

    /// @brief Get the maximum transmission unit (MTU) of the UDP media.
    ///
    /// This value may change arbitrarily at runtime. The transport implementation will query it before every
    /// transmission on the port. This value has no effect on the reception pipeline as it can accept arbitrary MTU.
    ///
    virtual std::size_t getMtu() const noexcept = 0;

    /// Constructs a new TX socket bound to this media.
    ///
    virtual Expected<UniquePtr<ITxSocket>, cetl::variant<MemoryError, PlatformError>> makeTxSocket() = 0;

    /// Constructs a new RX socket bound to the specified multicast group endpoint.
    ///
    virtual Expected<UniquePtr<IRxSocket>, cetl::variant<MemoryError, PlatformError, ArgumentError>> makeRxSocket(
        const IpEndpoint& multicast_endpoint) = 0;

protected:
    IMedia()  = default;
    ~IMedia() = default;

};  // IMedia

}  // namespace udp
}  // namespace transport
}  // namespace libcyphal

#endif  // LIBCYPHAL_TRANSPORT_UDP_MEDIA_HPP_INCLUDED
