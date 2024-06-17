/// @copyright
/// Copyright (C) OpenCyphal Development Team  <opencyphal.org>
/// Copyright Amazon.com Inc. or its affiliates.
/// SPDX-License-Identifier: MIT

#ifndef LIBCYPHAL_TRANSPORT_UDP_TX_RX_SOCKETS_MOCK_HPP_INCLUDED
#define LIBCYPHAL_TRANSPORT_UDP_TX_RX_SOCKETS_MOCK_HPP_INCLUDED

#include <cetl/pf17/cetlpf.hpp>
#include <libcyphal/transport/errors.hpp>
#include <libcyphal/transport/types.hpp>
#include <libcyphal/transport/udp/tx_rx_sockets.hpp>
#include <libcyphal/types.hpp>

#include <gmock/gmock.h>

#include <cstddef>
#include <cstdint>

namespace libcyphal
{
namespace transport
{
namespace udp
{

class TxSocketMock : public ITxSocket
{
public:
    struct ReferenceWrapper : ITxSocket
    {
        struct Spec
        {
            using Interface = ITxSocket;
            using Concrete  = ReferenceWrapper;
        };

        explicit ReferenceWrapper(TxSocketMock& tx_socket_mock)
            : tx_socket_mock_{tx_socket_mock}
        {
        }
        ReferenceWrapper(const ReferenceWrapper& other)
            : tx_socket_mock_{other.tx_socket_mock_}
        {
        }

        virtual ~ReferenceWrapper()                              = default;
        ReferenceWrapper(ReferenceWrapper&&) noexcept            = delete;
        ReferenceWrapper& operator=(const ReferenceWrapper&)     = delete;
        ReferenceWrapper& operator=(ReferenceWrapper&&) noexcept = delete;

        // MARK: ITxSocket

        std::size_t getMtu() const noexcept override
        {
            return tx_socket_mock_.getMtu();
        }

        libcyphal::Expected<bool, cetl::variant<PlatformError, ArgumentError>> send(
            const IpEndpoint       multicast_endpoint,
            const std::uint8_t     dscp,
            const PayloadFragments payload_fragments) override
        {
            return tx_socket_mock_.send(multicast_endpoint, dscp, payload_fragments);
        }

    private:
        TxSocketMock& tx_socket_mock_;

    };  // ReferenceWrapper

    TxSocketMock()          = default;
    virtual ~TxSocketMock() = default;

    TxSocketMock(const TxSocketMock&)                = delete;
    TxSocketMock(TxSocketMock&&) noexcept            = delete;
    TxSocketMock& operator=(const TxSocketMock&)     = delete;
    TxSocketMock& operator=(TxSocketMock&&) noexcept = delete;

    // NOLINTNEXTLINE(bugprone-exception-escape)
    MOCK_METHOD(std::size_t, getMtu, (), (const, noexcept, override));

    MOCK_METHOD((Expected<bool, cetl::variant<PlatformError, ArgumentError>>),
                send,
                (const IpEndpoint       multicast_endpoint,
                 const std::uint8_t     dscp,
                 const PayloadFragments payload_fragments),
                (override));

};  // TxSocketMock

// MARK: -

class RxSocketMock : public IRxSocket
{
public:
    RxSocketMock()          = default;
    virtual ~RxSocketMock() = default;

    RxSocketMock(const RxSocketMock&)                = delete;
    RxSocketMock(RxSocketMock&&) noexcept            = delete;
    RxSocketMock& operator=(const RxSocketMock&)     = delete;
    RxSocketMock& operator=(RxSocketMock&&) noexcept = delete;

};  // RxSocketMock

}  // namespace udp
}  // namespace transport
}  // namespace libcyphal

#endif  // LIBCYPHAL_TRANSPORT_UDP_TX_RX_SOCKETS_MOCK_HPP_INCLUDED
