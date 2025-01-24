/// @copyright
/// Copyright (C) OpenCyphal Development Team  <opencyphal.org>
/// Copyright Amazon.com Inc. or its affiliates.
/// SPDX-License-Identifier: MIT

#ifndef LIBCYPHAL_TRANSPORT_UDP_SESSION_TREE_HPP_INCLUDED
#define LIBCYPHAL_TRANSPORT_UDP_SESSION_TREE_HPP_INCLUDED

#include "tx_rx_sockets.hpp"

#include "libcyphal/common/cavl/cavl.hpp"
#include "libcyphal/executor.hpp"
#include "libcyphal/transport/errors.hpp"
#include "libcyphal/transport/msg_sessions.hpp"
#include "libcyphal/transport/svc_sessions.hpp"
#include "libcyphal/transport/types.hpp"
#include "libcyphal/types.hpp"

#include <cetl/cetl.hpp>
#include <cetl/pf17/cetlpf.hpp>
#include <udpard.h>

#include <array>
#include <cstdint>
#include <functional>
#include <tuple>

namespace libcyphal
{
namespace transport
{
namespace udp
{

/// Internal implementation details of the UDP transport.
/// Not supposed to be used directly by the users of the library.
///
namespace detail
{

template <typename SocketInterface>
struct SocketState
{
    UniquePtr<SocketInterface> interface;
    IExecutor::Callback::Any   callback;

};  // SocketState

/// @brief Defines a tree of sessions for the UDP transport.
///
template <typename Node>
class SessionTree final
{
public:
    using NodeRef = std::reference_wrapper<Node>;

    explicit SessionTree(cetl::pmr::memory_resource& mr)
        : allocator_{&mr}
    {
    }

    SessionTree(const SessionTree&)                = delete;
    SessionTree(SessionTree&&) noexcept            = delete;
    SessionTree& operator=(const SessionTree&)     = delete;
    SessionTree& operator=(SessionTree&&) noexcept = delete;

    ~SessionTree()
    {
        nodes_.traversePostOrder([this](auto& node) { destroyNode(node); });
    }

    CETL_NODISCARD bool isEmpty() const noexcept
    {
        return nodes_.empty();
    }

    template <bool ShouldBeNew = false, typename Params, typename... Args>
    CETL_NODISCARD Expected<NodeRef, AnyFailure> ensureNodeFor(const Params& params, Args&&... args)
    {
        auto args_tuple = std::make_tuple(std::forward<Args>(args)...);

        const auto node_existing = nodes_.search(
            [&params](const Node& node) {  // predicate
                //
                return node.compareByParams(params);
            },
            [this, &params, args_tuple = std::move(args_tuple)] {
                //
                return constructNewNode(params, std::move(args_tuple));
            });

        auto* const node = std::get<0>(node_existing);
        if (nullptr == node)
        {
            return MemoryError{};
        }
        if (ShouldBeNew && std::get<1>(node_existing))
        {
            return AlreadyExistsError{};
        }

        return *node;
    }

    template <typename Params>
    CETL_NODISCARD Node* tryFindNodeFor(const Params& params)
    {
        return nodes_.search([&params](const Node& node) {  // predicate
            //
            return node.compareByParams(params);
        });
    }

    template <typename Params>
    void removeNodeFor(const Params& params)
    {
        removeAndDestroyNode(tryFindNodeFor(params));
    }

    template <typename Action>
    CETL_NODISCARD cetl::optional<AnyFailure> forEachNode(Action&& action)
    {
        return nodes_.traverse(std::forward<Action>(action));
    }

private:
    template <typename Params, typename ArgsTuple>
    CETL_NODISCARD Node* constructNewNode(const Params& params, ArgsTuple&& args_tuple)
    {
        Node* const node = allocator_.allocate(1);
        if (nullptr != node)
        {
            allocator_.construct(node, params, std::forward<ArgsTuple>(args_tuple));
        }
        return node;
    }

    void removeAndDestroyNode(Node* node)
    {
        if (nullptr != node)
        {
            nodes_.remove(node);
            destroyNode(*node);
        }
    }

    void destroyNode(Node& node)
    {
        // No Sonar cpp:M23_329 b/c we do our own low-level PMR management here.
        node.~Node();  // NOSONAR cpp:M23_329
        allocator_.deallocate(&node, 1);
    }

    // MARK: Data members:

    common::cavl::Tree<Node>              nodes_;
    libcyphal::detail::PmrAllocator<Node> allocator_;

};  // SessionTree

// MARK: -

class IRxSessionDelegate;
class IMsgRxSessionDelegate;

struct RxSessionTreeNode
{
    template <typename Derived>
    class Base : public common::cavl::Node<Derived>
    {
    public:
        using common::cavl::Node<Derived>::getChildNode;
        using ReferenceWrapper = std::reference_wrapper<Derived>;

    };  // Base

    /// @brief Represents a message RX session node.
    ///
    class Message final : public Base<Message>
    {
    public:
        explicit Message(const MessageRxParams& params, const std::tuple<>&)
            : subject_id_{params.subject_id}
        {
        }

        CETL_NODISCARD std::int32_t compareByParams(const MessageRxParams& params) const
        {
            return static_cast<std::int32_t>(subject_id_) - static_cast<std::int32_t>(params.subject_id);
        }

        CETL_NODISCARD IMsgRxSessionDelegate*& delegate() noexcept
        {
            return delegate_;
        }

        CETL_NODISCARD SocketState<IRxSocket>& socketState(const std::uint8_t media_index) noexcept
        {
            CETL_DEBUG_ASSERT(media_index < socket_states_.size(), "");

            // No lint b/c at transport constructor we made sure that the number of media interfaces is bound.
            return socket_states_[media_index];  // NOLINT(cppcoreguidelines-pro-bounds-constant-array-index)
        }

    private:
        // MARK: Data members:

        const PortId                                                           subject_id_;
        IMsgRxSessionDelegate*                                                 delegate_{nullptr};
        std::array<SocketState<IRxSocket>, UDPARD_NETWORK_INTERFACE_COUNT_MAX> socket_states_;

    };  // Message

    /// @brief Represents a service request RX session node.
    ///
    class Request final : public Base<Request>
    {
    public:
        explicit Request(const RequestRxParams& params, const std::tuple<>&)
            : service_id_{params.service_id}
        {
        }

        CETL_NODISCARD std::int32_t compareByParams(const RequestRxParams& params) const
        {
            return static_cast<std::int32_t>(service_id_) - static_cast<std::int32_t>(params.service_id);
        }

    private:
        // MARK: Data members:

        const PortId service_id_;

    };  // Request

    /// @brief Represents a service response RX session node.
    ///
    class Response final : public Base<Response>
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
}  // namespace udp
}  // namespace transport
}  // namespace libcyphal

#endif  // LIBCYPHAL_TRANSPORT_UDP_SESSION_TREE_HPP_INCLUDED
