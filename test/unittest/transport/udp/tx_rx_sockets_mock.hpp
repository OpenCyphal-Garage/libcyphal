/// @copyright
/// Copyright (C) OpenCyphal Development Team  <opencyphal.org>
/// Copyright Amazon.com Inc. or its affiliates.
/// SPDX-License-Identifier: MIT

#ifndef LIBCYPHAL_TRANSPORT_UDP_TX_RX_SOCKETS_MOCK_HPP_INCLUDED
#define LIBCYPHAL_TRANSPORT_UDP_TX_RX_SOCKETS_MOCK_HPP_INCLUDED

#include "unique_ptr_reference_wrapper.hpp"

#include <cetl/pf17/cetlpf.hpp>
#include <libcyphal/executor.hpp>
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
    struct RefWrapper final : UniquePtrReferenceWrapper<ITxSocket, TxSocketMock, RefWrapper>
    {
        using UniquePtrReferenceWrapper::UniquePtrReferenceWrapper;

        // MARK: ITxSocket

        std::size_t getMtu() const noexcept override
        {
            return reference().getMtu();
        }
        SendResult::Type send(const TimePoint        deadline,
                              const IpEndpoint       multicast_endpoint,
                              const std::uint8_t     dscp,
                              const PayloadFragments payload_fragments) override
        {
            return reference().send(deadline, multicast_endpoint, dscp, payload_fragments);
        }
        CETL_NODISCARD IExecutor::Callback::Any registerCallback(IExecutor::Callback::Function&& function) override
        {
            return reference().registerCallback(std::move(function));
        }

    };  // RefWrapper

    explicit TxSocketMock(std::string name)
        : name_{std::move(name)}
    {
    }

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

    MOCK_METHOD(ITxSocket::SendResult::Type,
                send,
                (const TimePoint        deadline,
                 const IpEndpoint       multicast_endpoint,
                 const std::uint8_t     dscp,
                 const PayloadFragments payload_fragments),
                (override));

    MOCK_METHOD(IExecutor::Callback::Any, registerCallback, (IExecutor::Callback::Function && function), (override));

    MOCK_METHOD(void, deinit, (), (noexcept));  // NOLINT(*-exception-escape)

private:
    const std::string name_;

};  // TxSocketMock

// MARK: -

class RxSocketMock : public IRxSocket
{
public:
    struct RefWrapper final : UniquePtrReferenceWrapper<IRxSocket, RxSocketMock, RefWrapper>
    {
        using UniquePtrReferenceWrapper::UniquePtrReferenceWrapper;

        // MARK: IRxSocket

        ReceiveResult::Type receive() override
        {
            return reference().receive();
        }
        CETL_NODISCARD IExecutor::Callback::Any registerCallback(IExecutor::Callback::Function&& function) override
        {
            return reference().registerCallback(std::move(function));
        }

    };  // RefWrapper

    explicit RxSocketMock(std::string name)
        : name_{std::move(name)}
    {
    }

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

    MOCK_METHOD(ReceiveResult::Type, receive, (), (override));

    MOCK_METHOD(IExecutor::Callback::Any, registerCallback, (IExecutor::Callback::Function && function), (override));

    MOCK_METHOD(void, deinit, (), (noexcept));  // NOLINT(*-exception-escape)

private:
    const std::string name_;
    IpEndpoint        endpoint_{};

};  // RxSocketMock

}  // namespace udp
}  // namespace transport
}  // namespace libcyphal

#endif  // LIBCYPHAL_TRANSPORT_UDP_TX_RX_SOCKETS_MOCK_HPP_INCLUDED
