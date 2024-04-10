/// @copyright
/// Copyright (C) OpenCyphal Development Team  <opencyphal.org>
/// Copyright Amazon.com Inc. or its affiliates.
/// SPDX-License-Identifier: MIT

#ifndef LIBCYPHAL_TRANSPORT_UDP_TRANSPORT_HPP_INCLUDED
#define LIBCYPHAL_TRANSPORT_UDP_TRANSPORT_HPP_INCLUDED

#include "media.hpp"
#include "libcyphal/transport/transport.hpp"
#include "libcyphal/transport/multiplexer.hpp"

namespace libcyphal
{
namespace transport
{
namespace udp
{

class Transport final : public ITransport
{
public:
    // ITransport

    CETL_NODISCARD cetl::optional<NodeId> getLocalNodeId() const noexcept override
    {
        return cetl::nullopt;
    }
    CETL_NODISCARD ProtocolParams getProtocolParams() const noexcept override
    {
        return ProtocolParams{};
    }

    CETL_NODISCARD Expected<UniquePtr<IMessageRxSession>, AnyError> makeMessageRxSession(
        const MessageRxParams&) override
    {
        return ArgumentError{};
    }
    CETL_NODISCARD Expected<UniquePtr<IMessageTxSession>, AnyError> makeMessageTxSession(
        const MessageTxParams&) override
    {
        return ArgumentError{};
    }
    CETL_NODISCARD Expected<UniquePtr<IRequestRxSession>, AnyError> makeRequestRxSession(
        const RequestRxParams&) override
    {
        return ArgumentError{};
    }
    CETL_NODISCARD Expected<UniquePtr<IRequestTxSession>, AnyError> makeRequestTxSession(
        const RequestTxParams&) override
    {
        return ArgumentError{};
    }
    CETL_NODISCARD Expected<UniquePtr<IResponseRxSession>, AnyError> makeResponseRxSession(
        const ResponseRxParams&) override
    {
        return ArgumentError{};
    }
    CETL_NODISCARD Expected<UniquePtr<IResponseTxSession>, AnyError> makeResponseTxSession(
        const ResponseTxParams&) override
    {
        return ArgumentError{};
    }

    // IRunnable

    void run(const TimePoint) override {}

    // Factory

    CETL_NODISCARD static inline Expected<UniquePtr<Transport>, FactoryError> make(
        cetl::pmr::memory_resource&,
        IMultiplexer&,
        const std::array<IMedia*, 3>,  // TODO: replace with `cetl::span<IMedia*>`
        const cetl::optional<NodeId>)
    {
        return NotImplementedError{};
    }

};  // Transport

}  // namespace udp
}  // namespace transport
}  // namespace libcyphal

#endif  // LIBCYPHAL_TRANSPORT_UDP_TRANSPORT_HPP_INCLUDED
