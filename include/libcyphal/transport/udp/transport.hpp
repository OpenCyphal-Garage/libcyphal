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

class IUdpTransport : public ITransport
{};

struct Factory final
{
public:
    Factory() = delete;

    CETL_NODISCARD static inline Expected<UniquePtr<IUdpTransport>, FactoryError> make(
        cetl::pmr::memory_resource&  memory,
        IMultiplexer&                mux,
        const std::array<IMedia*, 3> media,  // TODO: replace with `cetl::span<IMedia*>`
        const cetl::optional<NodeId> local_node_id);

};  // Factory

namespace detail
{

class TransportImpl final : public IUdpTransport
{
public:
    // IUpdTransport

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
        return NotImplementedError{};
    }
    CETL_NODISCARD Expected<UniquePtr<IMessageTxSession>, AnyError> makeMessageTxSession(
        const MessageTxParams&) override
    {
        return NotImplementedError{};
    }
    CETL_NODISCARD Expected<UniquePtr<IRequestRxSession>, AnyError> makeRequestRxSession(
        const RequestRxParams&) override
    {
        return NotImplementedError{};
    }
    CETL_NODISCARD Expected<UniquePtr<IRequestTxSession>, AnyError> makeRequestTxSession(
        const RequestTxParams&) override
    {
        return NotImplementedError{};
    }
    CETL_NODISCARD Expected<UniquePtr<IResponseRxSession>, AnyError> makeResponseRxSession(
        const ResponseRxParams&) override
    {
        return NotImplementedError{};
    }
    CETL_NODISCARD Expected<UniquePtr<IResponseTxSession>, AnyError> makeResponseTxSession(
        const ResponseTxParams&) override
    {
        return NotImplementedError{};
    }

    // IRunnable

    void run(const TimePoint) override {}

};  // TransportImpl

}  // namespace detail

CETL_NODISCARD Expected<UniquePtr<IUdpTransport>, FactoryError> Factory::make(
    cetl::pmr::memory_resource&  memory,
    IMultiplexer&                mux,
    const std::array<IMedia*, 3> media,  // TODO: replace with `cetl::span<IMedia*>`
    const cetl::optional<NodeId> local_node_id)
{
    // TODO: Use these!
    (void) mux;
    (void) media;
    (void) memory;
    (void) local_node_id;

    return NotImplementedError{};
}
}  // namespace udp
}  // namespace transport
}  // namespace libcyphal

#endif  // LIBCYPHAL_TRANSPORT_UDP_TRANSPORT_HPP_INCLUDED
