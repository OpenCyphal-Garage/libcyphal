/// Copyright (c) 2021 Pavel Kirienko <pavel@uavcan.org>

#include "cavl.hpp"
#include <unity.h>
#include <algorithm>
#include <array>
#include <cstdio>
#include <cstdint>
#include <cstdlib>
#include <ctime>
#include <string>
#include <iostream>
#include <sstream>
#include <memory>
#include <numeric>
#include <limits>
#include <vector>
#include <type_traits>
#include <functional>

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

void setUp() {}

void tearDown() {}

namespace
{
/// These aliases are introduced to keep things nicely aligned in test cases.
constexpr auto Zzzz  = nullptr;
constexpr auto Zzzzz = nullptr;

/// Simple test -- fully static type with private inheritance.
class My : public cavl::Node<My>
{
public:
    explicit My(const std::uint16_t v) : value(v) {}
    using Self = cavl::Node<My>;
    using Self::getChildNode;
    using Self::getParentNode;
    using Self::getBalanceFactor;
    using Self::search;
    using Self::remove;
    using Self::traverse;
    using Self::min;
    using Self::max;

    NODISCARD auto getValue() const -> std::uint16_t { return value; }

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
NODISCARD auto getHeight(const N<T>* const n) -> std::int8_t
{
    return (n != nullptr) ? static_cast<std::int8_t>(1 + std::max(getHeight<T>(n->getChildNode(false)),  //
                                                                  getHeight<T>(n->getChildNode(true))))
                          : 0;
}

/// Returns the size if the tree is ordered correctly, otherwise SIZE_MAX.
template <typename T>
NODISCARD std::size_t checkOrdering(const N<T>* const root)
{
    const N<T>* prev  = nullptr;
    bool        valid = true;
    std::size_t size  = 0;
    T::traverse(root, [&](const N<T>& nd) {
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
NODISCARD const N<T>* findBrokenBalanceFactor(const N<T>* const n)
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
    tr.traverse([&](const typename cavl::Tree<T>::DerivedType& x) {
        const char* const fill_color =  // NOLINTNEXTLINE(*-avoid-nested-conditional-operator)
            (x.getBalanceFactor() == 0) ? "black" : ((x.getBalanceFactor() > 0) ? "orange" : "blue");
        ss << x.getValue() << "[fillcolor=" << fill_color << "];";
    });
    ss << "\n";
    tr.traverse([&](const typename cavl::Tree<T>::DerivedType& x) {
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
void testManual(const std::function<N*(std::uint8_t)>& factory)
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
    TEST_ASSERT(tr.empty());
    const auto insert = [&](const std::uint8_t i) {
        std::cout << "Inserting " << static_cast<int>(i) << std::endl;
        const auto pred = [&](const N& v) { return t.at(i)->getValue() - v.getValue(); };
        TEST_ASSERT_NULL(tr.search(pred));
        TEST_ASSERT_NULL(static_cast<const TreeType&>(tr).search(pred));
        TEST_ASSERT_EQUAL(t[i], tr.search(pred, [&]() { return t[i]; }));
        TEST_ASSERT_EQUAL(t[i], tr.search(pred));
        TEST_ASSERT_EQUAL(t[i], static_cast<const TreeType&>(tr).search(pred));
        // Validate the tree after every mutation.
        TEST_ASSERT(!tr.empty());
        TEST_ASSERT_NULL(findBrokenBalanceFactor<N>(tr));
        TEST_ASSERT_NULL(findBrokenAncestry<N>(tr));
        TEST_ASSERT_TRUE(checkOrdering<N>(tr) < std::numeric_limits<std::size_t>::max());
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
    TEST_ASSERT_EQUAL(31, tr.size());
    TEST_ASSERT_EQUAL(31, checkOrdering<N>(tr));
    std::cout << toGraphviz(tr) << std::endl;
    TEST_ASSERT_NULL(findBrokenBalanceFactor<N>(tr));
    TEST_ASSERT_NULL(findBrokenAncestry<N>(tr));
    TEST_ASSERT_EQUAL(31, checkOrdering<N>(tr));
    // Check composition -- ensure that every element is in the tree and it is there exactly once.
    {
        bool seen[32]{};
        tr.traverse([&](const N& n) {
            TEST_ASSERT_FALSE(seen[n.getValue()]);
            seen[n.getValue()] = true;
        });
        TEST_ASSERT(std::all_of(&seen[1], &seen[31], [](bool x) { return x; }));
    }
    TEST_ASSERT_EQUAL(t.at(1), tr.min());
    TEST_ASSERT_EQUAL(t.at(31), tr.max());
    TEST_ASSERT_EQUAL(t.at(1), static_cast<const TreeType&>(tr).min());
    TEST_ASSERT_EQUAL(t.at(31), static_cast<const TreeType&>(tr).max());
    // Check index operator, both const and mutable.
    TEST_ASSERT_EQUAL_INT64(10, tr[9]->getValue());
    TEST_ASSERT_EQUAL_INT64(10, static_cast<const TreeType&>(tr)[9]->getValue());
    TEST_ASSERT_NULL(tr[32]);
    TEST_ASSERT_NULL(static_cast<const TreeType&>(tr)[100500UL]);
    for (auto i = 1U; i <= 31; i++)
    {
        TEST_ASSERT_NOT_NULL(tr[i - 1]);
        TEST_ASSERT_EQUAL_INT64(i, tr[i - 1]->getValue());
        TEST_ASSERT_EQUAL_INT64(i, static_cast<const TreeType&>(tr)[i - 1]->getValue());
    }

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
    TEST_ASSERT(checkLinkage<N>(t[24], t[16], {t[20], t[28]}, 00));
    tr.remove(t[24]);
    TEST_ASSERT_NULL(t[24]->getParentNode());  // Ensure everything has been reset.
    TEST_ASSERT_NULL(t[24]->getChildNode(false));
    TEST_ASSERT_NULL(t[24]->getChildNode(true));
    TEST_ASSERT_EQUAL(0, t[24]->getBalanceFactor());
    TEST_ASSERT_EQUAL(t[16], static_cast<N*>(tr));
    TEST_ASSERT(checkLinkage<N>(t[25], t[16], {t[20], t[28]}, 00));
    TEST_ASSERT(checkLinkage<N>(t[26], t[28], {Zzzzz, t[27]}, +1));
    TEST_ASSERT_NULL(findBrokenBalanceFactor<N>(tr));
    TEST_ASSERT_NULL(findBrokenAncestry<N>(tr));
    TEST_ASSERT_EQUAL(30, checkOrdering<N>(tr));

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
    TEST_ASSERT(checkLinkage<N>(t[25], t[16], {t[20], t[28]}, 00));
    tr.remove(t[25]);
    TEST_ASSERT_EQUAL(t[16], static_cast<N*>(tr));
    TEST_ASSERT(checkLinkage<N>(t[26], t[16], {t[20], t[28]}, 00));
    TEST_ASSERT(checkLinkage<N>(t[28], t[26], {t[27], t[30]}, +1));
    TEST_ASSERT_NULL(findBrokenBalanceFactor<N>(tr));
    TEST_ASSERT_NULL(findBrokenAncestry<N>(tr));
    TEST_ASSERT_EQUAL(29, checkOrdering<N>(tr));

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
    TEST_ASSERT(checkLinkage<N>(t[26], t[16], {t[20], t[28]}, 00));
    tr.remove(t[26]);
    TEST_ASSERT_EQUAL(t[16], static_cast<N*>(tr));
    TEST_ASSERT(checkLinkage<N>(t[27], t[16], {t[20], t[30]}, 00));
    TEST_ASSERT(checkLinkage<N>(t[30], t[27], {t[28], t[31]}, -1));
    TEST_ASSERT(checkLinkage<N>(t[28], t[30], {Zzzzz, t[29]}, +1));
    TEST_ASSERT_NULL(findBrokenBalanceFactor<N>(tr));
    TEST_ASSERT_NULL(findBrokenAncestry<N>(tr));
    TEST_ASSERT_EQUAL(28, checkOrdering<N>(tr));

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
    TEST_ASSERT(checkLinkage<N>(t[20], t[27], {t[18], t[22]}, 00));
    tr.remove(t[20]);
    TEST_ASSERT_EQUAL(t[16], static_cast<N*>(tr));
    TEST_ASSERT(checkLinkage<N>(t[21], t[27], {t[18], t[22]}, 00));
    TEST_ASSERT(checkLinkage<N>(t[22], t[21], {Zzzzz, t[23]}, +1));
    TEST_ASSERT_NULL(findBrokenBalanceFactor<N>(tr));
    TEST_ASSERT_NULL(findBrokenAncestry<N>(tr));
    TEST_ASSERT_EQUAL(27, checkOrdering<N>(tr));

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
    TEST_ASSERT(checkLinkage<N>(t[27], t[16], {t[21], t[30]}, 00));
    tr.remove(t[27]);
    TEST_ASSERT_EQUAL(t[16], static_cast<N*>(tr));
    TEST_ASSERT(checkLinkage<N>(t[28], t[16], {t[21], t[30]}, -1));
    TEST_ASSERT(checkLinkage<N>(t[30], t[28], {t[29], t[31]}, 00));
    TEST_ASSERT_NULL(findBrokenBalanceFactor<N>(tr));
    TEST_ASSERT_NULL(findBrokenAncestry<N>(tr));
    TEST_ASSERT_EQUAL(26, checkOrdering<N>(tr));

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
    TEST_ASSERT(checkLinkage<N>(t[28], t[16], {t[21], t[30]}, -1));
    tr.remove(t[28]);
    TEST_ASSERT_EQUAL(t[16], static_cast<N*>(tr));
    TEST_ASSERT(checkLinkage<N>(t[29], t[16], {t[21], t[30]}, -1));
    TEST_ASSERT(checkLinkage<N>(t[30], t[29], {Zzzzz, t[31]}, +1));
    TEST_ASSERT_NULL(findBrokenBalanceFactor<N>(tr));
    TEST_ASSERT_NULL(findBrokenAncestry<N>(tr));
    TEST_ASSERT_EQUAL(25, checkOrdering<N>(tr));

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
    TEST_ASSERT(checkLinkage<N>(t[29], t[16], {t[21], t[30]}, -1));
    tr.remove(t[29]);
    TEST_ASSERT_EQUAL(t[16], static_cast<N*>(tr));
    TEST_ASSERT(checkLinkage<N>(t[21], t[16], {t[18], t[30]}, +1));
    TEST_ASSERT(checkLinkage<N>(t[18], t[21], {t[17], t[19]}, 00));
    TEST_ASSERT(checkLinkage<N>(t[30], t[21], {t[22], t[31]}, -1));
    TEST_ASSERT(checkLinkage<N>(t[22], t[30], {Zzzzz, t[23]}, +1));
    TEST_ASSERT(checkLinkage<N>(t[16], Zzzzz, {t[8], t[21]}, 00));
    TEST_ASSERT_NULL(findBrokenBalanceFactor<N>(tr));
    TEST_ASSERT_NULL(findBrokenAncestry<N>(tr));
    TEST_ASSERT_EQUAL(24, checkOrdering<N>(tr));

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
    TEST_ASSERT(checkLinkage<N>(t[8], t[16], {t[4], t[12]}, 00));
    tr.remove(t[8]);
    TEST_ASSERT_EQUAL(t[16], static_cast<N*>(tr));
    TEST_ASSERT(checkLinkage<N>(t[9], t[16], {t[4], t[12]}, 00));
    TEST_ASSERT(checkLinkage<N>(t[10], t[12], {Zzzz, t[11]}, +1));
    TEST_ASSERT_NULL(findBrokenBalanceFactor<N>(tr));
    TEST_ASSERT_NULL(findBrokenAncestry<N>(tr));
    TEST_ASSERT_EQUAL(23, checkOrdering<N>(tr));

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
    TEST_ASSERT(checkLinkage<N>(t[9], t[16], {t[4], t[12]}, 00));
    tr.remove(t[9]);
    TEST_ASSERT_EQUAL(t[16], static_cast<N*>(tr));
    TEST_ASSERT(checkLinkage<N>(t[10], t[16], {t[4], t[12]}, 00));
    TEST_ASSERT(checkLinkage<N>(t[12], t[10], {t[11], t[14]}, +1));
    TEST_ASSERT_NULL(findBrokenBalanceFactor<N>(tr));
    TEST_ASSERT_NULL(findBrokenAncestry<N>(tr));
    TEST_ASSERT_EQUAL(22, checkOrdering<N>(tr));

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
    TEST_ASSERT(checkLinkage<N>(t[1], t[2], {Zzzz, Zzzz}, 00));
    tr.remove(t[1]);
    TEST_ASSERT_EQUAL(t[16], static_cast<N*>(tr));
    TEST_ASSERT(checkLinkage<N>(t[2], t[4], {Zzzz, t[3]}, +1));
    TEST_ASSERT_NULL(findBrokenBalanceFactor<N>(tr));
    TEST_ASSERT_NULL(findBrokenAncestry<N>(tr));
    TEST_ASSERT_EQUAL(21, checkOrdering<N>(tr));

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
    TEST_ASSERT(checkLinkage<N>(t[16], Zzzzz, {t[10], t[21]}, 00));
    tr.remove(t[16]);
    TEST_ASSERT_NULL(t[16]->getParentNode());  // Ensure everything has been reset after removal.
    TEST_ASSERT_NULL(t[16]->getChildNode(false));
    TEST_ASSERT_NULL(t[16]->getChildNode(true));
    TEST_ASSERT_EQUAL(0, t[16]->getBalanceFactor());
    TEST_ASSERT_EQUAL(t[17], static_cast<N*>(tr));  // This is the new root now.
    TEST_ASSERT(checkLinkage<N>(t[17], Zzzzz, {t[10], t[21]}, 00));
    TEST_ASSERT_NULL(findBrokenBalanceFactor<N>(tr));
    TEST_ASSERT_NULL(findBrokenAncestry<N>(tr));
    TEST_ASSERT_EQUAL(20, checkOrdering<N>(tr));

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
    TEST_ASSERT(checkLinkage<N>(t[22], t[30], {Zzzzz, t[23]}, +1));
    tr.remove(t[22]);
    TEST_ASSERT_EQUAL(t[17], static_cast<N*>(tr));  // Same root.
    TEST_ASSERT(checkLinkage<N>(t[30], t[21], {t[23], t[31]}, 00));
    TEST_ASSERT(checkLinkage<N>(t[23], t[30], {Zzzzz, Zzzzz}, 00));
    TEST_ASSERT_NULL(findBrokenBalanceFactor<N>(tr));
    TEST_ASSERT_NULL(findBrokenAncestry<N>(tr));
    TEST_ASSERT_EQUAL(19, checkOrdering<N>(tr));

    // Print intermediate state for inspection. Be sure to compare it against the above diagram for extra paranoia.
    std::cout << toGraphviz(tr) << std::endl;
    TEST_ASSERT(checkLinkage<N>(t[17], Zzzzz, {t[10], t[21]}, -1));
    TEST_ASSERT(checkLinkage<N>(t[10], t[17], {t[+4], t[12]}, 00));
    TEST_ASSERT(checkLinkage<N>(t[21], t[17], {t[18], t[30]}, 00));
    TEST_ASSERT(checkLinkage<N>(t[+4], t[10], {t[+2], t[+6]}, 00));
    TEST_ASSERT(checkLinkage<N>(t[12], t[10], {t[11], t[14]}, +1));
    TEST_ASSERT(checkLinkage<N>(t[18], t[21], {Zzzzz, t[19]}, +1));
    TEST_ASSERT(checkLinkage<N>(t[30], t[21], {t[23], t[31]}, 00));
    TEST_ASSERT_EQUAL(t.at(2), tr.min());
    TEST_ASSERT_EQUAL(t.at(31), tr.max());
    TEST_ASSERT_EQUAL(t.at(2), static_cast<const TreeType&>(tr).min());
    TEST_ASSERT_EQUAL(t.at(31), static_cast<const TreeType&>(tr).max());
    TEST_ASSERT_EQUAL(t[17], static_cast<N*>(tr));
    TEST_ASSERT_EQUAL(19, tr.size());

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
    TEST_ASSERT_EQUAL(t[17], static_cast<N*>(tr));  // Same root.
    TEST_ASSERT_NULL(findBrokenBalanceFactor<N>(tr));
    TEST_ASSERT_NULL(findBrokenAncestry<N>(tr));
    TEST_ASSERT_EQUAL(7, checkOrdering<N>(tr));
    TEST_ASSERT(checkLinkage<N>(t[17], Zzzzz, {t[10], t[21]}, 00));
    TEST_ASSERT(checkLinkage<N>(t[10], t[17], {t[+4], t[12]}, 00));
    TEST_ASSERT(checkLinkage<N>(t[21], t[17], {t[18], t[30]}, 00));
    TEST_ASSERT(checkLinkage<N>(t[+4], t[10], {Zzzzz, Zzzzz}, 00));
    TEST_ASSERT(checkLinkage<N>(t[12], t[10], {Zzzzz, Zzzzz}, 00));
    TEST_ASSERT(checkLinkage<N>(t[18], t[21], {Zzzzz, Zzzzz}, 00));
    TEST_ASSERT(checkLinkage<N>(t[30], t[21], {Zzzzz, Zzzzz}, 00));
    TEST_ASSERT_EQUAL(t.at(4), tr.min());
    TEST_ASSERT_EQUAL(t.at(30), tr.max());
    TEST_ASSERT_EQUAL(t.at(4), static_cast<const TreeType&>(tr).min());
    TEST_ASSERT_EQUAL(t.at(30), static_cast<const TreeType&>(tr).max());
    TEST_ASSERT_EQUAL(t[17], static_cast<N*>(tr));
    TEST_ASSERT_EQUAL(7, tr.size());

    // REMOVE 10, 21.
    //                               17
    //                       /               `
    //               12                             30
    //           /                               /
    //       4                              18
    std::puts("REMOVE 10, 21");
    tr.remove(t[10]);
    tr.remove(t[21]);
    TEST_ASSERT_EQUAL(t[17], static_cast<N*>(tr));  // Same root.
    TEST_ASSERT_NULL(findBrokenBalanceFactor<N>(tr));
    TEST_ASSERT_NULL(findBrokenAncestry<N>(tr));
    TEST_ASSERT_EQUAL(5, checkOrdering<N>(tr));
    TEST_ASSERT(checkLinkage<N>(t[17], Zzzzz, {t[12], t[30]}, 00));
    TEST_ASSERT(checkLinkage<N>(t[12], t[17], {t[+4], Zzzzz}, -1));
    TEST_ASSERT(checkLinkage<N>(t[30], t[17], {t[18], Zzzzz}, -1));
    TEST_ASSERT(checkLinkage<N>(t[+4], t[12], {Zzzzz, Zzzzz}, 00));
    TEST_ASSERT(checkLinkage<N>(t[18], t[30], {Zzzzz, Zzzzz}, 00));
    TEST_ASSERT_EQUAL(t.at(4), tr.min());
    TEST_ASSERT_EQUAL(t.at(30), tr.max());
    TEST_ASSERT_EQUAL(t.at(4), static_cast<const TreeType&>(tr).min());
    TEST_ASSERT_EQUAL(t.at(30), static_cast<const TreeType&>(tr).max());
    TEST_ASSERT_EQUAL(t[17], static_cast<N*>(tr));
    TEST_ASSERT_EQUAL(5, tr.size());

    // REMOVE 12, 18.
    //                               17
    //                       /               `
    //                4                             30
    std::puts("REMOVE 12, 18");
    tr.remove(t[12]);
    tr.remove(t[18]);
    TEST_ASSERT_EQUAL(t[17], static_cast<N*>(tr));  // Same root.
    TEST_ASSERT_NULL(findBrokenBalanceFactor<N>(tr));
    TEST_ASSERT_NULL(findBrokenAncestry<N>(tr));
    TEST_ASSERT_EQUAL(3, checkOrdering<N>(tr));
    TEST_ASSERT(checkLinkage<N>(t[17], Zzzzz, {t[+4], t[30]}, 00));
    TEST_ASSERT(checkLinkage<N>(t[30], t[17], {Zzzzz, Zzzzz}, 00));
    TEST_ASSERT(checkLinkage<N>(t[+4], t[17], {Zzzzz, Zzzzz}, 00));
    TEST_ASSERT_EQUAL(t.at(4), tr.min());
    TEST_ASSERT_EQUAL(t.at(30), tr.max());
    TEST_ASSERT_EQUAL(t.at(4), static_cast<const TreeType&>(tr).min());
    TEST_ASSERT_EQUAL(t.at(30), static_cast<const TreeType&>(tr).max());
    TEST_ASSERT_EQUAL(t[17], static_cast<N*>(tr));
    TEST_ASSERT_EQUAL(3, tr.size());

    // REMOVE 17. 30 is the new root.
    //                               30
    //                       /
    //                4
    std::puts("REMOVE 17");
    tr.remove(t[17]);
    TEST_ASSERT_EQUAL(t[30], static_cast<N*>(tr));
    TEST_ASSERT_NULL(findBrokenBalanceFactor<N>(tr));
    TEST_ASSERT_NULL(findBrokenAncestry<N>(tr));
    TEST_ASSERT_EQUAL(2, checkOrdering<N>(tr));
    TEST_ASSERT(checkLinkage<N>(t[30], Zzzzz, {t[+4], Zzzzz}, -1));
    TEST_ASSERT(checkLinkage<N>(t[+4], t[30], {Zzzzz, Zzzzz}, 00));
    TEST_ASSERT_EQUAL(t.at(4), tr.min());
    TEST_ASSERT_EQUAL(t.at(30), tr.max());
    TEST_ASSERT_EQUAL(t.at(4), static_cast<const TreeType&>(tr).min());
    TEST_ASSERT_EQUAL(t.at(30), static_cast<const TreeType&>(tr).max());
    TEST_ASSERT_EQUAL(t[30], static_cast<N*>(tr));
    TEST_ASSERT_EQUAL(2, tr.size());

    // REMOVE 30. 4 is the only node left.
    //                               4
    std::puts("REMOVE 30");
    tr.remove(t[30]);
    TEST_ASSERT_EQUAL(t[+4], static_cast<N*>(tr));
    TEST_ASSERT_NULL(findBrokenBalanceFactor<N>(tr));
    TEST_ASSERT_NULL(findBrokenAncestry<N>(tr));
    TEST_ASSERT_EQUAL(1, checkOrdering<N>(tr));
    TEST_ASSERT(checkLinkage<N>(t[+4], Zzzzz, {Zzzzz, Zzzzz}, 00));
    TEST_ASSERT_EQUAL(t.at(4), tr.min());
    TEST_ASSERT_EQUAL(t.at(4), tr.max());
    TEST_ASSERT_EQUAL(t.at(4), static_cast<const TreeType&>(tr).min());
    TEST_ASSERT_EQUAL(t.at(4), static_cast<const TreeType&>(tr).max());
    TEST_ASSERT_EQUAL(t[4], static_cast<N*>(tr));
    TEST_ASSERT_EQUAL(1, tr.size());

    // Check the move assignment and move constructor of the tree.
    TreeType tr2(std::move(tr));
    TEST_ASSERT_EQUAL(t.at(4), static_cast<N*>(tr2));  // Moved.
    TEST_ASSERT_NULL(static_cast<N*>(tr));             // NOLINT use after move is intentional.
    TreeType tr3;
    TEST_ASSERT_NULL(static_cast<N*>(tr3));
    tr3 = std::move(tr2);
    TEST_ASSERT_EQUAL(t.at(4), static_cast<N*>(tr3));  // Moved.
    TEST_ASSERT_NULL(static_cast<N*>(tr2));            // NOLINT use after move is intentional.
    TEST_ASSERT_EQUAL(1, tr3.size());

    // Clean up manually to reduce boilerplate in the tests. This is super sloppy but OK for a basic test suite.
    for (auto* const x : t)
    {
        delete x;  // NOLINT
    }
}

void testRandomized()
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

    const auto validate = [&]() {
        TEST_ASSERT_EQUAL(size,
                          std::accumulate(mask.begin(), mask.end(), 0U, [](const std::size_t a, const std::size_t b) {
                              return a + b;
                          }));
        TEST_ASSERT_NULL(findBrokenBalanceFactor<My>(root));
        TEST_ASSERT_NULL(findBrokenAncestry<My>(root));
        TEST_ASSERT_EQUAL(size, checkOrdering<My>(root));
        std::array<bool, 256> new_mask{};
        root.traverse([&](const My& node) { new_mask.at(node.getValue()) = true; });
        TEST_ASSERT_EQUAL(mask, new_mask);  // Otherwise, the contents of the tree does not match our expectations.
    };
    validate();

    const auto add = [&](const std::uint8_t x) {
        const auto predicate = [&](const My& v) { return x - v.getValue(); };
        if (My* const existing = root.search(predicate))
        {
            TEST_ASSERT_TRUE(mask.at(x));
            TEST_ASSERT_EQUAL(x, existing->getValue());
            TEST_ASSERT_EQUAL(x,
                              root.search(predicate,
                                          []() -> My* {
                                              TEST_FAIL_MESSAGE(
                                                  "Attempted to create a new node when there is one already");
                                              return nullptr;
                                          })
                                  ->getValue());
        }
        else
        {
            TEST_ASSERT_FALSE(mask.at(x));
            bool factory_called = false;
            TEST_ASSERT_EQUAL(x,
                              root.search(predicate,
                                          [&]() -> My* {
                                              factory_called = true;
                                              return t.at(x).get();
                                          })
                                  ->getValue());
            TEST_ASSERT(factory_called);
            size++;
            cnt_addition++;
            mask.at(x) = true;
        }
    };

    const auto drop = [&](const std::uint8_t x) {
        const auto predicate = [&](const My& v) { return x - v.getValue(); };
        if (My* const existing = root.search(predicate))
        {
            TEST_ASSERT_TRUE(mask.at(x));
            TEST_ASSERT_EQUAL(x, existing->getValue());
            root.remove(existing);
            size--;
            cnt_removal++;
            mask.at(x) = false;
            TEST_ASSERT_NULL(root.search(predicate));
        }
        else
        {
            TEST_ASSERT_FALSE(mask.at(x));
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

void testManualMy()
{
    testManual<My>([](const std::uint16_t x) {
        return new My(x);  // NOLINT
    });
}

/// Ensure that polymorphic types can be used with the tree. The tree node type itself is not polymorphic!
class V : public cavl::Node<V>
{
public:
    using Self = cavl::Node<V>;
    using Self::getChildNode;
    using Self::getParentNode;
    using Self::getBalanceFactor;
    using Self::search;
    using Self::remove;
    using Self::traverse;
    using Self::min;
    using Self::max;

    V()                    = default;
    virtual ~V()           = default;
    V(const V&)            = delete;
    V(V&&)                 = delete;
    V& operator=(const V&) = delete;
    V& operator=(V&&)      = delete;

    NODISCARD virtual auto getValue() const -> std::uint16_t = 0;

private:
    using E = struct
    {};
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
    NODISCARD auto getValue() const -> std::uint16_t override
    {
        return static_cast<std::uint16_t>(VValue<static_cast<std::uint8_t>(Value - 1)>::getValue() + 1);
    }
};
template <>
class VValue<0> : public V
{
public:
    NODISCARD auto getValue() const -> std::uint16_t override { return 0; }
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

void testManualV()
{
    testManual<V>(&makeV<>);
}

}  // namespace

int main(const int argc, const char* const argv[])
{
    const auto seed = static_cast<unsigned>((argc > 1) ? std::atoll(argv[1]) : std::time(nullptr));  // NOLINT
    std::cout << "Randomness seed: " << seed << std::endl;
    std::srand(seed);
    // NOLINTBEGIN(misc-include-cleaner)
    UNITY_BEGIN();
    RUN_TEST(testManualMy);
    RUN_TEST(testManualV);
    RUN_TEST(testRandomized);
    return UNITY_END();
    // NOLINTEND(misc-include-cleaner)
}
