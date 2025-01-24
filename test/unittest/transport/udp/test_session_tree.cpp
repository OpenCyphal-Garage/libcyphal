/// @copyright
/// Copyright (C) OpenCyphal Development Team  <opencyphal.org>
/// Copyright Amazon.com Inc. or its affiliates.
/// SPDX-License-Identifier: MIT

#include "memory_resource_mock.hpp"
#include "tracking_memory_resource.hpp"

#include <cetl/pf17/cetlpf.hpp>
#include <libcyphal/errors.hpp>
#include <libcyphal/transport/errors.hpp>
#include <libcyphal/transport/udp/session_tree.hpp>

#include <gmock/gmock.h>
#include <gtest/gtest.h>

#include <cstdint>
#include <functional>
#include <string>
#include <utility>

namespace
{

using libcyphal::MemoryError;
using namespace libcyphal::transport;       // NOLINT This our main concern here in the unit tests.
using namespace libcyphal::transport::udp;  // NOLINT This our main concern here in the unit tests.

using testing::_;
using testing::Return;
using testing::IsEmpty;
using testing::Pointer;
using testing::StrictMock;
using testing::VariantWith;

// NOLINTBEGIN(cppcoreguidelines-avoid-magic-numbers, readability-magic-numbers)

class TestSessionTree : public testing::Test
{
protected:
    class MyNode final : public detail::RxSessionTreeNode::Base<MyNode>
    {
    public:
        using Params = std::int32_t;

        explicit MyNode(const Params& params)
            : params_{params}
            , extra_arg_{0}
        {
        }

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

        CETL_NODISCARD std::int32_t compareByParams(const Params& params) const
        {
            return params_ - params;
        }

        void setNotifier(std::function<void(const std::string&)> notifier)
        {
            notifier_ = std::move(notifier);
        }

    private:
        const Params                            params_;
        const int                               extra_arg_;
        std::function<void(const std::string&)> notifier_;

    };  // MyNode

    void SetUp() override
    {
        cetl::pmr::set_default_resource(&mr_);
    }

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

TEST_F(TestSessionTree, ensureNodeFor_should_be_new)
{
    detail::SessionTree<MyNode> tree{mr_};

    EXPECT_THAT(tree.ensureNodeFor<true>(0), VariantWith<MyNode::ReferenceWrapper>(_));
    EXPECT_THAT(tree.isEmpty(), false);

    EXPECT_THAT(tree.ensureNodeFor<true>(1), VariantWith<MyNode::ReferenceWrapper>(_));
    EXPECT_THAT(tree.ensureNodeFor<true>(2), VariantWith<MyNode::ReferenceWrapper>(_));
    EXPECT_THAT(tree.ensureNodeFor<true>(0), VariantWith<AnyFailure>(VariantWith<AlreadyExistsError>(_)));
    EXPECT_THAT(tree.ensureNodeFor<true>(1), VariantWith<AnyFailure>(VariantWith<AlreadyExistsError>(_)));
    EXPECT_THAT(tree.ensureNodeFor<true>(2), VariantWith<AnyFailure>(VariantWith<AlreadyExistsError>(_)));
}

TEST_F(TestSessionTree, ensureNodeFor_existing_is_fine)
{
    detail::SessionTree<MyNode> tree{mr_};

    auto maybe_node_0a = tree.ensureNodeFor(0);
    ASSERT_THAT(maybe_node_0a, VariantWith<MyNode::ReferenceWrapper>(_));
    const auto node_0a = cetl::get<MyNode::ReferenceWrapper>(maybe_node_0a);

    EXPECT_THAT(tree.isEmpty(), false);

    auto maybe_node_1a = tree.ensureNodeFor(1);
    EXPECT_THAT(maybe_node_1a, VariantWith<MyNode::ReferenceWrapper>(_));
    const auto node_1a = cetl::get<MyNode::ReferenceWrapper>(maybe_node_1a);

    EXPECT_THAT(tree.ensureNodeFor(2), VariantWith<MyNode::ReferenceWrapper>(_));

    auto maybe_node_0b = tree.ensureNodeFor(0);
    EXPECT_THAT(maybe_node_0b, VariantWith<MyNode::ReferenceWrapper>(Pointer(&node_0a.get())));

    auto maybe_node_1b = tree.ensureNodeFor(1);
    EXPECT_THAT(maybe_node_1b, VariantWith<MyNode::ReferenceWrapper>(Pointer(&node_1a.get())));

    EXPECT_THAT(tree.ensureNodeFor(2), VariantWith<MyNode::ReferenceWrapper>(_));
}

TEST_F(TestSessionTree, ensureNodeFor_no_memory)
{
    StrictMock<MemoryResourceMock> mr_mock;
    mr_mock.redirectExpectedCallsTo(mr_);

    detail::SessionTree<MyNode> tree{mr_mock};

    // Emulate that there is no memory available for the message session.
    EXPECT_CALL(mr_mock, do_allocate(sizeof(MyNode), _))  //
        .WillOnce(Return(nullptr));

    EXPECT_THAT(tree.ensureNodeFor(0), VariantWith<AnyFailure>(VariantWith<MemoryError>(_)));
}

TEST_F(TestSessionTree, removeNodeFor)
{
    detail::SessionTree<MyNode> tree{mr_};

    tree.removeNodeFor(13);

    auto maybe_node = tree.ensureNodeFor<true>(42);
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
