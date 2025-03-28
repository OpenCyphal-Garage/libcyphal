/// @copyright
/// Copyright (C) OpenCyphal Development Team  <opencyphal.org>
/// Copyright Amazon.com Inc. or its affiliates.
/// SPDX-License-Identifier: MIT

#ifndef LIBCYPHAL_TRANSPORT_UDP_TX_RX_SOCKETS_HPP_INCLUDED
#define LIBCYPHAL_TRANSPORT_UDP_TX_RX_SOCKETS_HPP_INCLUDED

#include "libcyphal/executor.hpp"
#include "libcyphal/transport/errors.hpp"
#include "libcyphal/transport/types.hpp"
#include "libcyphal/types.hpp"

#include <cetl/pf17/cetlpf.hpp>
#include <udpard.h>

#include <cstddef>
#include <cstdint>

namespace libcyphal
{
namespace transport
{
namespace udp
{

/// @brief Defines IP endpoint information in use for multicast transmissions.
///
struct IpEndpoint final
{
    std::uint32_t ip_address;
    std::uint16_t udp_port;

    static IpEndpoint fromUdpardEndpoint(const UdpardUDPIPEndpoint& endpoint)
    {
        return IpEndpoint{endpoint.ip_address, endpoint.udp_port};
    }
};

/// @brief Defines interface to a custom UDP media TX socket implementation.
///
/// Implementation is supposed to be provided by an user of the library.
///
class ITxSocket
{
public:
    ITxSocket(const ITxSocket&)                = delete;
    ITxSocket(ITxSocket&&) noexcept            = delete;
    ITxSocket& operator=(const ITxSocket&)     = delete;
    ITxSocket& operator=(ITxSocket&&) noexcept = delete;

    /// @brief Get the maximum transmission unit (MTU) of the UDP TX socket.
    ///
    /// To guarantee a single frame transfer, the maximum payload size shall be 4 bytes less to accommodate the CRC.
    /// This value may change arbitrarily at runtime. The transport implementation will query it before every
    /// transmission on the socket.
    ///
    virtual std::size_t getMtu() const noexcept
    {
        return DefaultMtu;
    };

    /// The default MTU is derived as:
    /// 1500B Ethernet MTU (RFC 894) - 60B IPv4 max header - 8B UDP Header - 24B Cyphal header.
    ///
    static constexpr std::size_t DefaultMtu = UDPARD_MTU_DEFAULT;

    /// @brief Sends payload fragments to this socket.
    ///
    /// The payload may be fragmented to minimize data copying in the user space,
    /// allowing the implementation to use vectorized I/O (iov).
    ///
    /// @param deadline The deadline for the send operation. Socket implementation should drop the payload
    ///                 if the deadline is exceeded (aka `now > deadline`).
    /// @param multicast_endpoint The multicast endpoint to send the payload to.
    /// @param dscp The Differentiated Services Code Point (DSCP) to set in the IP header.
    /// @param payload_fragments Fragments of the payload to send.
    /// @return `true` if the payload has been accepted successfully, `false` if the socket is not ready for writing.
    ///         In case of failure, an error is returned.
    ///@{
    struct SendResult
    {
        struct Success
        {
            bool is_accepted;
        };
        using Failure = cetl::variant<PlatformError, ArgumentError>;

        using Type = Expected<Success, Failure>;
    };
    virtual SendResult::Type send(const TimePoint        deadline,
                                  const IpEndpoint       multicast_endpoint,
                                  const std::uint8_t     dscp,
                                  const PayloadFragments payload_fragments) = 0;
    ///@}

    /// @brief Registers "ready to send" callback function at a given executor.
    ///
    /// The callback will be called by an executor when this socket will be ready to accept more (MTU-worth) data.
    ///
    /// For example, POSIX socket implementation may pass its OS handle to the executor implementation,
    /// and executor will use `::poll` POSIX api & `POLLOUT` event to schedule this callback for execution.
    ///
    /// @param function The function to be called when TX socket became "ready to send".
    /// @return Type-erased instance of the registered callback.
    ///         Instance must not outlive the executor; otherwise undefined behavior.
    ///
    CETL_NODISCARD virtual IExecutor::Callback::Any registerCallback(IExecutor::Callback::Function&& function) = 0;

protected:
    ITxSocket()  = default;
    ~ITxSocket() = default;

};  // ITxSocket

/// @brief Defines interface to a custom UDP media RX socket implementation.
///
/// Implementation is supposed to be provided by an user of the library.
///
class IRxSocket
{
public:
    IRxSocket(const IRxSocket&)                = delete;
    IRxSocket(IRxSocket&&) noexcept            = delete;
    IRxSocket& operator=(const IRxSocket&)     = delete;
    IRxSocket& operator=(IRxSocket&&) noexcept = delete;

    /// @brief Takes the next payload fragment (aka UDP datagram) from the reception queue unless it's empty.
    ///
    /// @return Description of a received fragment if available; otherwise an empty optional is returned immediately.
    ///         `nodiscard` is used to prevent ignoring the return value, which contains not only possible media error,
    ///         but also important metadata (like `payload_size` field for further parsing of the result payload),
    ///         as well as fragment payload memory which is now owned by the caller (hence `std::unique_ptr`).
    ///@{
    struct ReceiveResult
    {
        struct Metadata
        {
            /// Holds time point when payload was originally received by this RX socket.
            TimePoint timestamp;

            /// Holds smart pointer to payload raw buffer, as well as its size and PMR (inside of the deleter).
            std::unique_ptr<cetl::byte, PmrRawBytesDeleter> payload_ptr;
        };
        using Success = cetl::optional<Metadata>;
        using Failure = cetl::variant<PlatformError, ArgumentError, MemoryError>;

        using Type = Expected<Success, Failure>;
    };
    CETL_NODISCARD virtual ReceiveResult::Type receive() = 0;
    ///@}

    /// @brief Registers "ready to receive" callback function at a given executor.
    ///
    /// The callback will be called by an executor when this socket will be ready to be read (MTU-worth data).
    ///
    /// For example, POSIX socket implementation may pass its OS handle to the executor implementation,
    /// and executor will use `::poll` POSIX api & `POLLIN` event to schedule this callback for execution.
    ///
    /// @param function The function to be called when TX socket became "ready to receive".
    /// @return Type-erased instance of the registered callback.
    ///         Instance must not outlive the executor; otherwise undefined behavior.
    ///
    CETL_NODISCARD virtual IExecutor::Callback::Any registerCallback(IExecutor::Callback::Function&& function) = 0;

protected:
    IRxSocket()  = default;
    ~IRxSocket() = default;

};  // IRxSocket

}  // namespace udp
}  // namespace transport
}  // namespace libcyphal

#endif  // LIBCYPHAL_TRANSPORT_UDP_TX_RX_SOCKETS_HPP_INCLUDED
