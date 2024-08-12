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

#include <array>
#include <cstddef>
#include <cstdint>
#include <tuple>
#include <type_traits>
#include <utility>

/// If CAVL is used in throughput-critical code, then it is recommended to disable assertion checks as they may
/// be costly in terms of execution time.
#ifndef CAVL_ASSERT
#    if defined(CAVL_NO_ASSERT) && CAVL_NO_ASSERT
// NOLINTNEXTLINE(cppcoreguidelines-macro-usage) function-like macro
#        define CAVL_ASSERT(x) (void) 0 /* NOSONAR cpp:S960 */
#    else
#        include <cassert>
// NOLINTNEXTLINE(cppcoreguidelines-macro-usage) function-like macro
#        define CAVL_ASSERT(x) assert(x) /* NOSONAR cpp:S960 */
#    endif
#endif

// NOLINTBEGIN(cppcoreguidelines-pro-bounds-constant-array-index)

namespace cavl
{
template <typename Derived>
class Tree;

/// The tree node type is to be composed with the user type through CRTP inheritance.
/// For instance, the derived type might be a key-value pair struct defined in the user code.
/// The worst-case complexity of all operations is O(log n), unless specifically noted otherwise.
/// Note that this class has no public members. The user type should re-export them if needed (usually it is not).
/// The size of this type is 4x pointer size (16 bytes on a 32-bit platform).
///
/// No Sonar cpp:S1448 b/c this is the main node entity without public members - maintainability is not a concern here.
/// No Sonar cpp:S4963 b/c `Node` supports move operation.
///
template <typename Derived>
class Node  // NOSONAR cpp:S1448 cpp:S4963
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

    // Tree nodes can be moved. We update the pointers in the adjacent nodes to keep the tree valid,
    // as well as root node pointer if needed (see `moveFrom`). This operation is constant time.
    Node(Node&& other) noexcept
    {
        moveFrom(other);
    }
    auto operator=(Node&& other) noexcept -> Node&
    {
        moveFrom(other);
        return *this;
    }

protected:
    Node()  = default;
    ~Node() = default;

    /// Accessors for advanced tree introspection. Not needed for typical usage.
    bool isLinked() const noexcept
    {
        return nullptr != up;
    }
    bool isRoot() const noexcept
    {
        return isLinked() && (!up->isLinked());
    }
    auto getParentNode() noexcept -> Derived*
    {
        return isRoot() ? nullptr : down(up);
    }
    auto getParentNode() const noexcept -> const Derived*
    {
        return isRoot() ? nullptr : down(up);
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
        return searchImpl<Derived>(root, predicate);
    }
    template <typename Pre>
    static auto search(const Node* const root, const Pre& predicate) noexcept -> const Derived*
    {
        return searchImpl<const Derived>(root, predicate);
    }

    /// This is like the regular search function except that if the node is missing, the factory will be invoked
    /// (without arguments) to construct a new one and insert it into the tree immediately.
    /// The root node (inside the origin) may be replaced in the process.
    /// If this method returns true, the tree is not modified;
    /// otherwise, the factory was (successfully!) invoked and a new node has been inserted into the tree.
    /// The factory does not need to be noexcept (may throw). It may also return nullptr to indicate intentional
    /// refusal to modify the tree, or f.e. in case of out of memory - result will be `(nullptr, true)` tuple.
    template <typename Pre, typename Fac>
    static auto search(Node& origin, const Pre& predicate, const Fac& factory) -> std::tuple<Derived*, bool>;

    /// Remove the specified node from its tree. The root node (inside the origin) may be replaced in the process.
    /// The function has no effect if the node pointer is nullptr.
    /// If the node is not in the tree, the behavior is undefined; it may create cycles in the tree which is deadly.
    /// It is safe to pass the result of search() directly as the second argument:
    ///     Node<T>::remove(root, Node<T>::search(root, search_predicate));
    ///
    /// No Sonar cpp:S6936 b/c the `remove` method name isolated inside `Node` type (doesn't conflict with C).
    static void remove(Node& origin, const Node* const node) noexcept;  // NOSONAR cpp:S6936

    /// This is like the const overload of remove() except that the node pointers are invalidated afterward for safety.
    ///
    /// No Sonar cpp:S6936 b/c the `remove` method name isolated inside `Node` type (doesn't conflict with C).
    static void remove(Node& origin, Node* const node) noexcept  // NOSONAR cpp:S6936
    {
        remove(origin, static_cast<const Node*>(node));
        if (nullptr != node)
        {
            node->unlink();
        }
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
    /// If the return type is non-void, then it shall be default-constructable and convertible to bool; in this case,
    /// traversal will stop when the first true value is returned, which is propagated back to the caller; if none
    /// of the calls returned true or the tree is empty, a default value is constructed and returned.
    /// The tree shall not be modified while traversal is in progress, otherwise bad memory access will likely occur.
    template <typename Vis, typename R = invoke_result<Vis, Derived&>>
    static auto traverseInOrder(Derived* const root, const Vis& visitor, const bool reverse = false)  //
        -> std::enable_if_t<!std::is_void<R>::value, R>
    {
        return traverseInOrderImpl<R, Node>(root, visitor, reverse);
    }
    template <typename Vis>
    static auto traverseInOrder(Derived* const root, const Vis& visitor, const bool reverse = false)  //
        -> std::enable_if_t<std::is_void<invoke_result<Vis, Derived&>>::value>
    {
        traverseInOrderImpl<Node>(root, visitor, reverse);
    }
    template <typename Vis, typename R = invoke_result<Vis, const Derived&>>
    static auto traverseInOrder(const Derived* const root, const Vis& visitor, const bool reverse = false)  //
        -> std::enable_if_t<!std::is_void<R>::value, R>
    {
        return traverseInOrderImpl<R, const Node>(root, visitor, reverse);
    }
    template <typename Vis>
    static auto traverseInOrder(const Derived* const root, const Vis& visitor, const bool reverse = false)  //
        -> std::enable_if_t<std::is_void<invoke_result<Vis, const Derived&>>::value>
    {
        traverseInOrderImpl<const Node>(root, visitor, reverse);
    }

    /// @breaf Post-order (or reverse-post-order) traversal of the tree.
    ///
    /// "Post" nature of the traversal guarantees that, once a node reference is passed to the visitor,
    /// traversal won't use or reference this node anymore, so it is safe to modify the node in the visitor -
    /// f.e. deallocate node's memory for an efficient "release whole tree" scenario. But the tree itself
    /// shall not be modified while traversal is in progress, otherwise bad memory access will likely occur.
    ///
    /// @param root The root node of the tree to traverse.
    /// @param visitor The callable object to invoke for each node. The visitor is invoked with a reference
    ///                to each node as a POST-action call, AFTER visiting all of its children.
    /// @param reverse If `false`, the traversal visits first "left" children, then "right" children.
    ///                If `true`, the traversal is performed in reverse order ("right" first, then "left").
    ///                In either case, the current node is visited last (hence the post-order).
    ///
    template <typename Vis>
    static void traversePostOrder(Derived* const root, const Vis& visitor, const bool reverse = false)
    {
        traversePostOrderImpl<Node>(root, visitor, reverse);
    }
    template <typename Vis>
    static void traversePostOrder(const Derived* const root, const Vis& visitor, const bool reverse = false)
    {
        traversePostOrderImpl<const Node>(root, visitor, reverse);
    }

private:
    void moveFrom(Node& other) noexcept
    {
        CAVL_ASSERT(!isLinked());  // Should not be part of any tree yet.

        up    = other.up;
        lr[0] = other.lr[0];
        lr[1] = other.lr[1];
        bf    = other.bf;
        other.unlink();

        if (nullptr != up)
        {
            up->lr[up->lr[1] == &other] = this;
        }
        if (nullptr != lr[0])
        {
            lr[0]->up = this;
        }
        if (nullptr != lr[1])
        {
            lr[1]->up = this;
        }
    }

    void rotate(const bool r) noexcept
    {
        CAVL_ASSERT(isLinked());
        CAVL_ASSERT((lr[!r] != nullptr) && ((bf >= -1) && (bf <= +1)));
        Node* const z             = lr[!r];
        up->lr[up->lr[1] == this] = z;
        z->up                     = up;
        up                        = z;
        lr[!r]                    = z->lr[r];
        if (lr[!r] != nullptr)
        {
            lr[!r]->up = this;
        }
        z->lr[r] = this;
    }

    auto adjustBalance(const bool increment) noexcept -> Node*;

    auto retraceOnGrowth() noexcept -> Node*;

    template <typename NodeT, typename DerivedT, typename Vis>
    static void traverseInOrderImpl(DerivedT* const root, const Vis& visitor, const bool reverse);
    template <typename Result, typename NodeT, typename DerivedT, typename Vis>
    static auto traverseInOrderImpl(DerivedT* const root, const Vis& visitor, const bool reverse) -> Result;

    template <typename NodeT, typename DerivedT, typename Vis>
    static void traversePostOrderImpl(DerivedT* const root, const Vis& visitor, const bool reverse);

    template <typename DerivedT, typename NodeT, typename Pre>
    static auto searchImpl(NodeT* const root, const Pre& predicate) noexcept -> DerivedT*
    {
        NodeT* n = root;
        while (n != nullptr)
        {
            CAVL_ASSERT(nullptr != n->up);

            DerivedT* const derived = down(n);
            const auto      cmp     = predicate(*derived);
            if (0 == cmp)
            {
                return derived;
            }
            n = n->lr[cmp > 0];
        }
        return nullptr;
    }

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

    Node*                up = nullptr;
    std::array<Node*, 2> lr{};
    std::int8_t          bf = 0;
};

template <typename Derived>
template <typename Pre, typename Fac>
auto Node<Derived>::search(Node& origin, const Pre& predicate, const Fac& factory) -> std::tuple<Derived*, bool>
{
    CAVL_ASSERT(!origin.isLinked());
    Node*& root = origin.lr[0];

    Node* out = nullptr;
    Node* up  = root;
    Node* n   = root;
    bool  r   = false;
    while (n != nullptr)
    {
        CAVL_ASSERT(n->isLinked());

        const auto cmp = predicate(*down(n));
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
    CAVL_ASSERT(out != &origin);
    if (nullptr == out)
    {
        return std::make_tuple(nullptr, true);
    }
    out->unlink();

    if (up != nullptr)
    {
        CAVL_ASSERT(up->lr[r] == nullptr);
        up->lr[r] = out;
        out->up   = up;
    }
    else
    {
        root    = out;
        out->up = &origin;
    }
    if (Node* const rt = out->retraceOnGrowth())
    {
        root = rt;
    }
    return std::make_tuple(down(out), false);
}

// No Sonar cpp:S6936 b/c the `remove` method name isolated inside `Node` type (doesn't conflict with C).
// No Sonar cpp:S3776 cpp:S134 cpp:S5311 b/c this is the main removal tool - maintainability is not a concern here.
template <typename Derived>
void Node<Derived>::remove(Node& origin, const Node* const node) noexcept  // NOSONAR cpp:S6936 cpp:S3776
{
    CAVL_ASSERT(!origin.isLinked());
    CAVL_ASSERT(node != &origin);  // The origin node is not part of the tree, so it cannot be removed.

    if (node != nullptr)
    {
        Node*& root = origin.lr[0];
        CAVL_ASSERT(root != nullptr);  // Otherwise, the node would have to be nullptr.
        CAVL_ASSERT(node->isLinked());
        Node* p = nullptr;  // The lowest parent node that suffered a shortening of its subtree.
        bool  r = false;    // Which side of the above was shortened.
        // The first step is to update the topology and remember the node where to start the retracing from later.
        // Balancing is not performed yet, so we may end up with an unbalanced tree.
        if ((node->lr[0] != nullptr) && (node->lr[1] != nullptr))
        {
            Node* const re = min(node->lr[1]);
            CAVL_ASSERT((re != nullptr) && (nullptr == re->lr[0]) && (re->up != nullptr));
            re->bf        = node->bf;
            re->lr[0]     = node->lr[0];
            re->lr[0]->up = re;
            if (re->up != node)
            {
                p = re->getParentNode();  // Retracing starts with the ex-parent of our replacement node.
                CAVL_ASSERT(p->lr[0] == re);
                p->lr[0] = re->lr[1];     // Reducing the height of the left subtree here.
                if (p->lr[0] != nullptr)  // NOSONAR cpp:S134
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
                r = true;  // The right child of the replacement node remains the same, so we don't bother relinking it.
            }
            re->up = node->up;
            if (!re->isRoot())
            {
                re->up->lr[re->up->lr[1] == node] = re;  // Replace link in the parent of node.
            }
            else
            {
                root = re;
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
            if (!node->isRoot())
            {
                r        = p->lr[1] == node;
                p->lr[r] = node->lr[rr];
                if (p->lr[r] != nullptr)  // NOSONAR cpp:S134
                {
                    p->lr[r]->up = p;
                }
            }
            else
            {
                root = node->lr[rr];
            }
        }
        // Now that the topology is updated, perform the retracing to restore balance. We climb up adjusting the
        // balance factors until we reach the root or a parent whose balance factor becomes plus/minus one, which
        // means that that parent was able to absorb the balance delta; in other words, the height of the outer
        // subtree is unchanged, so upper balance factors shall be kept unchanged.
        if (p != &origin)
        {
            Node* c = nullptr;
            for (;;)  // NOSONAR cpp:S5311
            {
                c = p->adjustBalance(!r);
                p = c->getParentNode();
                if ((c->bf != 0) || (nullptr == p))  // NOSONAR cpp:S134
                {
                    // Reached the root or the height difference is absorbed by `c`.
                    break;
                }
                r = p->lr[1] == c;
            }
            if (nullptr == p)
            {
                CAVL_ASSERT(c != nullptr);
                root = c;
            }
        }
    }
}

template <typename Derived>
auto Node<Derived>::adjustBalance(const bool increment) noexcept -> Node*
{
    CAVL_ASSERT(isLinked());
    CAVL_ASSERT(((bf >= -1) && (bf <= +1)));
    Node*      out    = this;
    const auto new_bf = static_cast<std::int8_t>(bf + (increment ? +1 : -1));
    if ((new_bf < -1) || (new_bf > 1))
    {
        const bool        r    = new_bf < 0;   // bf<0 if left-heavy --> right rotation is needed.
        const std::int8_t sign = r ? +1 : -1;  // Positive if we are rotating right.
        Node* const       z    = lr[!r];
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
    Node* c = this;                   // Child
    Node* p = this->getParentNode();  // Parent
    while (p != nullptr)
    {
        const bool r = p->lr[1] == c;  // c is the right child of parent
        CAVL_ASSERT(p->lr[r] == c);
        c = p->adjustBalance(r);
        p = c->getParentNode();
        if (0 == c->bf)
        {           // The height change of the subtree made this parent perfectly balanced (as all things should be),
            break;  // hence, the height of the outer subtree is unchanged, so upper balance factors are unchanged.
        }
    }
    CAVL_ASSERT(c != nullptr);
    return (nullptr == p) ? c : nullptr;  // New root or nothing.
}

// No Sonar cpp:S134 b/c this is the main in-order traversal tool - maintainability is not a concern here.
template <typename Derived>
template <typename NodeT, typename DerivedT, typename Vis>
void Node<Derived>::traverseInOrderImpl(DerivedT* const root, const Vis& visitor, const bool reverse)
{
    NodeT* node = root;
    NodeT* prev = nullptr;

    while (nullptr != node)
    {
        NodeT* next = node->getParentNode();

        // Did we come down to this node from `prev`?
        if (prev == next)
        {
            if (auto* const left = node->lr[reverse])
            {
                next = left;
            }
            else
            {
                visitor(*down(node));

                if (auto* const right = node->lr[!reverse])  // NOSONAR cpp:S134
                {
                    next = right;
                }
            }
        }
        // Did we come up to this node from the left child?
        else if (prev == node->lr[reverse])
        {
            visitor(*down(node));

            if (auto* const right = node->lr[!reverse])
            {
                next = right;
            }
        }
        else
        {
            // next has already been set to the parent node.
        }

        prev = std::exchange(node, next);
    }
}

// No Sonar cpp:S134 b/c this is the main in-order returning traversal tool - maintainability is not a concern here.
template <typename Derived>
template <typename Result, typename NodeT, typename DerivedT, typename Vis>
auto Node<Derived>::traverseInOrderImpl(DerivedT* const root, const Vis& visitor, const bool reverse) -> Result
{
    NodeT* node = root;
    NodeT* prev = nullptr;

    while (nullptr != node)
    {
        NodeT* next = node->getParentNode();

        // Did we come down to this node from `prev`?
        if (prev == next)
        {
            if (auto* const left = node->lr[reverse])
            {
                next = left;
            }
            else
            {
                // NOLINTNEXTLINE(*-qualified-auto)
                if (auto t = visitor(*down(node)))  // NOSONAR cpp:S134
                {
                    return t;
                }

                if (auto* const right = node->lr[!reverse])  // NOSONAR cpp:S134
                {
                    next = right;
                }
            }
        }
        // Did we come up to this node from the left child?
        else if (prev == node->lr[reverse])
        {
            if (auto t = visitor(*down(node)))  // NOLINT(*-qualified-auto)
            {
                return t;
            }

            if (auto* const right = node->lr[!reverse])
            {
                next = right;
            }
        }
        else
        {
            // next has already been set to the parent node.
        }

        prev = std::exchange(node, next);
    }
    return Result{};
}

template <typename Derived>
template <typename NodeT, typename DerivedT, typename Vis>
void Node<Derived>::traversePostOrderImpl(DerivedT* const root, const Vis& visitor, const bool reverse)
{
    NodeT* node = root;
    NodeT* prev = nullptr;

    while (nullptr != node)
    {
        NodeT* next = node->getParentNode();

        // Did we come down to this node from `prev`?
        if (prev == next)
        {
            if (auto* const left = node->lr[reverse])
            {
                next = left;
            }
            else if (auto* const right = node->lr[!reverse])
            {
                next = right;
            }
            else
            {
                visitor(*down(node));
            }
        }
        // Did we come up to this node from the left child?
        else if (prev == node->lr[reverse])
        {
            if (auto* const right = node->lr[!reverse])
            {
                next = right;
            }
            else
            {
                visitor(*down(node));
            }
        }
        // We came up to this node from the right child.
        else
        {
            visitor(*down(node));
        }

        prev = std::exchange(node, next);
    }
}

/// This is a very simple convenience wrapper that is entirely optional to use.
/// It simply keeps a single root pointer of the tree. The methods are mere wrappers over the static methods
/// defined in the Node<> template class, such that the node pointer kept in the instance of this class is passed
/// as the first argument of the static methods of Node<>.
/// Note that this type is implicitly convertible to Node<>* as the root node.
///
/// No Sonar cpp:S3624 b/c it's by design that ~Tree destructor is default one - resource management (allocation
/// and de-allocation of nodes) is client responsibility.
template <typename Derived>
class Tree final  // NOSONAR cpp:S3624
{
public:
    /// Helper alias of the compatible node type.
    using NodeType    = ::cavl::Node<Derived>;
    using DerivedType = Derived;

    Tree()  = default;
    ~Tree() = default;

    /// Trees cannot be copied.
    Tree(const Tree&)                    = delete;
    auto operator=(const Tree&) -> Tree& = delete;

    /// Trees can be easily moved in constant time. This does not actually affect the tree itself, only this object.
    Tree(Tree&& other) noexcept
        : origin_node_{std::move(other.origin_node_)}
    {
        CAVL_ASSERT(!traversal_in_progress_);  // Cannot modify the tree while it is being traversed.
    }
    auto operator=(Tree&& other) noexcept -> Tree&
    {
        CAVL_ASSERT(!traversal_in_progress_);  // Cannot modify the tree while it is being traversed.
        origin_node_ = std::move(other.origin_node_);
        return *this;
    }

    /// Wraps NodeType<>::search().
    template <typename Pre>
    auto search(const Pre& predicate) noexcept -> Derived*
    {
        return NodeType::template search<Pre>(getRootNode(), predicate);
    }
    template <typename Pre>
    auto search(const Pre& predicate) const noexcept -> const Derived*
    {
        return NodeType::template search<Pre>(getRootNode(), predicate);
    }
    template <typename Pre, typename Fac>
    auto search(const Pre& predicate, const Fac& factory) -> std::tuple<Derived*, bool>
    {
        CAVL_ASSERT(!traversal_in_progress_);  // Cannot modify the tree while it is being traversed.
        return NodeType::template search<Pre, Fac>(origin_node_, predicate, factory);
    }

    /// Wraps NodeType<>::remove().
    ///
    /// No Sonar cpp:S6936 b/c the `remove` method name isolated inside `Tree` type (doesn't conflict with C).
    void remove(NodeType* const node) noexcept  // NOSONAR cpp:S6936
    {
        CAVL_ASSERT(!traversal_in_progress_);  // Cannot modify the tree while it is being traversed.
        NodeType::remove(origin_node_, node);
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

    /// Wraps NodeType<>::traverseInOrder().
    template <typename Vis>
    auto traverseInOrder(const Vis& visitor, const bool reverse = false)
    {
        const TraversalIndicatorUpdater upd(*this);
        return NodeType::template traverseInOrder<Vis>(*this, visitor, reverse);
    }
    template <typename Vis>
    auto traverseInOrder(const Vis& visitor, const bool reverse = false) const
    {
        const TraversalIndicatorUpdater upd(*this);
        return NodeType::template traverseInOrder<Vis>(*this, visitor, reverse);
    }

    /// Wraps NodeType<>::traversePostOrder().
    template <typename Vis>
    void traversePostOrder(const Vis& visitor, const bool reverse = false)
    {
        const TraversalIndicatorUpdater upd(*this);
        NodeType::template traversePostOrder<Vis>(*this, visitor, reverse);
    }
    template <typename Vis>
    void traversePostOrder(const Vis& visitor, const bool reverse = false) const
    {
        const TraversalIndicatorUpdater upd(*this);
        NodeType::template traversePostOrder<Vis>(*this, visitor, reverse);
    }

    /// Normally these are not needed except if advanced introspection is desired.
    ///
    /// No linting and Sonar cpp:S1709 b/c implicit conversion by design.
    // NOLINTNEXTLINE(google-explicit-constructor,hicpp-explicit-conversions)
    operator Derived*() noexcept  // NOSONAR cpp:S1709
    {
        return getRootNode();
    }
    // NOLINTNEXTLINE(google-explicit-constructor,hicpp-explicit-conversions)
    operator const Derived*() const noexcept  // NOSONAR cpp:S1709
    {
        return getRootNode();
    }

    /// Access i-th element of the tree in linear time. Returns nullptr if the index is out of bounds.
    auto operator[](const std::size_t index) -> Derived*
    {
        std::size_t i = index;
        // No Sonar cpp:S881 b/c this decrement is pretty much straightforward - no maintenance concerns.
        return traverseInOrder([&i](auto& x) { return (i-- == 0) ? &x : nullptr; });  // NOSONAR cpp:S881
    }
    auto operator[](const std::size_t index) const -> const Derived*
    {
        std::size_t i = index;
        // No Sonar cpp:S881 b/c this decrement is pretty much straightforward - no maintenance concerns.
        return traverseInOrder([&i](const auto& x) { return (i-- == 0) ? &x : nullptr; });  // NOSONAR cpp:S881
    }

    /// Beware that this convenience method has linear complexity. Use responsibly.
    auto size() const noexcept
    {
        auto i = 0UL;
        traverseInOrder([&i](auto& /*unused*/) { i++; });
        return i;
    }

    /// Unlike size(), this one is constant-complexity.
    auto empty() const noexcept
    {
        return getRootNode() == nullptr;
    }

private:
    static_assert(!std::is_polymorphic<NodeType>::value,
                  "Internal check: The node type must not be a polymorphic type");
    static_assert(std::is_same<Tree<Derived>, typename NodeType::TreeType>::value, "Internal check: Bad type alias");

    /// We use a simple boolean flag instead of a nesting counter to avoid race conditions on the counter update.
    /// This implies that in the case of concurrent or recursive traversal (more than one call to traverseXxx() within
    /// the same call stack) we may occasionally fail to detect a bona fide case of a race condition, but this is
    /// acceptable because the purpose of this feature is to provide a mere best-effort data race detection.
    ///
    /// No Sonar cpp:S4963 b/c of the RAII pattern.
    class TraversalIndicatorUpdater final  // NOSONAR cpp:S4963
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

    // root node pointer is stored in the origin_node_ left child.
    auto getRootNode() noexcept -> Derived*
    {
        return origin_node_.getChildNode(false);
    }
    auto getRootNode() const noexcept -> const Derived*
    {
        return origin_node_.getChildNode(false);
    }

    // This a "fake" node, is not part of the tree itself, but it is used to store the root node pointer.
    // The root node pointer is stored in the left child (see `getRootNode` methods).
    // This is the only node which has the `up` pointer set to `nullptr`;
    // all other "real" nodes always have non-null `up` pointer,
    // including the root node whos `up` points to this origin node (see `isRoot` method).
    Node<Derived> origin_node_{};

    // No Sonar cpp:S4963 b/c of implicit modification by the `TraversalIndicatorUpdater` RAII class,
    // even for `const` instance of the `Tree` class (hence the `mutable volatile` keywords).
    mutable volatile bool traversal_in_progress_ = false;  // NOSONAR cpp:S3687
};

}  // namespace cavl

// NOLINTEND(cppcoreguidelines-pro-bounds-constant-array-index)
