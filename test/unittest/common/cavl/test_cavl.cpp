/// Copyright (c) 2021 Pavel Kirienko <pavel@uavcan.org>

#include <libcyphal/common/cavl/cavl.hpp>

#include <gmock/gmock.h>
#include <gtest/gtest.h>

#include <algorithm>
#include <array>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <functional>
#include <iostream>
#include <limits>
#include <memory>
#include <numeric>
#include <sstream>
#include <string>
#include <type_traits>
#include <vector>

#if __cplusplus >= 201703L
#    define NODISCARD [[nodiscard]]
#    define UNUSED [[maybe_unused]]
#else
#    if defined(__GNUC__) || defined(__clang__) || defined(__CC_ARM)
#        define NODISCARD __attribute__((warn_unused_result))
#        define UNUSED __attribute__((unused))
#    else
#        define NODISCARD
#        define UNUSED
#    endif
#endif

namespace
{
/// These aliases are introduced to keep things nicely aligned in test cases.
constexpr auto Zzzz  = nullptr;
constexpr auto Zzzzz = nullptr;

/// Simple test -- fully static type with private inheritance.
class My : public cavl::Node<My>
{
public:
    explicit My(const std::uint16_t v)
        : value(v)
    {
    }
    using Self = cavl::Node<My>;
    using Self::isLinked;
    using Self::isRoot;
    using Self::getChildNode;
    using Self::getParentNode;
    using Self::getBalanceFactor;
    using Self::search;
    using Self::remove;
    using Self::traverseInOrder;
    using Self::traversePostOrder;
    using Self::min;
    using Self::max;

    NODISCARD auto getValue() const -> std::uint16_t
    {
        return value;
    }

private:
    std::uint16_t value = 0;

    // These dummy fields are needed to ensure the node class does not make incorrect references to the fields
    // defined in the derived class. That would trigger compilation error in this case, but may be deadly in the field.
    using E = struct
    {};
    UNUSED E up;
    UNUSED E lr;
    UNUSED E bf;
};
using MyTree = cavl::Tree<My>;
static_assert(std::is_same<My::TreeType, MyTree>::value, "");
static_assert(std::is_same<cavl::Node<My>, MyTree::NodeType>::value, "");

template <typename T>
using N = typename cavl::Node<T>::DerivedType;
static_assert(std::is_same<My, N<My>>::value, "");

template <typename T>
NODISCARD bool checkLinkage(const N<T>* const           self,
                            const N<T>* const           up,
                            const std::array<N<T>*, 2>& lr,
                            const std::int8_t           bf)
{
    return (self->getParentNode() == up) &&                                                      //
           (self->getChildNode(false) == lr.at(0)) && (self->getChildNode(true) == lr.at(1)) &&  //
           (self->getBalanceFactor() == bf) &&                                                   //
           ((up == nullptr) || (up->getChildNode(false) == self) || (up->getChildNode(true) == self)) &&
           ((lr.at(0) == nullptr) || (lr.at(0)->getParentNode() == self)) &&  //
           ((lr.at(1) == nullptr) || (lr.at(1)->getParentNode() == self));
}

template <typename T>
NODISCARD auto getHeight(const N<T>* const n) -> std::int8_t  // NOLINT(misc-no-recursion)
{
    return (n != nullptr) ? static_cast<std::int8_t>(1 + std::max(getHeight<T>(n->getChildNode(false)),  //
                                                                  getHeight<T>(n->getChildNode(true))))
                          : 0;
}

/// Returns the size if the tree is ordered correctly, otherwise SIZE_MAX.
template <typename T>
NODISCARD std::size_t checkNormalOrdering(const N<T>* const root)
{
    const N<T>* prev  = nullptr;
    bool        valid = true;
    std::size_t size  = 0;
    T::traverseInOrder(root, [&](const N<T>& nd) {
        if (prev != nullptr)
        {
            valid = valid && (prev->getValue() < nd.getValue());
        }
        prev = &nd;
        size++;
    });

    return valid ? size : std::numeric_limits<std::size_t>::max();
}
template <typename T>
std::size_t checkReverseOrdering(const N<T>* const root)
{
    const N<T>* prev  = nullptr;
    bool        valid = true;
    std::size_t size  = 0;
    T::traverseInOrder(
        root,
        [&](const N<T>& nd) {
            if (prev != nullptr)
            {
                valid = valid && (prev->getValue() > nd.getValue());
            }
            prev = &nd;
            size++;

            // Fake `return` to cover other `traverseInOrder` overload (the returning one).
            return false;
        },
        true /* reverse */);

    return valid ? size : std::numeric_limits<std::size_t>::max();
}
template <typename T>
NODISCARD std::size_t checkOrdering(const N<T>* const root)
{
    const std::size_t ordered = checkNormalOrdering<T>(root);
    const std::size_t reverse = checkReverseOrdering<T>(root);
    return (ordered == reverse) ? ordered : std::numeric_limits<std::size_t>::max();
}

template <typename T>
void checkPostOrdering(const N<T>* const root, const std::vector<std::uint16_t>& expected, const bool reverse = false)
{
    std::vector<std::uint16_t> order;
    T::traversePostOrder(root, [&](const N<T>& nd) { order.push_back(nd.getValue()); }, reverse);
    EXPECT_THAT(order, ::testing::ContainerEq(expected));
}

template <typename T>
// NOLINTNEXTLINE(misc-no-recursion)
NODISCARD const N<T>* findBrokenAncestry(const N<T>* const n, const N<T>* const parent = nullptr)
{
    if ((n != nullptr) && (n->getParentNode() == parent))
    {
        for (const bool v : {true, false})
        {
            if (const N<T>* p = findBrokenAncestry<T>(n->getChildNode(v), n))
            {
                return p;
            }
        }
        return nullptr;
    }
    return n;
}

template <typename T>
NODISCARD const N<T>* findBrokenBalanceFactor(const N<T>* const n)  // NOLINT(misc-no-recursion)
{
    if (n != nullptr)
    {
        if (std::abs(n->getBalanceFactor()) > 1)
        {
            return n;
        }
        if (n->getBalanceFactor() != (getHeight<T>(n->getChildNode(true)) - getHeight<T>(n->getChildNode(false))))
        {
            return n;
        }
        for (const bool v : {true, false})
        {
            if (auto* const ch = n->getChildNode(v))
            {
                if (auto* const p = findBrokenBalanceFactor<T>(ch))
                {
                    return p;
                }
            }
        }
    }
    return nullptr;
}

template <typename T>
NODISCARD auto toGraphviz(const cavl::Tree<T>& tr) -> std::string
{
    std::ostringstream ss;
    ss << "// Feed the following text to Graphviz, or use an online UI like https://edotor.net/\n"
       << "digraph {\n"
       << "node[style=filled,shape=circle,fontcolor=white,penwidth=0,fontname=\"monospace\",fixedsize=1,fontsize=18];\n"
       << "edge[arrowhead=none,penwidth=2];\n"
       << "nodesep=0.0;ranksep=0.3;splines=false;\n";
    tr.traverseInOrder([&](const typename cavl::Tree<T>::DerivedType& x) {
        const char* const fill_color =  // NOLINTNEXTLINE(*-avoid-nested-conditional-operator)
            (x.getBalanceFactor() == 0) ? "black" : ((x.getBalanceFactor() > 0) ? "orange" : "blue");
        ss << x.getValue() << "[fillcolor=" << fill_color << "];";
    });
    ss << "\n";
    tr.traverseInOrder([&](const typename cavl::Tree<T>::DerivedType& x) {
        if (const auto* const ch = x.getChildNode(false))
        {
            ss << x.getValue() << ":sw->" << ch->getValue() << ":n;";
        }
        if (const auto* const ch = x.getChildNode(true))
        {
            ss << x.getValue() << ":se->" << ch->getValue() << ":n;";
        }
    });
    ss << "\n}";
    return ss.str();
}

auto getRandomByte()
{
    return static_cast<std::uint8_t>((0xFFLL * std::rand()) / RAND_MAX);
}

template <typename N>
void testManual(const std::function<N*(std::uint8_t)>& factory, const std::function<N*(N*)>& node_mover)
{
    using TreeType = typename N::TreeType;
    std::vector<N*> t;
    // Build a tree with 31 elements from 1 to 31 inclusive by adding new elements successively:
    //                               16
    //                       /               `
    //               8                              24
    //           /        `                      /       `
    //       4              12              20              28
    //     /    `         /    `          /    `          /    `
    //   2       6      10      14      18      22      26      30
    //  / `     / `     / `     / `     / `     / `     / `     / `
    // 1   3   5   7   9  11  13  15  17  19  21  23  25  27  29  31
    for (std::uint8_t i = 0; i < 32; i++)
    {
        t.emplace_back(factory(i));
    }
    // Build the actual tree.
    TreeType tr;
    EXPECT_TRUE(tr.empty());
    const auto insert = [&](const std::uint8_t i) {
        std::cout << "Inserting " << static_cast<int>(i) << std::endl;
        const auto pred = [&](const N& v) { return t.at(i)->getValue() - v.getValue(); };
        EXPECT_EQ(nullptr, tr.search(pred));
        EXPECT_EQ(nullptr, static_cast<const TreeType&>(tr).search(pred));
        EXPECT_FALSE(t[i]->isLinked());
        auto result = tr.search(pred, [&] { return t[i]; });
        EXPECT_TRUE(t[i]->isLinked());
        EXPECT_EQ(t[i], std::get<0>(result));
        EXPECT_FALSE(std::get<1>(result));
        EXPECT_EQ(t[i], tr.search(pred));
        EXPECT_EQ(t[i], static_cast<const TreeType&>(tr).search(pred));
        // Validate the tree after every mutation.
        EXPECT_TRUE(!tr.empty());
        EXPECT_EQ(nullptr, findBrokenBalanceFactor<N>(tr));
        EXPECT_EQ(nullptr, findBrokenAncestry<N>(tr));
        EXPECT_TRUE(checkOrdering<N>(tr) < std::numeric_limits<std::size_t>::max());
    };
    // Insert out of order to cover more branches in the insertion method.
    // We can't really go full random because we need perfectly balanced tree for the manual tests that follow.
    const std::array<std::uint8_t, 31> insertion_order{{
        2,  1,  4,  3,  6, 5, 8, 7, 10, 9, 12, 11, 14, 13, 16, 15, 18, 17, 20, 19, 22, 21, 24, 23, 26, 25, 28,  //
        27, 31, 30, 29,
    }};
    for (const auto i : insertion_order)
    {
        insert(i);
    }
    EXPECT_EQ(31, tr.size());
    EXPECT_EQ(31, checkOrdering<N>(tr));
    std::cout << toGraphviz(tr) << std::endl;
    EXPECT_EQ(nullptr, findBrokenBalanceFactor<N>(tr));
    EXPECT_EQ(nullptr, findBrokenAncestry<N>(tr));
    EXPECT_EQ(31, checkOrdering<N>(tr));
    // Check composition -- ensure that every element is in the tree and it is there exactly once.
    {
        bool seen[32]{};
        tr.traverseInOrder([&](const N& n) {
            EXPECT_FALSE(seen[n.getValue()]);
            seen[n.getValue()] = true;
        });
        EXPECT_TRUE(std::all_of(&seen[1], &seen[31], [](bool x) { return x; }));
    }
    EXPECT_EQ(t.at(1), tr.min());
    EXPECT_EQ(t.at(31), tr.max());
    EXPECT_EQ(t.at(1), static_cast<const TreeType&>(tr).min());
    EXPECT_EQ(t.at(31), static_cast<const TreeType&>(tr).max());
    // Check index operator, both const and mutable.
    EXPECT_EQ(10, tr[9]->getValue());
    EXPECT_EQ(10, static_cast<const TreeType&>(tr)[9]->getValue());
    EXPECT_EQ(nullptr, tr[32]);
    EXPECT_EQ(nullptr, static_cast<const TreeType&>(tr)[100500UL]);
    for (auto i = 1U; i <= 31; i++)
    {
        EXPECT_NE(nullptr, tr[i - 1]);
        EXPECT_EQ(i, tr[i - 1]->getValue());
        EXPECT_EQ(i, static_cast<const TreeType&>(tr)[i - 1]->getValue());
    }
    checkPostOrdering<N>(tr, {1,  3,  2,  5,  7,  6,  4,  9,  11, 10, 13, 15, 14, 12, 8, 17,
                              19, 18, 21, 23, 22, 20, 25, 27, 26, 29, 31, 30, 28, 24, 16});
    checkPostOrdering<N>(tr,
                         {31, 29, 30, 27, 25, 26, 28, 23, 21, 22, 19, 17, 18, 20, 24, 15,
                          13, 14, 11, 9,  10, 12, 7,  5,  6,  3,  1,  2,  4,  8,  16},
                         true);
    EXPECT_TRUE(t[16]->isRoot());
    EXPECT_FALSE(t[24]->isRoot());

    // MOVE 16, 18 & 23
    t[16] = node_mover(t[16]);
    t[18] = node_mover(t[18]);
    t[23] = node_mover(t[23]);
    EXPECT_TRUE(t[16]->isRoot());
    EXPECT_FALSE(t[18]->isRoot());
    EXPECT_TRUE(t[18]->isLinked());
    EXPECT_FALSE(t[23]->isRoot());
    EXPECT_TRUE(t[23]->isLinked());

    // REMOVE 24
    //                               16
    //                       /               `
    //               8                              25
    //           /        `                      /       `
    //       4              12              20              28
    //     /    `         /    `          /    `          /    `
    //   2       6      10      14      18      22      26      30
    //  / `     / `     / `     / `     / `     / `       `     / `
    // 1   3   5   7   9  11  13  15  17  19  21  23      27  29  31
    std::puts("REMOVE 24");
    EXPECT_TRUE(checkLinkage<N>(t[24], t[16], {t[20], t[28]}, 00));
    tr.remove(t[24]);
    EXPECT_EQ(nullptr, t[24]->getParentNode());  // Ensure everything has been reset.
    EXPECT_EQ(nullptr, t[24]->getChildNode(false));
    EXPECT_EQ(nullptr, t[24]->getChildNode(true));
    EXPECT_EQ(0, t[24]->getBalanceFactor());
    EXPECT_EQ(t[16], static_cast<N*>(tr));
    EXPECT_TRUE(checkLinkage<N>(t[25], t[16], {t[20], t[28]}, 00));
    EXPECT_TRUE(checkLinkage<N>(t[26], t[28], {Zzzzz, t[27]}, +1));
    EXPECT_EQ(nullptr, findBrokenBalanceFactor<N>(tr));
    EXPECT_EQ(nullptr, findBrokenAncestry<N>(tr));
    EXPECT_EQ(30, checkOrdering<N>(tr));
    EXPECT_TRUE(t[16]->isRoot());
    EXPECT_FALSE(t[24]->isRoot());
    EXPECT_FALSE(t[24]->isLinked());
    checkPostOrdering<N>(tr, {1,  3,  2,  5,  7,  6,  4,  9,  11, 10, 13, 15, 14, 12, 8,
                              17, 19, 18, 21, 23, 22, 20, 27, 26, 29, 31, 30, 28, 25, 16});

    // REMOVE 25
    //                               16
    //                       /               `
    //               8                              26
    //           /        `                      /       `
    //       4              12              20              28
    //     /    `         /    `          /    `          /    `
    //   2       6      10      14      18      22      27      30
    //  / `     / `     / `     / `     / `     / `             / `
    // 1   3   5   7   9  11  13  15  17  19  21  23          29  31
    std::puts("REMOVE 25");
    EXPECT_TRUE(checkLinkage<N>(t[25], t[16], {t[20], t[28]}, 00));
    tr.remove(t[25]);
    EXPECT_EQ(t[16], static_cast<N*>(tr));
    EXPECT_TRUE(checkLinkage<N>(t[26], t[16], {t[20], t[28]}, 00));
    EXPECT_TRUE(checkLinkage<N>(t[28], t[26], {t[27], t[30]}, +1));
    EXPECT_EQ(nullptr, findBrokenBalanceFactor<N>(tr));
    EXPECT_EQ(nullptr, findBrokenAncestry<N>(tr));
    EXPECT_EQ(29, checkOrdering<N>(tr));
    EXPECT_TRUE(t[16]->isRoot());
    EXPECT_FALSE(t[25]->isRoot());
    EXPECT_FALSE(t[25]->isLinked());
    checkPostOrdering<N>(tr, {1,  3,  2,  5,  7,  6,  4,  9,  11, 10, 13, 15, 14, 12, 8,
                              17, 19, 18, 21, 23, 22, 20, 27, 29, 31, 30, 28, 26, 16});

    // REMOVE 26
    //                               16
    //                       /               `
    //               8                              27
    //           /        `                      /       `
    //       4              12              20              30
    //     /    `         /    `          /    `          /    `
    //   2       6      10      14      18      22      28      31
    //  / `     / `     / `     / `     / `     / `       `
    // 1   3   5   7   9  11  13  15  17  19  21  23      29
    std::puts("REMOVE 26");
    EXPECT_TRUE(checkLinkage<N>(t[26], t[16], {t[20], t[28]}, 00));
    tr.remove(t[26]);
    EXPECT_EQ(t[16], static_cast<N*>(tr));
    EXPECT_TRUE(checkLinkage<N>(t[27], t[16], {t[20], t[30]}, 00));
    EXPECT_TRUE(checkLinkage<N>(t[30], t[27], {t[28], t[31]}, -1));
    EXPECT_TRUE(checkLinkage<N>(t[28], t[30], {Zzzzz, t[29]}, +1));
    EXPECT_EQ(nullptr, findBrokenBalanceFactor<N>(tr));
    EXPECT_EQ(nullptr, findBrokenAncestry<N>(tr));
    EXPECT_EQ(28, checkOrdering<N>(tr));
    EXPECT_TRUE(t[16]->isRoot());
    EXPECT_FALSE(t[26]->isRoot());
    EXPECT_FALSE(t[26]->isLinked());
    checkPostOrdering<N>(tr, {1, 3,  2,  5,  7,  6,  4,  9,  11, 10, 13, 15, 14, 12,
                              8, 17, 19, 18, 21, 23, 22, 20, 29, 28, 31, 30, 27, 16});

    // REMOVE 20
    //                               16
    //                       /               `
    //               8                              27
    //           /        `                      /       `
    //       4              12              21              30
    //     /    `         /    `          /    `          /    `
    //   2       6      10      14      18      22      28      31
    //  / `     / `     / `     / `     / `       `       `
    // 1   3   5   7   9  11  13  15  17  19      23      29
    std::puts("REMOVE 20");
    EXPECT_TRUE(checkLinkage<N>(t[20], t[27], {t[18], t[22]}, 00));
    tr.remove(t[20]);
    EXPECT_EQ(t[16], static_cast<N*>(tr));
    EXPECT_TRUE(checkLinkage<N>(t[21], t[27], {t[18], t[22]}, 00));
    EXPECT_TRUE(checkLinkage<N>(t[22], t[21], {Zzzzz, t[23]}, +1));
    EXPECT_EQ(nullptr, findBrokenBalanceFactor<N>(tr));
    EXPECT_EQ(nullptr, findBrokenAncestry<N>(tr));
    EXPECT_EQ(27, checkOrdering<N>(tr));
    EXPECT_TRUE(t[16]->isRoot());
    EXPECT_FALSE(t[20]->isRoot());
    EXPECT_FALSE(t[20]->isLinked());
    checkPostOrdering<N>(tr, {1, 3,  2,  5,  7,  6,  4,  9,  11, 10, 13, 15, 14, 12,
                              8, 17, 19, 18, 23, 22, 21, 29, 28, 31, 30, 27, 16});

    // REMOVE 27
    //                               16
    //                       /               `
    //               8                              28
    //           /        `                      /       `
    //       4              12              21              30
    //     /    `         /    `          /    `          /    `
    //   2       6      10      14      18      22      29      31
    //  / `     / `     / `     / `     / `       `
    // 1   3   5   7   9  11  13  15  17  19      23
    std::puts("REMOVE 27");
    EXPECT_TRUE(checkLinkage<N>(t[27], t[16], {t[21], t[30]}, 00));
    tr.remove(t[27]);
    EXPECT_EQ(t[16], static_cast<N*>(tr));
    EXPECT_TRUE(checkLinkage<N>(t[28], t[16], {t[21], t[30]}, -1));
    EXPECT_TRUE(checkLinkage<N>(t[30], t[28], {t[29], t[31]}, 00));
    EXPECT_EQ(nullptr, findBrokenBalanceFactor<N>(tr));
    EXPECT_EQ(nullptr, findBrokenAncestry<N>(tr));
    EXPECT_EQ(26, checkOrdering<N>(tr));
    EXPECT_TRUE(t[16]->isRoot());
    EXPECT_FALSE(t[27]->isRoot());
    EXPECT_FALSE(t[27]->isLinked());
    checkPostOrdering<N>(tr, {1,  3, 2,  5,  7,  6,  4,  9,  11, 10, 13, 15, 14,
                              12, 8, 17, 19, 18, 23, 22, 21, 29, 31, 30, 28, 16});

    // REMOVE 28
    //                               16
    //                       /               `
    //               8                              29
    //           /        `                      /       `
    //       4              12              21              30
    //     /    `         /    `          /    `               `
    //   2       6      10      14      18      22              31
    //  / `     / `     / `     / `     / `       `
    // 1   3   5   7   9  11  13  15  17  19      23
    std::puts("REMOVE 28");
    EXPECT_TRUE(checkLinkage<N>(t[28], t[16], {t[21], t[30]}, -1));
    tr.remove(t[28]);
    EXPECT_EQ(t[16], static_cast<N*>(tr));
    EXPECT_TRUE(checkLinkage<N>(t[29], t[16], {t[21], t[30]}, -1));
    EXPECT_TRUE(checkLinkage<N>(t[30], t[29], {Zzzzz, t[31]}, +1));
    EXPECT_EQ(nullptr, findBrokenBalanceFactor<N>(tr));
    EXPECT_EQ(nullptr, findBrokenAncestry<N>(tr));
    EXPECT_EQ(25, checkOrdering<N>(tr));
    EXPECT_TRUE(t[16]->isRoot());
    EXPECT_FALSE(t[28]->isRoot());
    EXPECT_FALSE(t[28]->isLinked());
    checkPostOrdering<N>(tr,
                         {1, 3, 2, 5, 7, 6, 4, 9, 11, 10, 13, 15, 14, 12, 8, 17, 19, 18, 23, 22, 21, 31, 30, 29, 16});

    // REMOVE 29; UNBALANCED TREE BEFORE ROTATION:
    //                               16
    //                       /               `
    //               8                              30
    //           /        `                      /       `
    //       4              12              21              31
    //     /    `         /    `          /    `
    //   2       6      10      14      18      22
    //  / `     / `     / `     / `     / `       `
    // 1   3   5   7   9  11  13  15  17  19      23
    //
    // FINAL STATE AFTER ROTATION:
    //                               16
    //                       /               `
    //               8                              21
    //           /        `                      /       `
    //       4              12              18              30
    //     /    `         /    `          /    `          /    `
    //   2       6      10      14      17      19      22      31
    //  / `     / `     / `     / `                       `
    // 1   3   5   7   9  11  13  15                      23
    std::puts("REMOVE 29");
    EXPECT_TRUE(checkLinkage<N>(t[29], t[16], {t[21], t[30]}, -1));
    tr.remove(t[29]);
    EXPECT_EQ(t[16], static_cast<N*>(tr));
    EXPECT_TRUE(checkLinkage<N>(t[21], t[16], {t[18], t[30]}, +1));
    EXPECT_TRUE(checkLinkage<N>(t[18], t[21], {t[17], t[19]}, 00));
    EXPECT_TRUE(checkLinkage<N>(t[30], t[21], {t[22], t[31]}, -1));
    EXPECT_TRUE(checkLinkage<N>(t[22], t[30], {Zzzzz, t[23]}, +1));
    EXPECT_TRUE(checkLinkage<N>(t[16], Zzzzz, {t[8], t[21]}, 00));
    EXPECT_EQ(nullptr, findBrokenBalanceFactor<N>(tr));
    EXPECT_EQ(nullptr, findBrokenAncestry<N>(tr));
    EXPECT_EQ(24, checkOrdering<N>(tr));
    EXPECT_TRUE(t[16]->isRoot());
    EXPECT_FALSE(t[29]->isRoot());
    EXPECT_FALSE(t[29]->isLinked());
    checkPostOrdering<N>(tr, {1, 3, 2, 5, 7, 6, 4, 9, 11, 10, 13, 15, 14, 12, 8, 17, 19, 18, 23, 22, 31, 30, 21, 16});

    // REMOVE 8
    //                               16
    //                       /               `
    //               9                              21
    //           /        `                      /       `
    //       4              12              18              30
    //     /    `         /    `          /    `          /    `
    //   2       6      10      14      17      19      22      31
    //  / `     / `       `     / `                       `
    // 1   3   5   7      11  13  15                      23
    std::puts("REMOVE 8");
    EXPECT_TRUE(checkLinkage<N>(t[8], t[16], {t[4], t[12]}, 00));
    tr.remove(t[8]);
    EXPECT_EQ(t[16], static_cast<N*>(tr));
    EXPECT_TRUE(checkLinkage<N>(t[9], t[16], {t[4], t[12]}, 00));
    EXPECT_TRUE(checkLinkage<N>(t[10], t[12], {Zzzz, t[11]}, +1));
    EXPECT_EQ(nullptr, findBrokenBalanceFactor<N>(tr));
    EXPECT_EQ(nullptr, findBrokenAncestry<N>(tr));
    EXPECT_EQ(23, checkOrdering<N>(tr));
    EXPECT_TRUE(t[16]->isRoot());
    EXPECT_FALSE(t[8]->isRoot());
    EXPECT_FALSE(t[8]->isLinked());
    checkPostOrdering<N>(tr, {1, 3, 2, 5, 7, 6, 4, 11, 10, 13, 15, 14, 12, 9, 17, 19, 18, 23, 22, 31, 30, 21, 16});

    // REMOVE 9
    //                               16
    //                       /               `
    //               10                             21
    //           /        `                      /       `
    //       4              12              18              30
    //     /    `         /    `          /    `          /    `
    //   2       6      11      14      17      19      22      31
    //  / `     / `             / `                       `
    // 1   3   5   7          13  15                      23
    std::puts("REMOVE 9");
    EXPECT_TRUE(checkLinkage<N>(t[9], t[16], {t[4], t[12]}, 00));
    tr.remove(t[9]);
    EXPECT_EQ(t[16], static_cast<N*>(tr));
    EXPECT_TRUE(checkLinkage<N>(t[10], t[16], {t[4], t[12]}, 00));
    EXPECT_TRUE(checkLinkage<N>(t[12], t[10], {t[11], t[14]}, +1));
    EXPECT_EQ(nullptr, findBrokenBalanceFactor<N>(tr));
    EXPECT_EQ(nullptr, findBrokenAncestry<N>(tr));
    EXPECT_EQ(22, checkOrdering<N>(tr));
    EXPECT_TRUE(t[16]->isRoot());
    EXPECT_FALSE(t[9]->isRoot());
    EXPECT_FALSE(t[9]->isLinked());
    checkPostOrdering<N>(tr, {1, 3, 2, 5, 7, 6, 4, 11, 13, 15, 14, 12, 10, 17, 19, 18, 23, 22, 31, 30, 21, 16});

    // REMOVE 1
    //                               16
    //                       /               `
    //               10                             21
    //           /        `                      /       `
    //       4              12              18              30
    //     /    `         /    `          /    `          /    `
    //   2       6      11      14      17      19      22      31
    //    `     / `             / `                       `
    //     3   5   7          13  15                      23
    std::puts("REMOVE 1");
    EXPECT_TRUE(checkLinkage<N>(t[1], t[2], {Zzzz, Zzzz}, 00));
    tr.remove(t[1]);
    EXPECT_EQ(t[16], static_cast<N*>(tr));
    EXPECT_TRUE(checkLinkage<N>(t[2], t[4], {Zzzz, t[3]}, +1));
    EXPECT_EQ(nullptr, findBrokenBalanceFactor<N>(tr));
    EXPECT_EQ(nullptr, findBrokenAncestry<N>(tr));
    EXPECT_EQ(21, checkOrdering<N>(tr));
    EXPECT_TRUE(t[16]->isRoot());
    EXPECT_FALSE(t[1]->isRoot());
    EXPECT_FALSE(t[1]->isLinked());
    checkPostOrdering<N>(tr, {3, 2, 5, 7, 6, 4, 11, 13, 15, 14, 12, 10, 17, 19, 18, 23, 22, 31, 30, 21, 16});

    // REMOVE 16, the tree got new root.
    //                               17
    //                       /               `
    //               10                             21
    //           /        `                      /       `
    //       4              12              18              30
    //     /    `         /    `               `          /    `
    //   2       6      11      14              19      22      31
    //    `     / `             / `                       `
    //     3   5   7          13  15                      23
    std::puts("REMOVE 16");
    EXPECT_TRUE(checkLinkage<N>(t[16], Zzzzz, {t[10], t[21]}, 00));
    tr.remove(t[16]);
    EXPECT_EQ(nullptr, t[16]->getParentNode());  // Ensure everything has been reset after removal.
    EXPECT_EQ(nullptr, t[16]->getChildNode(false));
    EXPECT_EQ(nullptr, t[16]->getChildNode(true));
    EXPECT_EQ(0, t[16]->getBalanceFactor());
    EXPECT_EQ(t[17], static_cast<N*>(tr));  // This is the new root now.
    EXPECT_TRUE(checkLinkage<N>(t[17], Zzzzz, {t[10], t[21]}, 00));
    EXPECT_EQ(nullptr, findBrokenBalanceFactor<N>(tr));
    EXPECT_EQ(nullptr, findBrokenAncestry<N>(tr));
    EXPECT_EQ(20, checkOrdering<N>(tr));
    EXPECT_TRUE(t[17]->isRoot());
    EXPECT_FALSE(t[16]->isRoot());
    EXPECT_FALSE(t[16]->isLinked());
    checkPostOrdering<N>(tr, {3, 2, 5, 7, 6, 4, 11, 13, 15, 14, 12, 10, 19, 18, 23, 22, 31, 30, 21, 17});

    // REMOVE 22, only has one child.
    //                               17
    //                       /               `
    //               10                             21
    //           /        `                      /       `
    //       4              12              18              30
    //     /    `         /    `               `          /    `
    //   2       6      11      14              19      23      31
    //    `     / `             / `
    //     3   5   7          13  15
    std::puts("REMOVE 22");
    EXPECT_TRUE(checkLinkage<N>(t[22], t[30], {Zzzzz, t[23]}, +1));
    tr.remove(t[22]);
    EXPECT_EQ(t[17], static_cast<N*>(tr));  // Same root.
    EXPECT_TRUE(checkLinkage<N>(t[30], t[21], {t[23], t[31]}, 00));
    EXPECT_TRUE(checkLinkage<N>(t[23], t[30], {Zzzzz, Zzzzz}, 00));
    EXPECT_EQ(nullptr, findBrokenBalanceFactor<N>(tr));
    EXPECT_EQ(nullptr, findBrokenAncestry<N>(tr));
    EXPECT_EQ(19, checkOrdering<N>(tr));
    EXPECT_TRUE(t[17]->isRoot());
    EXPECT_FALSE(t[22]->isRoot());
    EXPECT_FALSE(t[22]->isLinked());
    checkPostOrdering<N>(tr, {3, 2, 5, 7, 6, 4, 11, 13, 15, 14, 12, 10, 19, 18, 23, 31, 30, 21, 17});

    // Print intermediate state for inspection. Be sure to compare it against the above diagram for extra paranoia.
    std::cout << toGraphviz(tr) << std::endl;
    EXPECT_TRUE(checkLinkage<N>(t[17], Zzzzz, {t[10], t[21]}, -1));
    EXPECT_TRUE(checkLinkage<N>(t[10], t[17], {t[+4], t[12]}, 00));
    EXPECT_TRUE(checkLinkage<N>(t[21], t[17], {t[18], t[30]}, 00));
    EXPECT_TRUE(checkLinkage<N>(t[+4], t[10], {t[+2], t[+6]}, 00));
    EXPECT_TRUE(checkLinkage<N>(t[12], t[10], {t[11], t[14]}, +1));
    EXPECT_TRUE(checkLinkage<N>(t[18], t[21], {Zzzzz, t[19]}, +1));
    EXPECT_TRUE(checkLinkage<N>(t[30], t[21], {t[23], t[31]}, 00));
    EXPECT_EQ(t.at(2), tr.min());
    EXPECT_EQ(t.at(31), tr.max());
    EXPECT_EQ(t.at(2), static_cast<const TreeType&>(tr).min());
    EXPECT_EQ(t.at(31), static_cast<const TreeType&>(tr).max());
    EXPECT_EQ(t[17], static_cast<N*>(tr));
    EXPECT_EQ(19, tr.size());

    // REMOVE TWO BOTTOM ROWS. Removal is done in a purposefully complex order to enlarge the coverage.
    //                               17
    //                       /               `
    //               10                             21
    //           /        `                      /       `
    //       4              12              18              30
    std::puts("REMOVE TWO BOTTOM ROWS");
    tr.remove(t[15]);
    tr.remove(t[11]);
    tr.remove(t[5]);
    tr.remove(t[6]);
    tr.remove(t[7]);
    tr.remove(t[3]);
    tr.remove(t[2]);
    tr.remove(t[13]);
    tr.remove(t[14]);
    tr.remove(t[19]);
    tr.remove(t[23]);
    tr.remove(t[31]);
    EXPECT_EQ(t[17], static_cast<N*>(tr));  // Same root.
    EXPECT_EQ(nullptr, findBrokenBalanceFactor<N>(tr));
    EXPECT_EQ(nullptr, findBrokenAncestry<N>(tr));
    EXPECT_EQ(7, checkOrdering<N>(tr));
    EXPECT_TRUE(checkLinkage<N>(t[17], Zzzzz, {t[10], t[21]}, 00));
    EXPECT_TRUE(checkLinkage<N>(t[10], t[17], {t[+4], t[12]}, 00));
    EXPECT_TRUE(checkLinkage<N>(t[21], t[17], {t[18], t[30]}, 00));
    EXPECT_TRUE(checkLinkage<N>(t[+4], t[10], {Zzzzz, Zzzzz}, 00));
    EXPECT_TRUE(checkLinkage<N>(t[12], t[10], {Zzzzz, Zzzzz}, 00));
    EXPECT_TRUE(checkLinkage<N>(t[18], t[21], {Zzzzz, Zzzzz}, 00));
    EXPECT_TRUE(checkLinkage<N>(t[30], t[21], {Zzzzz, Zzzzz}, 00));
    EXPECT_EQ(t.at(4), tr.min());
    EXPECT_EQ(t.at(30), tr.max());
    EXPECT_EQ(t.at(4), static_cast<const TreeType&>(tr).min());
    EXPECT_EQ(t.at(30), static_cast<const TreeType&>(tr).max());
    EXPECT_EQ(t[17], static_cast<N*>(tr));
    EXPECT_EQ(7, tr.size());
    EXPECT_TRUE(t[17]->isRoot());
    checkPostOrdering<N>(tr, {4, 12, 10, 18, 30, 21, 17});
    checkPostOrdering<N>(tr, {30, 18, 21, 12, 4, 10, 17}, true);

    // REMOVE 10, 21.
    //                               17
    //                       /               `
    //               12                             30
    //           /                               /
    //       4                              18
    std::puts("REMOVE 10, 21");
    tr.remove(t[10]);
    tr.remove(t[21]);
    EXPECT_EQ(t[17], static_cast<N*>(tr));  // Same root.
    EXPECT_EQ(nullptr, findBrokenBalanceFactor<N>(tr));
    EXPECT_EQ(nullptr, findBrokenAncestry<N>(tr));
    EXPECT_EQ(5, checkOrdering<N>(tr));
    EXPECT_TRUE(checkLinkage<N>(t[17], Zzzzz, {t[12], t[30]}, 00));
    EXPECT_TRUE(checkLinkage<N>(t[12], t[17], {t[+4], Zzzzz}, -1));
    EXPECT_TRUE(checkLinkage<N>(t[30], t[17], {t[18], Zzzzz}, -1));
    EXPECT_TRUE(checkLinkage<N>(t[+4], t[12], {Zzzzz, Zzzzz}, 00));
    EXPECT_TRUE(checkLinkage<N>(t[18], t[30], {Zzzzz, Zzzzz}, 00));
    EXPECT_EQ(t.at(4), tr.min());
    EXPECT_EQ(t.at(30), tr.max());
    EXPECT_EQ(t.at(4), static_cast<const TreeType&>(tr).min());
    EXPECT_EQ(t.at(30), static_cast<const TreeType&>(tr).max());
    EXPECT_EQ(t[17], static_cast<N*>(tr));
    EXPECT_EQ(5, tr.size());
    EXPECT_TRUE(t[17]->isRoot());
    EXPECT_FALSE(t[10]->isRoot());
    EXPECT_FALSE(t[10]->isLinked());
    EXPECT_FALSE(t[21]->isRoot());
    EXPECT_FALSE(t[21]->isLinked());
    checkPostOrdering<N>(tr, {4, 12, 18, 30, 17});
    checkPostOrdering<N>(tr, {18, 30, 4, 12, 17}, true);

    // REMOVE 12, 18.
    //                               17
    //                       /               `
    //                4                             30
    std::puts("REMOVE 12, 18");
    tr.remove(t[12]);
    tr.remove(t[18]);
    EXPECT_EQ(t[17], static_cast<N*>(tr));  // Same root.
    EXPECT_EQ(nullptr, findBrokenBalanceFactor<N>(tr));
    EXPECT_EQ(nullptr, findBrokenAncestry<N>(tr));
    EXPECT_EQ(3, checkOrdering<N>(tr));
    EXPECT_TRUE(checkLinkage<N>(t[17], Zzzzz, {t[+4], t[30]}, 00));
    EXPECT_TRUE(checkLinkage<N>(t[30], t[17], {Zzzzz, Zzzzz}, 00));
    EXPECT_TRUE(checkLinkage<N>(t[+4], t[17], {Zzzzz, Zzzzz}, 00));
    EXPECT_EQ(t.at(4), tr.min());
    EXPECT_EQ(t.at(30), tr.max());
    EXPECT_EQ(t.at(4), static_cast<const TreeType&>(tr).min());
    EXPECT_EQ(t.at(30), static_cast<const TreeType&>(tr).max());
    EXPECT_EQ(t[17], static_cast<N*>(tr));
    EXPECT_EQ(3, tr.size());
    EXPECT_TRUE(t[17]->isRoot());
    EXPECT_FALSE(t[12]->isRoot());
    EXPECT_FALSE(t[12]->isLinked());
    EXPECT_FALSE(t[18]->isRoot());
    EXPECT_FALSE(t[18]->isLinked());
    checkPostOrdering<N>(tr, {4, 30, 17});
    checkPostOrdering<N>(tr, {30, 4, 17}, true);

    // REMOVE 17. 30 is the new root.
    //                               30
    //                       /
    //                4
    std::puts("REMOVE 17");
    tr.remove(t[17]);
    EXPECT_EQ(t[30], static_cast<N*>(tr));
    EXPECT_EQ(nullptr, findBrokenBalanceFactor<N>(tr));
    EXPECT_EQ(nullptr, findBrokenAncestry<N>(tr));
    EXPECT_EQ(2, checkOrdering<N>(tr));
    EXPECT_TRUE(checkLinkage<N>(t[30], Zzzzz, {t[+4], Zzzzz}, -1));
    EXPECT_TRUE(checkLinkage<N>(t[+4], t[30], {Zzzzz, Zzzzz}, 00));
    EXPECT_EQ(t.at(4), tr.min());
    EXPECT_EQ(t.at(30), tr.max());
    EXPECT_EQ(t.at(4), static_cast<const TreeType&>(tr).min());
    EXPECT_EQ(t.at(30), static_cast<const TreeType&>(tr).max());
    EXPECT_EQ(t[30], static_cast<N*>(tr));
    EXPECT_EQ(2, tr.size());
    EXPECT_TRUE(t[30]->isRoot());
    EXPECT_FALSE(t[17]->isRoot());
    EXPECT_FALSE(t[17]->isLinked());
    checkPostOrdering<N>(tr, {4, 30});
    checkPostOrdering<N>(tr, {4, 30}, true);

    // REMOVE 30. 4 is the only node left.
    //                               4
    std::puts("REMOVE 30");
    tr.remove(t[30]);
    EXPECT_EQ(t[+4], static_cast<N*>(tr));
    EXPECT_EQ(nullptr, findBrokenBalanceFactor<N>(tr));
    EXPECT_EQ(nullptr, findBrokenAncestry<N>(tr));
    EXPECT_EQ(1, checkOrdering<N>(tr));
    EXPECT_TRUE(checkLinkage<N>(t[+4], Zzzzz, {Zzzzz, Zzzzz}, 00));
    EXPECT_EQ(t.at(4), tr.min());
    EXPECT_EQ(t.at(4), tr.max());
    EXPECT_EQ(t.at(4), static_cast<const TreeType&>(tr).min());
    EXPECT_EQ(t.at(4), static_cast<const TreeType&>(tr).max());
    EXPECT_EQ(t[4], static_cast<N*>(tr));
    EXPECT_EQ(1, tr.size());
    EXPECT_TRUE(t[4]->isRoot());
    EXPECT_FALSE(t[30]->isRoot());
    EXPECT_FALSE(t[30]->isLinked());
    checkPostOrdering<N>(tr, {4});
    checkPostOrdering<N>(tr, {4}, true);

    // Check the move assignment and move constructor of the tree.
    TreeType tr2(std::move(tr));
    EXPECT_EQ(t.at(4), static_cast<N*>(tr2));  // Moved.
    EXPECT_EQ(nullptr, static_cast<N*>(tr));   // NOLINT use after move is intentional.
    TreeType tr3;
    EXPECT_EQ(nullptr, static_cast<N*>(tr3));
    tr3 = std::move(tr2);
    EXPECT_EQ(t.at(4), static_cast<N*>(tr3));  // Moved.
    EXPECT_EQ(nullptr, static_cast<N*>(tr2));  // NOLINT use after move is intentional.
    EXPECT_EQ(1, tr3.size());
    EXPECT_TRUE(t[4]->isRoot());

    // Try various methods on empty tree (including `const` one).
    //
    std::puts("REMOVE 4");
    tr3.remove(t[4]);
    tr3.remove(nullptr);
    EXPECT_EQ(nullptr, tr3.min());
    EXPECT_EQ(nullptr, tr3.max());
    const TreeType tr4_const{std::move(tr3)};
    EXPECT_EQ(0, tr4_const.size());
    EXPECT_EQ(nullptr, tr4_const.min());
    EXPECT_EQ(nullptr, tr4_const.max());
    EXPECT_EQ(0, tr4_const.traverseInOrder([](const N&) { return 13; }));
    EXPECT_FALSE(t[4]->isRoot());
    EXPECT_FALSE(t[4]->isLinked());
    checkPostOrdering<N>(tr4_const, {});
    checkPostOrdering<N>(tr4_const, {}, true);

    // Clean up manually to reduce boilerplate in the tests. This is super sloppy but OK for a basic test suite.
    for (auto* const x : t)
    {
        delete x;  // NOLINT
    }
}

TEST(TestCavl, randomized)
{
    std::array<std::shared_ptr<My>, 256> t{};
    for (std::uint8_t i = 0U; i < 255U; i++)
    {
        t.at(i) = std::make_shared<My>(i);
    }
    std::array<bool, 256> mask{};
    std::size_t           size = 0;
    typename My::TreeType root;
    std::uint64_t         cnt_addition = 0;
    std::uint64_t         cnt_removal  = 0;

    const auto validate = [&] {
        EXPECT_EQ(size, std::accumulate(mask.begin(), mask.end(), 0U, [](const std::size_t a, const std::size_t b) {
                      return a + b;
                  }));
        EXPECT_EQ(nullptr, findBrokenBalanceFactor<My>(root));
        EXPECT_EQ(nullptr, findBrokenAncestry<My>(root));
        EXPECT_EQ(size, checkOrdering<My>(root));
        std::array<bool, 256> new_mask{};
        root.traverseInOrder([&](const My& node) { new_mask.at(node.getValue()) = true; });
        EXPECT_EQ(mask, new_mask);  // Otherwise, the contents of the tree does not match our expectations.
    };
    validate();

    const auto add = [&](const std::uint8_t x) {
        const auto predicate = [&](const My& v) { return x - v.getValue(); };
        if (My* const existing = root.search(predicate))
        {
            EXPECT_TRUE(mask.at(x));
            EXPECT_EQ(x, existing->getValue());
            auto result = root.search(predicate, []() -> My* {
                EXPECT_FALSE(true) << "Attempted to create a new node when there is one already";
                return nullptr;
            });
            EXPECT_EQ(x, std::get<0>(result)->getValue());
            EXPECT_TRUE(std::get<1>(result));
        }
        else
        {
            EXPECT_FALSE(mask.at(x));
            bool factory_called = false;
            auto result         = root.search(predicate, [&]() -> My* {
                factory_called = true;
                return t.at(x).get();
            });
            EXPECT_EQ(x, std::get<0>(result)->getValue());
            EXPECT_FALSE(std::get<1>(result));
            EXPECT_TRUE(factory_called);
            size++;
            cnt_addition++;
            mask.at(x) = true;
        }
    };

    const auto drop = [&](const std::uint8_t x) {
        const auto predicate = [&](const My& v) { return x - v.getValue(); };
        if (My* const existing = root.search(predicate))
        {
            EXPECT_TRUE(mask.at(x));
            EXPECT_EQ(x, existing->getValue());
            root.remove(existing);
            size--;
            cnt_removal++;
            mask.at(x) = false;
            EXPECT_EQ(nullptr, root.search(predicate));
        }
        else
        {
            EXPECT_FALSE(mask.at(x));
        }
    };

    std::puts("Running the randomized test...");
    for (std::uint32_t iteration = 0U; iteration < 100'000U; iteration++)
    {
        if ((getRandomByte() % 2U) != 0)
        {
            add(getRandomByte());
        }
        else
        {
            drop(getRandomByte());
        }
        validate();
    }

    std::cout << "Final state:" << std::endl;
    std::cout << "size=" << size << ", cnt_addition=" << cnt_addition << ", cnt_removal=" << cnt_removal << std::endl;
    if (root != nullptr)
    {
        std::cout << "min/max: " << root.min()->getValue() << "/" << root.max()->getValue() << std::endl;
    }
    std::cout << toGraphviz(root) << std::endl;
    validate();
}

TEST(TestCavl, manualMy)
{
    testManual<My>(
        [](const std::uint16_t x) {
            return new My(x);  // NOLINT
        },
        [](My* const old_node) {
            const auto value    = old_node->getValue();
            My* const  new_node = new My(std::move(*old_node));  // NOLINT(*-owning-memory)
            EXPECT_EQ(value, new_node->getValue());
            delete old_node;  // NOLINT(*-owning-memory)
            return new_node;
        });
}

/// Ensure that polymorphic types can be used with the tree. The tree node type itself is not polymorphic!
class V : public cavl::Node<V>
{
public:
    using Self = cavl::Node<V>;
    using Self::isLinked;
    using Self::isRoot;
    using Self::getChildNode;
    using Self::getParentNode;
    using Self::getBalanceFactor;
    using Self::search;
    using Self::remove;
    using Self::traverseInOrder;
    using Self::traversePostOrder;
    using Self::min;
    using Self::max;

    V()                    = default;
    virtual ~V()           = default;
    V(const V&)            = delete;
    V& operator=(const V&) = delete;

    V& operator=(V&&) noexcept = default;
    V(V&&) noexcept            = default;

    NODISCARD virtual V*   clone()                           = 0;
    NODISCARD virtual auto getValue() const -> std::uint16_t = 0;

private:
    using E = struct
    {};
    UNUSED E root_ptr;
    UNUSED E up;
    UNUSED E lr;
    UNUSED E bf;
};
using VTree = cavl::Tree<V>;
static_assert(std::is_same<V::TreeType, VTree>::value, "");
static_assert(std::is_same<cavl::Node<V>, VTree::NodeType>::value, "");

// Dummy polymorphism for testing purposes.
template <std::uint8_t Value>
class VValue : public VValue<static_cast<std::uint8_t>(Value - 1)>
{
public:
    NODISCARD V* clone() override
    {
        return new VValue(std::move(*this));  // NOLINT(*-owning-memory)
    }
    NODISCARD auto getValue() const -> std::uint16_t override
    {
        return static_cast<std::uint16_t>(VValue<static_cast<std::uint8_t>(Value - 1)>::getValue() + 1);
    }
};
template <>
class VValue<0> : public V
{
public:
    NODISCARD V* clone() override
    {
        return new VValue(std::move(*this));  // NOLINT(*-owning-memory)
    }
    NODISCARD auto getValue() const -> std::uint16_t override
    {
        return 0;
    }
};

template <std::uint8_t Candidate, std::uint8_t Limit, std::enable_if_t<(Candidate >= Limit), int> = 0>
auto makeV_impl(const std::uint8_t val) -> V*
{
    if (val == Candidate)
    {
        return new VValue<Candidate>();  // NOLINT(*-owning-memory)
    }
    return nullptr;
}

template <std::uint8_t Candidate, std::uint8_t Limit, std::enable_if_t<(Candidate < Limit), int> = 0>
auto makeV_impl(const std::uint8_t val) -> V*
{
    if (val == Candidate)
    {
        return new VValue<Candidate>();  // NOLINT(*-owning-memory)
    }
    return makeV_impl<Candidate + 1, Limit>(val);
}

template <std::uint8_t Candidate = 0>
auto makeV(const std::uint8_t val) -> V*
{
    return makeV_impl<Candidate, std::numeric_limits<std::uint8_t>::max()>(val);
}

TEST(TestCavl, manualV)
{
    testManual<V>(&makeV<>, [](V* const old_node) {  //
        auto* const new_node = old_node->clone();
        delete old_node;  // NOLINT(*-owning-memory)
        return new_node;
    });
}

}  // namespace
