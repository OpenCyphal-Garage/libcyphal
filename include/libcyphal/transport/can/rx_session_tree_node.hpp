/// @copyright
/// Copyright (C) OpenCyphal Development Team  <opencyphal.org>
/// Copyright Amazon.com Inc. or its affiliates.
/// SPDX-License-Identifier: MIT

#ifndef LIBCYPHAL_TRANSPORT_CAN_RX_SESSION_TREE_NODE_HPP_INCLUDED
#define LIBCYPHAL_TRANSPORT_CAN_RX_SESSION_TREE_NODE_HPP_INCLUDED

#include "libcyphal/transport/session_tree.hpp"
#include "libcyphal/transport/svc_sessions.hpp"
#include "libcyphal/transport/types.hpp"

#include <cetl/cetl.hpp>

#include <cstdint>
#include <tuple>

namespace libcyphal
{
namespace transport
{
namespace can
{

/// Internal implementation details of the UDP transport.
/// Not supposed to be used directly by the users of the library.
///
namespace detail
{

class IRxSessionDelegate;

struct RxSessionTreeNode
{
    /// @brief Represents a service response RX session node.
    ///
    class Response final : public transport::detail::SessionTree<Response>::NodeBase
    {
    public:
        explicit Response(const ResponseRxParams& params, const std::tuple<>&)
            : service_id_{params.service_id}
            , server_node_id{params.server_node_id}
            , delegate_{nullptr}
        {
        }

        CETL_NODISCARD std::int32_t compareByParams(const ResponseRxParams& params) const
        {
            if (service_id_ != params.service_id)
            {
                return static_cast<std::int32_t>(service_id_) - static_cast<std::int32_t>(params.service_id);
            }
            return static_cast<std::int32_t>(server_node_id) - static_cast<std::int32_t>(params.server_node_id);
        }

        CETL_NODISCARD IRxSessionDelegate*& delegate() noexcept
        {
            return delegate_;
        }

    private:
        // MARK: Data members:

        const PortId        service_id_;
        const NodeId        server_node_id;
        IRxSessionDelegate* delegate_;

    };  // Response

};  // RxSessionTreeNode

}  // namespace detail
}  // namespace can
}  // namespace transport
}  // namespace libcyphal

#endif  // LIBCYPHAL_TRANSPORT_CAN_RX_SESSION_TREE_NODE_HPP_INCLUDED
