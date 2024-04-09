/// @copyright
/// Copyright (C) OpenCyphal Development Team  <opencyphal.org>
/// Copyright Amazon.com Inc. or its affiliates.
/// SPDX-License-Identifier: MIT

#ifndef LIBCYPHAL_TRANSPORT_CAN_CAN_TRANSPORT_HPP_INCLUDED
#define LIBCYPHAL_TRANSPORT_CAN_CAN_TRANSPORT_HPP_INCLUDED

#include "media.hpp"
#include "libcyphal/transport/transport.hpp"

namespace libcyphal
{
namespace transport
{
namespace can
{

class CanTransport final : public ITransport
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

    CETL_NODISCARD Expected<UniquePtr<session::IMessageRxSession>, AnyError> makeMessageRxSession(
        const session::MessageRxParams&) override
    {
        return ArgumentError{};
    }
    CETL_NODISCARD Expected<UniquePtr<session::IMessageTxSession>, AnyError> makeMessageTxSession(
        const session::MessageTxParams&) override
    {
        return ArgumentError{};
    }
    CETL_NODISCARD Expected<UniquePtr<session::IRequestRxSession>, AnyError> makeRequestRxSession(
        const session::RequestRxParams&) override
    {
        return ArgumentError{};
    }
    CETL_NODISCARD Expected<UniquePtr<session::IRequestTxSession>, AnyError> makeRequestTxSession(
        const session::RequestTxParams&) override
    {
        return ArgumentError{};
    }
    CETL_NODISCARD Expected<UniquePtr<session::IResponseRxSession>, AnyError> makeResponseRxSession(
        const session::ResponseRxParams&) override
    {
        return ArgumentError{};
    }
    CETL_NODISCARD Expected<UniquePtr<session::IResponseTxSession>, AnyError> makeResponseTxSession(
        const session::ResponseTxParams&) override
    {
        return ArgumentError{};
    }

    // IRunnable

    void run(const TimePoint) override {}

    // Factory

    CETL_NODISCARD static inline Expected<CanTransport, FactoryError> make(
        cetl::pmr::memory_resource&,  // memory,
        //    IMultiplexer&                      mux,  // only if the second execution strategy proposal is chosen
        //    const cetl::span<IMedia>,
        const cetl::optional<NodeId>  // local_node_id
    )
    {
        return ArgumentError{};
    }

};  // CanTransport

}  // namespace can
}  // namespace transport
}  // namespace libcyphal

#endif  // LIBCYPHAL_TRANSPORT_CAN_CAN_TRANSPORT_HPP_INCLUDED
