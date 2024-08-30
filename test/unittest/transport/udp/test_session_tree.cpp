/// @copyright
/// Copyright (C) OpenCyphal Development Team  <opencyphal.org>
/// Copyright Amazon.com Inc. or its affiliates.
/// SPDX-License-Identifier: MIT

#include "memory_resource_mock.hpp"
#include "tracking_memory_resource.hpp"

#include <cetl/pf17/cetlpf.hpp>
#include <libcyphal/transport/errors.hpp>
#include <libcyphal/transport/udp/session_tree.hpp>

#include <gmock/gmock.h>
#include <gtest/gtest.h>

#include <functional>
#include <string>
#include <utility>

namespace
{

using namespace libcyphal::transport;       // NOLINT This our main concern here in the unit tests.
using namespace libcyphal::transport::udp;  // NOLINT This our main concern here in the unit tests.

using testing::_;
using testing::Return;
using testing::IsEmpty;
using testing::StrictMock;
using testing::VariantWith;

// NOLINTBEGIN(cppcoreguidelines-avoid-magic-numbers, readability-magic-numbers)

class TestSessionTree : public testing::Test
{
protected:
    class MyNode final : public detail::RxSessionTreeNode::Base<MyNode>
    {
    public:
        using Base::Base;

        MyNode(const MyNode&)                = delete;
        MyNode(MyNode&&) noexcept            = delete;
        MyNode& operator=(const MyNode&)     = delete;
        MyNode& operator=(MyNode&&) noexcept = delete;

        ~MyNode()
        {
            if (notifier_)
            {
                notifier_("~");
            }
        }

        void setNotifier(std::function<void(const std::string&)> notifier)
        {
            notifier_ = std::move(notifier);
        }

    private:
        std::function<void(const std::string&)> notifier_;
    };

    void TearDown() override
    {
        EXPECT_THAT(mr_.allocations, IsEmpty());
        EXPECT_THAT(mr_.total_allocated_bytes, mr_.total_deallocated_bytes);
    }

    // MARK: Data members:

    // NOLINTBEGIN
    TrackingMemoryResource mr_;
    // NOLINTEND

};  // TestSessionTree

// MARK: - Tests:

TEST_F(TestSessionTree, constructor_destructor_empty_tree)
{
    const detail::SessionTree<MyNode> tree{mr_};
    EXPECT_THAT(tree.isEmpty(), true);
}

TEST_F(TestSessionTree, ensureNewNodeFor)
{
    detail::SessionTree<MyNode> tree{mr_};

    EXPECT_THAT(tree.ensureNewNodeFor(0), VariantWith<MyNode::ReferenceWrapper>(_));
    EXPECT_THAT(tree.isEmpty(), false);

    EXPECT_THAT(tree.ensureNewNodeFor(1), VariantWith<MyNode::ReferenceWrapper>(_));
    EXPECT_THAT(tree.ensureNewNodeFor(2), VariantWith<MyNode::ReferenceWrapper>(_));

    EXPECT_THAT(tree.ensureNewNodeFor(0), VariantWith<AnyFailure>(VariantWith<AlreadyExistsError>(_)));
    EXPECT_THAT(tree.ensureNewNodeFor(1), VariantWith<AnyFailure>(VariantWith<AlreadyExistsError>(_)));
    EXPECT_THAT(tree.ensureNewNodeFor(2), VariantWith<AnyFailure>(VariantWith<AlreadyExistsError>(_)));
}

TEST_F(TestSessionTree, ensureNewNodeFor_no_memory)
{
    StrictMock<MemoryResourceMock> mr_mock;
    mr_mock.redirectExpectedCallsTo(mr_);

    detail::SessionTree<MyNode> tree{mr_mock};

    // Emulate that there is no memory available for the message session.
    EXPECT_CALL(mr_mock, do_allocate(sizeof(MyNode), _))  //
        .WillOnce(Return(nullptr));

    EXPECT_THAT(tree.ensureNewNodeFor(0), VariantWith<AnyFailure>(VariantWith<MemoryError>(_)));
}

TEST_F(TestSessionTree, removeNodeFor)
{
    detail::SessionTree<MyNode> tree{mr_};

    tree.removeNodeFor(13);

    auto maybe_node = tree.ensureNewNodeFor(42);
    ASSERT_THAT(maybe_node, VariantWith<MyNode::ReferenceWrapper>(_));
    EXPECT_THAT(tree.isEmpty(), false);

    auto node_ref = cetl::get<MyNode::ReferenceWrapper>(maybe_node);

    std::string side_effects;
    node_ref.get().setNotifier([&](const std::string& msg) { side_effects += msg; });

    tree.removeNodeFor(42);
    EXPECT_THAT(side_effects, "~");

    EXPECT_THAT(tree.isEmpty(), true);
}

// NOLINTEND(cppcoreguidelines-avoid-magic-numbers, readability-magic-numbers)

}  // namespace
