/// @copyright
/// Copyright (C) OpenCyphal Development Team  <opencyphal.org>
/// Copyright Amazon.com Inc. or its affiliates.
/// SPDX-License-Identifier: MIT

#include "../../memory_resource_mock.hpp"
#include "../../tracking_memory_resource.hpp"

#include <canard.h>
#include <cetl/pf17/cetlpf.hpp>
#include <libcyphal/transport/can/delegate.hpp>
#include <libcyphal/transport/errors.hpp>
#include <libcyphal/transport/types.hpp>
#include <libcyphal/types.hpp>

#include <gmock/gmock.h>
#include <gtest/gtest.h>

#include <array>
#include <cstddef>
#include <cstdint>
#include <numeric>
#include <string>
#include <utility>
#include <vector>

namespace
{

using libcyphal::TimePoint;
using namespace libcyphal::transport;  // NOLINT This our main concern here in the unit tests.

using testing::_;
using testing::Eq;
using testing::Each;
using testing::Return;
using testing::IsNull;
using testing::IsEmpty;
using testing::Optional;
using testing::StrictMock;
using testing::ElementsAre;
using testing::VariantWith;

// NOLINTBEGIN(cppcoreguidelines-avoid-magic-numbers, readability-magic-numbers)

class TestCanDelegate : public testing::Test
{
protected:
    class TransportDelegateImpl final : public can::detail::TransportDelegate
    {
    public:
        explicit TransportDelegateImpl(cetl::pmr::memory_resource& memory)
            : can::detail::TransportDelegate{memory}
        {
        }

        MOCK_METHOD((cetl::optional<AnyError>),
                    sendTransfer,
                    (const libcyphal::TimePoint    deadline,
                     const CanardTransferMetadata& metadata,
                     const PayloadFragments        payload_fragments),
                    (final));
        // NOLINTNEXTLINE(bugprone-exception-escape)
        MOCK_METHOD(void,
                    triggerUpdateOfFilters,
                    (const bool is_service, const bool is_subscription_added),
                    (noexcept, final));
    };

    void TearDown() override
    {
        EXPECT_THAT(mr_.allocations, IsEmpty());
        EXPECT_EQ(mr_.total_allocated_bytes, mr_.total_deallocated_bytes);
    }

    // MARK: Data members:

    // NOLINTBEGIN
    TrackingMemoryResource mr_;
    // NOLINTEND
};

// MARK: Tests:

TEST_F(TestCanDelegate, CanardMemory_copy)
{
    using CanardMemory = can::detail::TransportDelegate::CanardMemory;

    TransportDelegateImpl delegate{mr_};
    auto&                 canard_instance = delegate.canard_instance();

    char* const payload = static_cast<char*>(canard_instance.memory_allocate(&canard_instance, 8));
    std::iota(payload, payload + 8, '0');  // NOLINT

    const std::size_t  payload_size = 4;
    const CanardMemory canard_memory{delegate, payload, payload_size};
    EXPECT_THAT(canard_memory.size(), payload_size);

    // Ask exactly as payload
    {
        const std::size_t          ask_size = payload_size;
        std::array<char, ask_size> buffer{};

        EXPECT_THAT(canard_memory.copy(0, buffer.data(), ask_size), ask_size);
        EXPECT_THAT(buffer, ElementsAre('0', '1', '2', '3'));
    }

    // Ask more than payload
    {
        const std::size_t                  ask_size = payload_size + 2;
        std::array<std::uint8_t, ask_size> buffer{};

        EXPECT_THAT(canard_memory.copy(0, buffer.data(), ask_size), payload_size);
        EXPECT_THAT(buffer, ElementsAre('0', '1', '2', '3', '\0', '\0'));
    }

    // Ask less than payload (with different offsets)
    {
        const std::size_t                  ask_size = payload_size - 2;
        std::array<std::uint8_t, ask_size> buffer{};

        EXPECT_THAT(canard_memory.copy(0, buffer.data(), ask_size), ask_size);
        EXPECT_THAT(buffer, ElementsAre('0', '1'));

        EXPECT_THAT(canard_memory.copy(3, buffer.data(), buffer.size()), 1);
        EXPECT_THAT(buffer, ElementsAre('3', '1'));

        EXPECT_THAT(canard_memory.copy(2, buffer.data(), ask_size), ask_size);
        EXPECT_THAT(buffer, ElementsAre('2', '3'));

        EXPECT_THAT(canard_memory.copy(payload_size, buffer.data(), ask_size), 0);
        EXPECT_THAT(buffer, ElementsAre('2', '3'));

        // Ask nothing
        EXPECT_THAT(canard_memory.copy(0, buffer.data(), 0), 0);
        EXPECT_THAT(buffer, ElementsAre('2', '3'));

        // No output buffer
        EXPECT_THAT(canard_memory.copy(0, nullptr, 0), 0);
    }
}

TEST_F(TestCanDelegate, CanardMemory_copy_on_moved)
{
    using CanardMemory = can::detail::TransportDelegate::CanardMemory;

    TransportDelegateImpl delegate{mr_};
    auto&                 canard_instance = delegate.canard_instance();

    const std::size_t payload_size = 4;
    char* const       payload = static_cast<char*>(canard_instance.memory_allocate(&canard_instance, payload_size));
    std::iota(payload, payload + payload_size, '0'); // NOLINT

    CanardMemory old_canard_memory{delegate, payload, payload_size};
    EXPECT_THAT(old_canard_memory.size(), payload_size);

    const CanardMemory new_canard_memory{std::move(old_canard_memory)};
    // NOLINTNEXTLINE(clang-analyzer-cplusplus.Move,bugprone-use-after-move,hicpp-invalid-access-moved)
    EXPECT_THAT(old_canard_memory.size(), 0);
    EXPECT_THAT(new_canard_memory.size(), payload_size);

    // Try old one
    {
        std::array<char, payload_size> buffer{};
        // NOLINTNEXTLINE(clang-analyzer-cplusplus.Move)
        EXPECT_THAT(old_canard_memory.copy(0, buffer.data(), buffer.size()), 0);
        EXPECT_THAT(buffer, Each('\0'));
    }

    // Try new one
    {
        std::array<char, payload_size> buffer{};
        EXPECT_THAT(new_canard_memory.copy(0, buffer.data(), buffer.size()), payload_size);
        EXPECT_THAT(buffer, ElementsAre('0', '1', '2', '3'));
    }
}

TEST_F(TestCanDelegate, anyErrorFromCanard)
{
    EXPECT_THAT(can::detail::TransportDelegate::anyErrorFromCanard(-CANARD_ERROR_OUT_OF_MEMORY),
                Optional(VariantWith<MemoryError>(_)));

    EXPECT_THAT(can::detail::TransportDelegate::anyErrorFromCanard(-CANARD_ERROR_INVALID_ARGUMENT),
                Optional(VariantWith<ArgumentError>(_)));

    EXPECT_THAT(can::detail::TransportDelegate::anyErrorFromCanard(0), Eq(cetl::nullopt));
    EXPECT_THAT(can::detail::TransportDelegate::anyErrorFromCanard(1), Eq(cetl::nullopt));
    EXPECT_THAT(can::detail::TransportDelegate::anyErrorFromCanard(-1), Eq(cetl::nullopt));
}

TEST_F(TestCanDelegate, canardMemoryAllocate_no_memory)
{
    StrictMock<MemoryResourceMock> mr_mock{};

    TransportDelegateImpl delegate{mr_mock};
    auto&                 canard_instance = delegate.canard_instance();

    // Emulate that there is no memory at all.
    EXPECT_CALL(mr_mock, do_allocate(_, _)).WillOnce(Return(nullptr));

    EXPECT_THAT(canard_instance.memory_allocate(&canard_instance, 1), IsNull());
}

TEST_F(TestCanDelegate, CanardConcreteTree_visitCounting)
{
    struct MyNode final : public CanardTreeNode
    {
        explicit MyNode(std::string _name)
            : CanardTreeNode{}
            , name{std::move(_name)}
        {
        }
        std::string name; // NOLINT
    };
    //        Root
    //      ↙     ↘
    //  Left       Right
    //      ↘      ↙   ↘
    //       LR  RL     RR
    //
    // NOLINTBEGIN(readability-isolate-declaration)
    MyNode root{"Root"}, left{"Left"}, right{"Right"};
    MyNode left_r{"LR"}, right_l{"RL"}, right_r{"RR"};
    // NOLINTEND(readability-isolate-declaration)
    root.lr[0]  = &left;
    root.lr[1]  = &right;
    left_r.up   = &left;
    left.lr[1]  = &left_r;
    right.lr[0] = &right_l;
    right.lr[1] = &right_r;
    left.up = right.up = &root;
    right_l.up = right_r.up = &right;

    using MyTree = can::detail::TransportDelegate::CanardConcreteTree<const MyNode>;
    {
        std::vector<std::string> names;
        auto count = MyTree::visitCounting(&root, [&names](const MyNode& node) { names.push_back(node.name); });
        EXPECT_THAT(count, 6);
        EXPECT_THAT(names, ElementsAre("Left", "LR", "Root", "RL", "Right", "RR"));
    }
    {
        std::vector<std::string> names;
        auto count = MyTree::visitCounting(&left, [&names](const MyNode& node) { names.push_back(node.name); });
        EXPECT_THAT(count, 2);
        EXPECT_THAT(names, ElementsAre("Left", "LR"));
    }
    {
        std::vector<std::string> names;
        auto count = MyTree::visitCounting(&right, [&names](const MyNode& node) { names.push_back(node.name); });
        EXPECT_THAT(count, 3);
        EXPECT_THAT(names, ElementsAre("RL", "Right", "RR"));
    }
    {
        std::vector<std::string> names;
        auto count = MyTree::visitCounting(&left_r, [&names](const MyNode& node) { names.push_back(node.name); });
        EXPECT_THAT(count, 1);
        EXPECT_THAT(names, ElementsAre("LR"));
    }
    {
        std::vector<std::string> names;
        auto count = MyTree::visitCounting(nullptr, [&names](const MyNode& node) { names.push_back(node.name); });
        EXPECT_THAT(count, 0);
        EXPECT_THAT(names, IsEmpty());
    }
}

// NOLINTEND(cppcoreguidelines-avoid-magic-numbers, readability-magic-numbers)

}  // namespace
