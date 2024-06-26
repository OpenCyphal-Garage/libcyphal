/// Source: https://github.com/pavel-kirienko/cavl
///
/// This is a single-header C++14 library providing an implementation of AVL tree suitable for deeply embedded systems.
/// To integrate it into your project, simply copy this file into your source tree. Read the API docs below.
/// The implementation does not use RTTI, exceptions, or dynamic memory.
///
/// See also O1Heap <https://github.com/pavel-kirienko/o1heap> -- a deterministic memory manager for hard-real-time
/// high-integrity embedded systems.
///
/// Copyright (c) 2021 Pavel Kirienko <pavel@uavcan.org>
///
/// Permission is hereby granted, free of charge, to any person obtaining a copy of this software and associated
/// documentation files (the "Software"), to deal in the Software without restriction, including without limitation
/// the rights to use, copy, modify, merge, publish, distribute, sublicense, and/or sell copies of the Software,
/// and to permit persons to whom the Software is furnished to do so, subject to the following conditions:
///
/// The above copyright notice and this permission notice shall be included in all copies or substantial portions of
/// the Software.
///
/// THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE
/// WARRANTIES OF MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE AUTHORS
/// OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR
/// OTHERWISE, ARISING FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.

#pragma once

#include <cstdint>
#include <tuple>
#include <type_traits>

/// If CAVL is used in throughput-critical code, then it is recommended to disable assertion checks as they may
/// be costly in terms of execution time.
#ifndef CAVL_ASSERT
#    if defined(CAVL_NO_ASSERT) && CAVL_NO_ASSERT
#        define CAVL_ASSERT(x) (void) 0
#    else
#        include <cassert>  // NOLINTNEXTLINE function-like macro
#        define CAVL_ASSERT(x) assert(x)
#    endif
#endif

namespace cavl
{
template <typename Derived>
class Tree;

/// The tree node type is to be composed with the user type through CRTP inheritance.
/// For instance, the derived type might be a key-value pair struct defined in the user code.
/// The worst-case complexity of all operations is O(log n), unless specifically noted otherwise.
/// Note that this class has no public members. The user type should re-export them if needed (usually it is not).
/// The size of this type is 4x pointer size (16 bytes on a 32-bit platform).
template <typename Derived>
class Node
{
    // Polyfill for C++17's std::invoke_result_t.
    template <typename F, typename... Args>
    using invoke_result =
#if __cplusplus >= 201703L
        std::invoke_result_t<F, Args...>;
#else
        std::result_of_t<F(Args...)>;
#endif

public:
    /// Helper aliases.
    using TreeType    = Tree<Derived>;
    using DerivedType = Derived;

    // Tree nodes cannot be copied for obvious reasons.
    Node(const Node&)                    = delete;
    auto operator=(const Node&) -> Node& = delete;

    // They can't be moved either, but the reason is less obvious.
    // While we can trivially update the pointers in the adjacent nodes to keep the tree valid,
    // we can't update external references to the tree. This breaks the tree if one attempted to move its root node.
    Node(Node&& other)                    = delete;
    auto operator=(Node&& other) -> Node& = delete;

protected:
    Node()  = default;
    ~Node() = default;

    /// Accessors for advanced tree introspection. Not needed for typical usage.
    auto getParentNode() noexcept -> Derived*
    {
        return down(up);
    }
    auto getParentNode() const noexcept -> const Derived*
    {
        return down(up);
    }
    auto getChildNode(const bool right) noexcept -> Derived*
    {
        return down(lr[right]);
    }
    auto getChildNode(const bool right) const noexcept -> const Derived*
    {
        return down(lr[right]);
    }
    auto getBalanceFactor() const noexcept
    {
        return bf;
    }

    /// Find a node for which the predicate returns zero, or nullptr if there is no such node or the tree is empty.
    /// The predicate is invoked with a single argument which is a constant reference to Derived.
    /// The predicate returns POSITIVE if the search target is GREATER than the provided node, negative if smaller.
    /// The predicate should be noexcept.
    template <typename Pre>
    static auto search(Node* const root, const Pre& predicate) noexcept -> Derived*
    {
        Derived*                         p   = down(root);
        std::tuple<Derived*, bool> const out = search<Pre>(p, predicate, []() -> Derived* { return nullptr; });
        CAVL_ASSERT(p == root);
        return std::get<0>(out);
    }

    /// Same but const.
    template <typename Pre>
    static auto search(const Node* const root, const Pre& predicate) noexcept -> const Derived*
    {
        const Node* out = nullptr;
        const Node* n   = root;
        while (n != nullptr)
        {
            const auto cmp = predicate(*down(n));
            if (0 == cmp)
            {
                out = n;
                break;
            }
            n = n->lr[cmp > 0];
        }
        return down(out);
    }

    /// This is like the regular search function except that if the node is missing, the factory will be invoked
    /// (without arguments) to construct a new one and insert it into the tree immediately.
    /// The root node may be replaced in the process. If this method returns true, the tree is not modified;
    /// otherwise, the factory was (successfully!) invoked and a new node has been inserted into the tree.
    /// The factory does not need to be noexcept (may throw). It may also return nullptr to indicate intentional
    /// refusal to modify the tree, or f.e. in case of out of memory - result will be `(nullptr, true)` tuple.
    template <typename Pre, typename Fac>
    static auto search(Derived*& root, const Pre& predicate, const Fac& factory) -> std::tuple<Derived*, bool>;

    /// Remove the specified node from its tree. The root node may be replaced in the process.
    /// The function has no effect if the node pointer is nullptr.
    /// If the node is not in the tree, the behavior is undefined; it may create cycles in the tree which is deadly.
    /// It is safe to pass the result of search() directly as the second argument:
    ///     Node<T>::remove(root, Node<T>::search(root, search_predicate));
    static void remove(Derived*& root, const Node* const node) noexcept;

    /// This is like the const overload of remove() except that the node pointers are invalidated afterward for safety.
    static void remove(Derived*& root, Node* const node) noexcept
    {
        remove(root, static_cast<const Node*>(node));
        node->unlink();
    }

    /// These methods provide very fast retrieval of min/max values, either const or mutable.
    /// They return nullptr iff the tree is empty.
    static auto min(Node* const root) noexcept -> Derived*
    {
        return extremum(root, false);
    }
    static auto max(Node* const root) noexcept -> Derived*
    {
        return extremum(root, true);
    }
    static auto min(const Node* const root) noexcept -> const Derived*
    {
        return extremum(root, false);
    }
    static auto max(const Node* const root) noexcept -> const Derived*
    {
        return extremum(root, true);
    }

    /// In-order or reverse-in-order traversal of the tree; the visitor is invoked with a reference to each node.
    /// Required stack depth is less than 2*log2(size).
    /// If the return type is non-void, then it shall be default-constructable and convertible to bool; in this case,
    /// traversal will stop when the first true value is returned, which is propagated back to the caller; if none
    /// of the calls returned true or the tree is empty, a default value is constructed and returned.
    /// The tree shall not be modified while traversal is in progress, otherwise bad memory access will likely occur.
    template <typename Vis, typename R = invoke_result<Vis, Derived&>>
    static auto traverse(Derived* const root, const Vis& visitor, const bool reverse = false)  //
        -> std::enable_if_t<!std::is_void<R>::value, R>
    {
        if (Node* const n = root)
        {
            if (auto t = Node::traverse<Vis>(down(n->lr[reverse]), visitor, reverse))  // NOLINT qualified-auto
            {
                return t;
            }
            if (auto t = visitor(*root))  // NOLINT qualified-auto
            {
                return t;
            }
            return Node::traverse<Vis>(down(n->lr[!reverse]), visitor, reverse);
        }
        return R{};
    }
    template <typename Vis>
    static auto traverse(Derived* const root, const Vis& visitor, const bool reverse = false)
        -> std::enable_if_t<std::is_void<invoke_result<Vis, Derived&>>::value>
    {
        if (Node* const n = root)
        {
            Node::traverse<Vis>(down(n->lr[reverse]), visitor, reverse);
            visitor(*root);
            Node::traverse<Vis>(down(n->lr[!reverse]), visitor, reverse);
        }
    }
    template <typename Vis, typename R = invoke_result<Vis, const Derived&>>
    static auto traverse(const Derived* const root, const Vis& visitor, const bool reverse = false)  //
        -> std::enable_if_t<!std::is_void<R>::value, R>
    {
        if (const Node* const n = root)
        {
            if (auto t = Node::traverse<Vis>(down(n->lr[reverse]), visitor, reverse))  // NOLINT qualified-auto
            {
                return t;
            }
            if (auto t = visitor(*root))  // NOLINT qualified-auto
            {
                return t;
            }
            return Node::traverse<Vis>(down(n->lr[!reverse]), visitor, reverse);
        }
        return R{};
    }
    template <typename Vis>
    static auto traverse(const Derived* const root, const Vis& visitor, const bool reverse = false)
        -> std::enable_if_t<std::is_void<invoke_result<Vis, const Derived&>>::value>
    {
        if (const Node* const n = root)
        {
            Node::traverse<Vis>(down(n->lr[reverse]), visitor, reverse);
            visitor(*root);
            Node::traverse<Vis>(down(n->lr[!reverse]), visitor, reverse);
        }
    }

private:
    void rotate(const bool r) noexcept
    {
        CAVL_ASSERT((lr[!r] != nullptr) && ((bf >= -1) && (bf <= +1)));
        Node* const z = lr[!r];
        if (up != nullptr)
        {
            up->lr[up->lr[1] == this] = z;
        }
        z->up  = up;
        up     = z;
        lr[!r] = z->lr[r];
        if (lr[!r] != nullptr)
        {
            lr[!r]->up = this;
        }
        z->lr[r] = this;
    }

    auto adjustBalance(const bool increment) noexcept -> Node*;

    auto retraceOnGrowth() noexcept -> Node*;

    void unlink() noexcept
    {
        up    = nullptr;
        lr[0] = nullptr;
        lr[1] = nullptr;
        bf    = 0;
    }

    static auto extremum(Node* const root, const bool maximum) noexcept -> Derived*
    {
        Node* result = nullptr;
        Node* c      = root;
        while (c != nullptr)
        {
            result = c;
            c      = c->lr[maximum];
        }
        return down(result);
    }
    static auto extremum(const Node* const root, const bool maximum) noexcept -> const Derived*
    {
        const Node* result = nullptr;
        const Node* c      = root;
        while (c != nullptr)
        {
            result = c;
            c      = c->lr[maximum];
        }
        return down(result);
    }

    // This is MISRA-compliant as long as we are not polymorphic. The derived class may be polymorphic though.
    static auto down(Node* x) noexcept -> Derived*
    {
        return static_cast<Derived*>(x);
    }
    static auto down(const Node* x) noexcept -> const Derived*
    {
        return static_cast<const Derived*>(x);
    }

    friend class Tree<Derived>;

    // The binary layout is compatible with the C version.
    Node*       up = nullptr;
    Node*       lr[2]{};
    std::int8_t bf = 0;
};

template <typename Derived>
template <typename Pre, typename Fac>
auto Node<Derived>::search(Derived*& root, const Pre& predicate, const Fac& factory) -> std::tuple<Derived*, bool>
{
    Node* out = nullptr;
    Node* up  = root;
    Node* n   = root;
    bool  r   = false;
    while (n != nullptr)
    {
        const auto cmp = predicate(static_cast<const Derived&>(*n));
        if (0 == cmp)
        {
            out = n;
            break;
        }
        r  = cmp > 0;
        up = n;
        n  = n->lr[r];
        CAVL_ASSERT((nullptr == n) || (n->up == up));
    }
    if (nullptr != out)
    {
        return std::make_tuple(down(out), true);
    }

    out = factory();
    if (nullptr == out)
    {
        return std::make_tuple(nullptr, true);
    }

    if (up != nullptr)
    {
        CAVL_ASSERT(up->lr[r] == nullptr);
        up->lr[r] = out;
    }
    else
    {
        root = down(out);
    }
    out->unlink();
    out->up = up;
    if (Node* const rt = out->retraceOnGrowth())
    {
        root = down(rt);
    }
    return std::make_tuple(down(out), false);
}

template <typename Derived>
void Node<Derived>::remove(Derived*& root, const Node* const node) noexcept
{
    if (node != nullptr)
    {
        CAVL_ASSERT(root != nullptr);  // Otherwise, the node would have to be nullptr.
        CAVL_ASSERT((node->up != nullptr) || (node == root));
        Node* p = nullptr;  // The lowest parent node that suffered a shortening of its subtree.
        bool  r = false;    // Which side of the above was shortened.
        // The first step is to update the topology and remember the node where to start the retracing from later.
        // Balancing is not performed yet so we may end up with an unbalanced tree.
        if ((node->lr[0] != nullptr) && (node->lr[1] != nullptr))
        {
            Node* const re = min(node->lr[1]);
            CAVL_ASSERT((re != nullptr) && (nullptr == re->lr[0]) && (re->up != nullptr));
            re->bf        = node->bf;
            re->lr[0]     = node->lr[0];
            re->lr[0]->up = re;
            if (re->up != node)
            {
                p = re->up;  // Retracing starts with the ex-parent of our replacement node.
                CAVL_ASSERT(p->lr[0] == re);
                p->lr[0] = re->lr[1];  // Reducing the height of the left subtree here.
                if (p->lr[0] != nullptr)
                {
                    p->lr[0]->up = p;
                }
                re->lr[1]     = node->lr[1];
                re->lr[1]->up = re;
                r             = false;
            }
            else  // In this case, we are reducing the height of the right subtree, so r=1.
            {
                p = re;    // Retracing starts with the replacement node itself as we are deleting its parent.
                r = true;  // The right child of the replacement node remains the same so we don't bother relinking
                // it.
            }
            re->up = node->up;
            if (re->up != nullptr)
            {
                re->up->lr[re->up->lr[1] == node] = re;  // Replace link in the parent of node.
            }
            else
            {
                root = down(re);
            }
        }
        else  // Either or both of the children are nullptr.
        {
            p             = node->up;
            const bool rr = node->lr[1] != nullptr;
            if (node->lr[rr] != nullptr)
            {
                node->lr[rr]->up = p;
            }
            if (p != nullptr)
            {
                r        = p->lr[1] == node;
                p->lr[r] = node->lr[rr];
                if (p->lr[r] != nullptr)
                {
                    p->lr[r]->up = p;
                }
            }
            else
            {
                root = down(node->lr[rr]);
            }
        }
        // Now that the topology is updated, perform the retracing to restore balance. We climb up adjusting the
        // balance factors until we reach the root or a parent whose balance factor becomes plus/minus one, which
        // means that that parent was able to absorb the balance delta; in other words, the height of the outer
        // subtree is unchanged, so upper balance factors shall be kept unchanged.
        if (p != nullptr)
        {
            Node* c = nullptr;
            for (;;)
            {
                c = p->adjustBalance(!r);
                p = c->up;
                if ((c->bf != 0) || (nullptr == p))  // Reached the root or the height difference is absorbed by c.
                {
                    break;
                }
                r = p->lr[1] == c;
            }
            if (nullptr == p)
            {
                CAVL_ASSERT(c != nullptr);
                root = down(c);
            }
        }
    }
}

template <typename Derived>
auto Node<Derived>::adjustBalance(const bool increment) noexcept -> Node*
{
    CAVL_ASSERT(((bf >= -1) && (bf <= +1)));
    Node*      out    = this;
    const auto new_bf = static_cast<std::int8_t>(bf + (increment ? +1 : -1));
    if ((new_bf < -1) || (new_bf > 1))
    {
        const bool   r    = new_bf < 0;   // bf<0 if left-heavy --> right rotation is needed.
        const int8_t sign = r ? +1 : -1;  // Positive if we are rotating right.
        Node* const  z    = lr[!r];
        CAVL_ASSERT(z != nullptr);  // Heavy side cannot be empty. NOLINTNEXTLINE(clang-analyzer-core.NullDereference)
        if ((z->bf * sign) <= 0)    // Parent and child are heavy on the same side or the child is balanced.
        {
            out = z;
            rotate(r);
            if (0 == z->bf)
            {
                bf    = static_cast<std::int8_t>(-sign);
                z->bf = static_cast<std::int8_t>(+sign);
            }
            else
            {
                bf    = 0;
                z->bf = 0;
            }
        }
        else  // Otherwise, the child needs to be rotated in the opposite direction first.
        {
            Node* const y = z->lr[r];
            CAVL_ASSERT(y != nullptr);  // Heavy side cannot be empty.
            out = y;
            z->rotate(!r);
            rotate(r);
            if ((y->bf * sign) < 0)
            {
                bf    = static_cast<std::int8_t>(+sign);
                y->bf = 0;
                z->bf = 0;
            }
            else if ((y->bf * sign) > 0)
            {
                bf    = 0;
                y->bf = 0;
                z->bf = static_cast<std::int8_t>(-sign);
            }
            else
            {
                bf    = 0;
                z->bf = 0;
            }
        }
    }
    else
    {
        bf = new_bf;  // Balancing not needed, just update the balance factor and call it a day.
    }
    return out;
}

template <typename Derived>
auto Node<Derived>::retraceOnGrowth() noexcept -> Node*
{
    CAVL_ASSERT(0 == bf);
    Node* c = this;      // Child
    Node* p = this->up;  // Parent
    while (p != nullptr)
    {
        const bool r = p->lr[1] == c;  // c is the right child of parent
        CAVL_ASSERT(p->lr[r] == c);
        c = p->adjustBalance(r);
        p = c->up;
        if (0 == c->bf)
        {           // The height change of the subtree made this parent perfectly balanced (as all things should be),
            break;  // hence, the height of the outer subtree is unchanged, so upper balance factors are unchanged.
        }
    }
    CAVL_ASSERT(c != nullptr);
    return (nullptr == p) ? c : nullptr;  // New root or nothing.
}

/// This is a very simple convenience wrapper that is entirely optional to use.
/// It simply keeps a single root pointer of the tree. The methods are mere wrappers over the static methods
/// defined in the Node<> template class, such that the node pointer kept in the instance of this class is passed
/// as the first argument of the static methods of Node<>.
/// Note that this type is implicitly convertible to Node<>* as the root node.
template <typename Derived>
class Tree final
{
public:
    /// Helper alias of the compatible node type.
    using NodeType    = ::cavl::Node<Derived>;
    using DerivedType = Derived;

    explicit Tree(Derived* const root)
        : root_(root)
    {
    }
    Tree()  = default;
    ~Tree() = default;

    /// Trees cannot be copied.
    Tree(const Tree&)                    = delete;
    auto operator=(const Tree&) -> Tree& = delete;

    /// Trees can be easily moved in constant time. This does not actually affect the tree itself, only this object.
    Tree(Tree&& other) noexcept
        : root_(other.root_)
    {
        CAVL_ASSERT(!traversal_in_progress_);  // Cannot modify the tree while it is being traversed.
        other.root_ = nullptr;
    }
    auto operator=(Tree&& other) noexcept -> Tree&
    {
        CAVL_ASSERT(!traversal_in_progress_);  // Cannot modify the tree while it is being traversed.
        root_       = other.root_;
        other.root_ = nullptr;
        return *this;
    }

    /// Wraps NodeType<>::search().
    template <typename Pre>
    auto search(const Pre& predicate) noexcept -> Derived*
    {
        return NodeType::template search<Pre>(*this, predicate);
    }
    template <typename Pre>
    auto search(const Pre& predicate) const noexcept -> const Derived*
    {
        return NodeType::template search<Pre>(*this, predicate);
    }
    template <typename Pre, typename Fac>
    auto search(const Pre& predicate, const Fac& factory) -> std::tuple<Derived*, bool>
    {
        CAVL_ASSERT(!traversal_in_progress_);  // Cannot modify the tree while it is being traversed.
        return NodeType::template search<Pre, Fac>(root_, predicate, factory);
    }

    /// Wraps NodeType<>::remove().
    void remove(const NodeType* const node) const noexcept
    {
        CAVL_ASSERT(!traversal_in_progress_);  // Cannot modify the tree while it is being traversed.
        return NodeType::remove(root_, node);
    }
    void remove(NodeType* const node) noexcept
    {
        CAVL_ASSERT(!traversal_in_progress_);  // Cannot modify the tree while it is being traversed.
        return NodeType::remove(root_, node);
    }

    /// Wraps NodeType<>::min/max().
    auto min() noexcept -> Derived*
    {
        return NodeType::min(*this);
    }
    auto max() noexcept -> Derived*
    {
        return NodeType::max(*this);
    }
    auto min() const noexcept -> const Derived*
    {
        return NodeType::min(*this);
    }
    auto max() const noexcept -> const Derived*
    {
        return NodeType::max(*this);
    }

    /// Wraps NodeType<>::traverse().
    template <typename Vis>
    auto traverse(const Vis& visitor, const bool reverse = false)
    {
        const TraversalIndicatorUpdater upd(*this);
        return NodeType::template traverse<Vis>(*this, visitor, reverse);
    }
    template <typename Vis>
    auto traverse(const Vis& visitor, const bool reverse = false) const
    {
        const TraversalIndicatorUpdater upd(*this);
        return NodeType::template traverse<Vis>(*this, visitor, reverse);
    }

    /// Normally these are not needed except if advanced introspection is desired.
    operator Derived*() noexcept
    {
        return root_;
    }  // NOLINT implicit conversion by design
    operator const Derived*() const noexcept
    {
        return root_;
    }  // NOLINT ditto

    /// Access i-th element of the tree in linear time. Returns nullptr if the index is out of bounds.
    auto operator[](const std::size_t index) -> Derived*
    {
        std::size_t i = index;
        return traverse([&i](auto& x) { return (i-- == 0) ? &x : nullptr; });
    }
    auto operator[](const std::size_t index) const -> const Derived*
    {
        std::size_t i = index;
        return traverse([&i](const auto& x) { return (i-- == 0) ? &x : nullptr; });
    }

    /// Beware that this convenience method has linear complexity and uses recursion. Use responsibly.
    auto size() const noexcept
    {
        auto i = 0UL;
        traverse([&i](auto& /*unused*/) { i++; });
        return i;
    }

    /// Unlike size(), this one is constant-complexity.
    auto empty() const noexcept
    {
        return root_ == nullptr;
    }

private:
    static_assert(!std::is_polymorphic<NodeType>::value,
                  "Internal check: The node type must not be a polymorphic type");
    static_assert(std::is_same<Tree<Derived>, typename NodeType::TreeType>::value, "Internal check: Bad type alias");

    /// We use a simple boolean flag instead of a nesting counter to avoid race conditions on the counter update.
    /// This implies that in the case of concurrent or recursive traversal (more than one call to traverse() within
    /// the same call stack) we may occasionally fail to detect a bona fide case of a race condition, but this is
    /// acceptable because the purpose of this feature is to provide a mere best-effort data race detection.
    class TraversalIndicatorUpdater final
    {
    public:
        explicit TraversalIndicatorUpdater(const Tree& sup) noexcept
            : that(sup)
        {
            that.traversal_in_progress_ = true;
        }
        ~TraversalIndicatorUpdater() noexcept
        {
            that.traversal_in_progress_ = false;
        }

        TraversalIndicatorUpdater(const TraversalIndicatorUpdater&)                    = delete;
        TraversalIndicatorUpdater(TraversalIndicatorUpdater&&)                         = delete;
        auto operator=(const TraversalIndicatorUpdater&) -> TraversalIndicatorUpdater& = delete;
        auto operator=(TraversalIndicatorUpdater&&) -> TraversalIndicatorUpdater&      = delete;

    private:
        const Tree& that;
    };

    Derived*              root_                  = nullptr;
    mutable volatile bool traversal_in_progress_ = false;
};

}  // namespace cavl
