/// @copyright
/// Copyright (C) OpenCyphal Development Team  <opencyphal.org>
/// Copyright Amazon.com Inc. or its affiliates.
/// SPDX-License-Identifier: MIT

#ifndef LIBCYPHAL_TRANSPORT_UDP_SESSIONS_TREE_HPP
#define LIBCYPHAL_TRANSPORT_UDP_SESSIONS_TREE_HPP

#include "libcyphal/common/cavl/cavl.hpp"
#include "libcyphal/transport/types.hpp"

#include <cetl/pf17/cetlpf.hpp>

#include <cstdint>
#include <functional>

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

template <typename Node>
class SessionsTree final
{
public:
    using NodeRef = typename Node::ReferenceWrapper;

    explicit SessionsTree(cetl::pmr::memory_resource& mr)
        : allocator_{&mr}
    {
    }

    SessionsTree(const SessionsTree&)                = delete;
    SessionsTree(SessionsTree&&) noexcept            = delete;
    SessionsTree& operator=(const SessionsTree&)     = delete;
    SessionsTree& operator=(SessionsTree&&) noexcept = delete;

    ~SessionsTree()
    {
        // TODO: Implement tree cleanup
    }

    CETL_NODISCARD Expected<NodeRef, AnyError> ensureNewNodeFor(const PortId port_id)
    {
        auto const node_existing = nodes_.search([port_id](const Node& node) { return node.compareWith(port_id); },
                                                 [port_id, this]() { return constructNewNode(port_id); });
        if (std::get<1>(node_existing))
        {
            return AlreadyExistsError{};
        }
        auto* node = std::get<0>(node_existing);
        if (nullptr == node)
        {
            return MemoryError{};
        }

        return *node;
    }

    void removeNodeFor(const PortId port_id)
    {
        removeAndDestroyNode(nodes_.search([port_id](const Node& node) { return node.compareWith(port_id); }));
    }

private:
    Node* constructNewNode(const PortId port_id)
    {
        Node* const node = allocator_.allocate(1);
        if (nullptr != node)
        {
            allocator_.construct(node, port_id);
        }
        return node;
    }

    void removeAndDestroyNode(Node* node)
    {
        if (nullptr != node)
        {
            nodes_.remove(node);
            allocator_.destroy(node);
            allocator_.deallocate(node, 1);
        }
    }

    cavl::Tree<Node>                      nodes_;
    libcyphal::detail::PmrAllocator<Node> allocator_;

};  // SessionsTree

struct RxSessionTreeNode
{
    class Message final : public cavl::Node<Message>
    {
    public:
        using ReferenceWrapper = std::reference_wrapper<Message>;

        explicit Message(const PortId port_id)
            : port_id_{port_id}
        {
        }

        std::int32_t compareWith(const PortId port_id) const
        {
            return static_cast<std::int32_t>(port_id_) - static_cast<std::int32_t>(port_id);
        }

    private:
        // MARK: Data members:

        PortId port_id_{};

    };  // Message

};  // RxSessionTreeNode

}  // namespace detail
}  // namespace udp
}  // namespace transport
}  // namespace libcyphal

#endif  // LIBCYPHAL_TRANSPORT_UDP_SESSIONS_TREE_HPP
