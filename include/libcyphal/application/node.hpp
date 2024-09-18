/// @copyright
/// Copyright (C) OpenCyphal Development Team  <opencyphal.org>
/// Copyright Amazon.com Inc. or its affiliates.
/// SPDX-License-Identifier: MIT

#ifndef LIBCYPHAL_APPLICATION_NODE_HPP_INCLUDED
#define LIBCYPHAL_APPLICATION_NODE_HPP_INCLUDED

#include "libcyphal/presentation/presentation.hpp"
#include "libcyphal/types.hpp"
#include "node/heartbeat.hpp"

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
        auto maybe_heartbeat = node::Heartbeat::make(presentation);
        if (auto* const failure = cetl::get_if<presentation::Presentation::MakeFailure>(&maybe_heartbeat))
        {
            return std::move(*failure);
        }

        return Node{cetl::get<node::Heartbeat>(std::move(maybe_heartbeat))};
    }

    /// @brief Gets reference to the 'Heartbeat' component.
    ///
    /// Could be used to setup the heartbeat update callback.
    ///
    node::Heartbeat& getHeartbeat() noexcept
    {
        return heartbeat_;
    }

private:
    explicit Node(node::Heartbeat&& heartbeat) noexcept
        : heartbeat_{std::move(heartbeat)}
    {
    }

    // MARK: Data members:

    node::Heartbeat heartbeat_;

};  // Node

}  // namespace application
}  // namespace libcyphal

#endif  // LIBCYPHAL_APPLICATION_NODE_HPP_INCLUDED
