/// @copyright
/// Copyright (C) OpenCyphal Development Team  <opencyphal.org>
/// Copyright Amazon.com Inc. or its affiliates.
/// SPDX-License-Identifier: MIT

#ifndef LIBCYPHAL_TRANSPORT_SESSION_TREE_HPP_INCLUDED
#define LIBCYPHAL_TRANSPORT_SESSION_TREE_HPP_INCLUDED

#include "errors.hpp"
#include "libcyphal/common/cavl/cavl.hpp"
#include "libcyphal/types.hpp"

#include <cetl/cetl.hpp>
#include <cetl/pf17/cetlpf.hpp>

#include <array>
#include <functional>
#include <tuple>

namespace libcyphal
{
namespace transport
{

/// Internal implementation details of the UDP transport.
/// Not supposed to be used directly by the users of the library.
///
namespace detail
{

/// @brief Defines a tree of sessions for the UDP transport.
///
template <typename Node>
class SessionTree final
{
public:
    /// Base class for the session tree node.
    ///
    class NodeBase : public common::cavl::Node<Node>
    {
    public:
        using common::cavl::Node<Node>::getChildNode;
        using RefWrapper = std::reference_wrapper<Node>;

        NodeBase(const NodeBase&)                = delete;
        NodeBase(NodeBase&&) noexcept            = delete;
        NodeBase& operator=(const NodeBase&)     = delete;
        NodeBase& operator=(NodeBase&&) noexcept = delete;

    protected:
        NodeBase()  = default;
        ~NodeBase() = default;

    };  // NodeBase

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
    CETL_NODISCARD auto ensureNodeFor(const Params& params,
                                      Args&&... args) -> Expected<typename NodeBase::RefWrapper, AnyFailure>
    {
        auto args_tuple = std::make_tuple(std::forward<Args>(args)...);

        const auto node_existing = nodes_.search(
            [&params](const Node& node) {  // predicate
                //
                return node.compareByParams(params);
            },
            [this, &params, args_tuple_ = std::move(args_tuple)] {
                //
                return constructNewNode(params, std::move(args_tuple_));
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
    void removeNodeFor(const Params& params) noexcept
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

}  // namespace detail
}  // namespace transport
}  // namespace libcyphal

#endif  // LIBCYPHAL_TRANSPORT_SESSION_TREE_HPP_INCLUDED
