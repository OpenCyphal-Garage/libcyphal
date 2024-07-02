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
#include <string>
#include <utility>

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
        struct Spec : libcyphal::detail::UniquePtrSpec<ITxSocket, ReferenceWrapper>
        {};

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

        TxSocketMock& tx_socket_mock()
        {
            return tx_socket_mock_;
        }

        // MARK: ITxSocket

        std::size_t getMtu() const noexcept override
        {
            return tx_socket_mock_.getMtu();
        }

        Expected<bool, ITxSocket::SendFailure> send(const TimePoint        deadline,
                                                    const IpEndpoint       multicast_endpoint,
                                                    const std::uint8_t     dscp,
                                                    const PayloadFragments payload_fragments) override
        {
            return tx_socket_mock_.send(deadline, multicast_endpoint, dscp, payload_fragments);
        }

    private:
        TxSocketMock& tx_socket_mock_;

    };  // ReferenceWrapper

    explicit TxSocketMock(std::string name)
        : name_{std::move(name)} {}

    virtual ~TxSocketMock()                          = default;
    TxSocketMock(const TxSocketMock&)                = delete;
    TxSocketMock(TxSocketMock&&) noexcept            = delete;
    TxSocketMock& operator=(const TxSocketMock&)     = delete;
    TxSocketMock& operator=(TxSocketMock&&) noexcept = delete;

    std::string getMockName() const
    {
        return name_;
    }

    std::size_t getBaseMtu() const noexcept
    {
        return ITxSocket::getMtu();
    }

    // MARK: ITxSocket

    // NOLINTNEXTLINE(bugprone-exception-escape)
    MOCK_METHOD(std::size_t, getMtu, (), (const, noexcept, override));

    MOCK_METHOD((Expected<bool, ITxSocket::SendFailure>),
                send,
                (const TimePoint        deadline,
                 const IpEndpoint       multicast_endpoint,
                 const std::uint8_t     dscp,
                 const PayloadFragments payload_fragments),
                (override));

private:
    const std::string name_;

};  // TxSocketMock

// MARK: -

class RxSocketMock : public IRxSocket
{
public:
    struct ReferenceWrapper : IRxSocket
    {
        struct Spec : libcyphal::detail::UniquePtrSpec<IRxSocket, ReferenceWrapper>
        {};

        explicit ReferenceWrapper(RxSocketMock& rx_socket_mock)
            : rx_socket_mock_{rx_socket_mock}
        {
        }
        ReferenceWrapper(const ReferenceWrapper& other)
            : rx_socket_mock_{other.rx_socket_mock_}
        {
        }

        virtual ~ReferenceWrapper()                              = default;
        ReferenceWrapper(ReferenceWrapper&&) noexcept            = delete;
        ReferenceWrapper& operator=(const ReferenceWrapper&)     = delete;
        ReferenceWrapper& operator=(ReferenceWrapper&&) noexcept = delete;

        RxSocketMock& rx_socket_mock()
        {
            return rx_socket_mock_;
        }

        // MARK: IRxSocket

        Expected<cetl::optional<ReceiveSuccess>, ReceiveFailure> receive() override
        {
            return rx_socket_mock_.receive();
        }

    private:
        RxSocketMock& rx_socket_mock_;

    };  // ReferenceWrapper

    explicit RxSocketMock(std::string name)
        : name_{std::move(name)} {}

    virtual ~RxSocketMock()                          = default;
    RxSocketMock(const RxSocketMock&)                = delete;
    RxSocketMock(RxSocketMock&&) noexcept            = delete;
    RxSocketMock& operator=(const RxSocketMock&)     = delete;
    RxSocketMock& operator=(RxSocketMock&&) noexcept = delete;

    std::string getMockName() const
    {
        return name_;
    }

    IpEndpoint getEndpoint() const
    {
        return endpoint_;
    }

    void setEndpoint(const IpEndpoint& endpoint)
    {
        endpoint_ = endpoint;
    }

    // MARK: IRxSocket

    MOCK_METHOD((Expected<cetl::optional<ReceiveSuccess>, ReceiveFailure>), receive, (), (override));

private:
    const std::string name_;
    IpEndpoint        endpoint_{};

};  // RxSocketMock

}  // namespace udp
}  // namespace transport
}  // namespace libcyphal

#endif  // LIBCYPHAL_TRANSPORT_UDP_TX_RX_SOCKETS_MOCK_HPP_INCLUDED
