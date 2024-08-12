/// @copyright
/// Copyright (C) OpenCyphal Development Team  <opencyphal.org>
/// Copyright Amazon.com Inc. or its affiliates.
/// SPDX-License-Identifier: MIT

#ifndef LIBCYPHAL_TRANSPORT_TRANSPORT_MOCK_HPP_INCLUDED
#define LIBCYPHAL_TRANSPORT_TRANSPORT_MOCK_HPP_INCLUDED

#include <cetl/pf17/cetlpf.hpp>
#include <libcyphal/transport/errors.hpp>
#include <libcyphal/transport/transport.hpp>

#include <gmock/gmock.h>

namespace libcyphal
{
namespace transport
{

class TransportMock : public ITransport
{
public:
    TransportMock()          = default;
    virtual ~TransportMock() = default;

    TransportMock(const TransportMock&)                = delete;
    TransportMock(TransportMock&&) noexcept            = delete;
    TransportMock& operator=(const TransportMock&)     = delete;
    TransportMock& operator=(TransportMock&&) noexcept = delete;

    MOCK_METHOD(ProtocolParams, getProtocolParams, (), (const, noexcept, override));
    MOCK_METHOD(cetl::optional<NodeId>, getLocalNodeId, (), (const, noexcept, override));
    MOCK_METHOD(cetl::optional<ArgumentError>, setLocalNodeId, (const NodeId), (noexcept, override));
    MOCK_METHOD((Expected<UniquePtr<IMessageRxSession>, AnyFailure>),
                makeMessageRxSession,
                (const MessageRxParams&),
                (override));
    MOCK_METHOD((Expected<UniquePtr<IMessageTxSession>, AnyFailure>),
                makeMessageTxSession,
                (const MessageTxParams& params),
                (override));
    MOCK_METHOD((Expected<UniquePtr<IRequestRxSession>, AnyFailure>),
                makeRequestRxSession,
                (const RequestRxParams& params),
                (override));
    MOCK_METHOD((Expected<UniquePtr<IRequestTxSession>, AnyFailure>),
                makeRequestTxSession,
                (const RequestTxParams& params),
                (override));
    MOCK_METHOD((Expected<UniquePtr<IResponseRxSession>, AnyFailure>),
                makeResponseRxSession,
                (const ResponseRxParams& params),
                (override));
    MOCK_METHOD((Expected<UniquePtr<IResponseTxSession>, AnyFailure>),
                makeResponseTxSession,
                (const ResponseTxParams& params),
                (override));

};  // TransportMock

}  // namespace transport
}  // namespace libcyphal

#endif  // LIBCYPHAL_TRANSPORT_TRANSPORT_MOCK_HPP_INCLUDED
