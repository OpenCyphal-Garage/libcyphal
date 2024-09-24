/// @copyright
/// Copyright (C) OpenCyphal Development Team  <opencyphal.org>
/// Copyright Amazon.com Inc. or its affiliates.
/// SPDX-License-Identifier: MIT

#ifndef LIBCYPHAL_APPLICATION_NODE_HPP_INCLUDED
#define LIBCYPHAL_APPLICATION_NODE_HPP_INCLUDED

#include "libcyphal/presentation/presentation.hpp"
#include "libcyphal/types.hpp"
#include "node/get_info_provider.hpp"
#include "node/heartbeat_producer.hpp"

#include <cetl/pf17/cetlpf.hpp>

#include <utility>

namespace libcyphal
{
namespace application
{

/// @brief Defines application layer Node class.
///
class Node final
{
public:
    /// @brief Defines failure type for node operations.
    ///
    /// The set of possible failures includes presentation layer ones.
    ///
    using MakeFailure = presentation::Presentation::MakeFailure;

    /// @brief Factory method to create a Node instance.
    ///
    /// Only one single instance of the Node class should be created for a given presentation layer instance.
    /// Normally, application has only one such Node (and its presentation & transport instances). But if application
    /// needs to bring up multiple nodes then different presentation and transport instances should be used,
    /// and the latter one should have a different node id assigned eventually (see `ITransport::setLocalNodeId`).
    ///
    /// @param presentation The presentation layer instance. In use to create various node components, such as
    ///                     'Heartbeat' publisher and 'GetInfo' service server.
    /// @return The Node instance or a failure.
    ///
    static Expected<Node, MakeFailure> make(presentation::Presentation& presentation)
    {
        auto maybe_heartbeat_producer = node::HeartbeatProducer::make(presentation);
        if (auto* const failure = cetl::get_if<presentation::Presentation::MakeFailure>(&maybe_heartbeat_producer))
        {
            return std::move(*failure);
        }

        auto maybe_get_info_provider = node::GetInfoProvider::make(presentation);
        if (auto* const failure = cetl::get_if<presentation::Presentation::MakeFailure>(&maybe_get_info_provider))
        {
            return std::move(*failure);
        }

        return Node{cetl::get<node::GetInfoProvider>(std::move(maybe_get_info_provider)),
                    cetl::get<node::HeartbeatProducer>(std::move(maybe_heartbeat_producer))};
    }

    /// @brief Gets reference to the 'GetInfo' provider component.
    ///
    /// Could be used to setup node's information which is returned by the GetInfo server.
    ///
    node::GetInfoProvider& getInfoProvider() noexcept
    {
        return get_info_provider_;
    }

    /// @brief Gets reference to the 'Heartbeat' producer component.
    ///
    /// Could be used to setup the heartbeat update callback.
    ///
    node::HeartbeatProducer& heartbeatProducer() noexcept
    {
        return heartbeat_producer_;
    }

private:
    Node(node::GetInfoProvider&& get_info_provider, node::HeartbeatProducer&& heartbeat_producer) noexcept
        : get_info_provider_{std::move(get_info_provider)}
        , heartbeat_producer_{std::move(heartbeat_producer)}
    {
    }

    // MARK: Data members:

    node::GetInfoProvider   get_info_provider_;
    node::HeartbeatProducer heartbeat_producer_;

};  // Node

}  // namespace application
}  // namespace libcyphal

#endif  // LIBCYPHAL_APPLICATION_NODE_HPP_INCLUDED
