/// @copyright
/// Copyright (C) OpenCyphal Development Team  <opencyphal.org>
/// Copyright Amazon.com Inc. or its affiliates.
/// SPDX-License-Identifier: MIT

#ifndef LIBCYPHAL_TRANSPORT_UDP_TRANSPORT_HPP_INCLUDED
#define LIBCYPHAL_TRANSPORT_UDP_TRANSPORT_HPP_INCLUDED

#include "media.hpp"

#include "libcyphal/transport/errors.hpp"
#include "libcyphal/transport/msg_sessions.hpp"
#include "libcyphal/transport/multiplexer.hpp"
#include "libcyphal/transport/svc_sessions.hpp"
#include "libcyphal/transport/transport.hpp"
#include "libcyphal/transport/types.hpp"
#include "libcyphal/types.hpp"

#include <cetl/cetl.hpp>
#include <cetl/pf17/cetlpf.hpp>
#include <cetl/pf20/cetlpf.hpp>
#include <udpard.h>

namespace libcyphal
{
namespace transport
{
namespace udp
{

class IUdpTransport : public ITransport
{
public:
    IUdpTransport(const IUdpTransport&)                = delete;
    IUdpTransport(IUdpTransport&&) noexcept            = delete;
    IUdpTransport& operator=(const IUdpTransport&)     = delete;
    IUdpTransport& operator=(IUdpTransport&&) noexcept = delete;

protected:
    IUdpTransport()  = default;
    ~IUdpTransport() = default;
};

/// Internal implementation details of the UDP transport.
/// Not supposed to be used directly by the users of the library.
///
namespace detail
{

class TransportImpl final : public IUdpTransport
{
    /// @brief Defines specification for making interface unique ptr.
    ///
    struct Spec
    {
        using Interface = IUdpTransport;
        using Concrete  = TransportImpl;

        // In use to disable public construction.
        // See https://seanmiddleditch.github.io/enabling-make-unique-with-private-constructors/
        explicit Spec() = default;
    };

public:
    TransportImpl(Spec,
                  cetl::pmr::memory_resource&            memory,
                  IMultiplexer&                          multiplexer,
                  libcyphal::detail::VarArray<IMedia*>&& media_array,  // NOLINT
                  const UdpardNodeID                     udpard_node_id)
    {
        // TODO: Use them!
        (void) memory;
        (void) multiplexer;
        (void) media_array;
        (void) udpard_node_id;
    }

private:
    // MARK: ITransport

    CETL_NODISCARD cetl::optional<NodeId> getLocalNodeId() const noexcept override
    {
        return cetl::nullopt;
    }

    CETL_NODISCARD cetl::optional<ArgumentError> setLocalNodeId(const NodeId node_id) noexcept override
    {
        if (node_id > UDPARD_NODE_ID_MAX)
        {
            return ArgumentError{};
        }

        // TODO: Implement!
        return {};
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

    // MARK: IRunnable

    void run(const TimePoint) override {}

};  // TransportImpl

}  // namespace detail

/// @brief Makes a new UDP transport instance.
///
/// NB! Lifetime of the transport instance must never outlive `memory`, `media` and `multiplexer` instances.
///
/// @param memory Reference to a polymorphic memory resource to use for all allocations.
/// @param multiplexer Interface of the multiplexer to use.
/// @param media Collection of redundant media interfaces to use.
/// @param local_node_id Optional id of the local node. Could be set (once!) later by `setLocalNodeId` call.
/// @return Unique pointer to the new UDP transport instance or an error.
///
inline Expected<UniquePtr<IUdpTransport>, FactoryError> makeTransport(cetl::pmr::memory_resource&  memory,
                                                                      IMultiplexer&                multiplexer,
                                                                      const cetl::span<IMedia*>    media,
                                                                      const cetl::optional<NodeId> local_node_id)
{
    // TODO: Use these!
    (void) multiplexer;
    (void) media;
    (void) memory;
    (void) local_node_id;

    return NotImplementedError{};
}
}  // namespace udp
}  // namespace transport
}  // namespace libcyphal

#endif  // LIBCYPHAL_TRANSPORT_UDP_TRANSPORT_HPP_INCLUDED
