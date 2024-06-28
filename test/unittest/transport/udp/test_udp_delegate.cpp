/// @copyright
/// Copyright (C) OpenCyphal Development Team  <opencyphal.org>
/// Copyright Amazon.com Inc. or its affiliates.
/// SPDX-License-Identifier: MIT

#include "../../memory_resource_mock.hpp"
#include "../../tracking_memory_resource.hpp"

#include <cetl/pf17/cetlpf.hpp>
#include <libcyphal/transport/errors.hpp>
#include <libcyphal/transport/types.hpp>
#include <libcyphal/transport/udp/delegate.hpp>
#include <libcyphal/types.hpp>
#include <udpard.h>

#include <gmock/gmock.h>
#include <gtest/gtest.h>

namespace
{

using libcyphal::TimePoint;
using namespace libcyphal::transport;       // NOLINT This our main concern here in the unit tests.
using namespace libcyphal::transport::udp;  // NOLINT This our main concern here in the unit tests.

using testing::_;
using testing::Eq;
using testing::Return;
using testing::IsNull;
using testing::NotNull;
using testing::IsEmpty;
using testing::Optional;
using testing::StrictMock;
using testing::VariantWith;

// NOLINTBEGIN(cppcoreguidelines-avoid-magic-numbers, readability-magic-numbers)

class TestUdpDelegate : public testing::Test
{
protected:
    class TransportDelegateImpl final : public udp::detail::TransportDelegate
    {
    public:
        using udp::detail::TransportDelegate::memoryResources;
        using udp::detail::TransportDelegate::makeUdpardMemoryDeleter;
        using udp::detail::TransportDelegate::makeUdpardMemoryResource;

        explicit TransportDelegateImpl(cetl::pmr::memory_resource& general_mr)
            : udp::detail::TransportDelegate{MemoryResources{general_mr,
                                                             makeUdpardMemoryResource(nullptr, general_mr),
                                                             makeUdpardMemoryResource(nullptr, general_mr),
                                                             makeUdpardMemoryDeleter(nullptr, general_mr)}}
        {
        }

        // MARK: TransportDelegate

        MOCK_METHOD((cetl::optional<AnyError>),
                    sendAnyTransfer,
                    (const udp::detail::AnyUdpardTxMetadata::Variant& tx_metadata_var,
                     const PayloadFragments                           payload_fragments),
                    (override));

        MOCK_METHOD(void, onSessionEvent, (const SessionEvent::Variant& event_var), (override));

    };  // TransportDelegateImpl

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

TEST_F(TestUdpDelegate, optAnyErrorFromUdpard)
{
    EXPECT_THAT(udp::detail::TransportDelegate::optAnyErrorFromUdpard(-UDPARD_ERROR_MEMORY),
                Optional(VariantWith<MemoryError>(_)));

    EXPECT_THAT(udp::detail::TransportDelegate::optAnyErrorFromUdpard(-UDPARD_ERROR_ARGUMENT),
                Optional(VariantWith<ArgumentError>(_)));

    EXPECT_THAT(udp::detail::TransportDelegate::optAnyErrorFromUdpard(-UDPARD_ERROR_CAPACITY),
                Optional(VariantWith<CapacityError>(_)));

    EXPECT_THAT(udp::detail::TransportDelegate::optAnyErrorFromUdpard(-UDPARD_ERROR_ANONYMOUS),
                Optional(VariantWith<AnonymousError>(_)));

    EXPECT_THAT(udp::detail::TransportDelegate::optAnyErrorFromUdpard(0), Eq(cetl::nullopt));
    EXPECT_THAT(udp::detail::TransportDelegate::optAnyErrorFromUdpard(1), Eq(cetl::nullopt));
    EXPECT_THAT(udp::detail::TransportDelegate::optAnyErrorFromUdpard(-1), Eq(cetl::nullopt));
}

TEST_F(TestUdpDelegate, makeUdpardMemoryResource)
{
    const auto udp_mem_res1 = TransportDelegateImpl::makeUdpardMemoryResource(nullptr, mr_);
    EXPECT_THAT(udp_mem_res1.user_reference, &mr_);
    EXPECT_THAT(udp_mem_res1.allocate, NotNull());
    EXPECT_THAT(udp_mem_res1.deallocate, NotNull());

    StrictMock<MemoryResourceMock> mr_mock{};
    const auto                     udp_mem_res2 = TransportDelegateImpl::makeUdpardMemoryResource(&mr_mock, mr_);
    EXPECT_THAT(udp_mem_res2.user_reference, &mr_mock);
    EXPECT_THAT(udp_mem_res2.allocate, NotNull());
    EXPECT_THAT(udp_mem_res2.deallocate, NotNull());
}

TEST_F(TestUdpDelegate, makeUdpardMemoryDeleter)
{
    const auto udp_mr_del1 = TransportDelegateImpl::makeUdpardMemoryDeleter(nullptr, mr_);
    EXPECT_THAT(udp_mr_del1.user_reference, &mr_);
    EXPECT_THAT(udp_mr_del1.deallocate, NotNull());

    StrictMock<MemoryResourceMock> mr_mock{};
    const auto                     udp_mr_del2 = TransportDelegateImpl::makeUdpardMemoryDeleter(&mr_mock, mr_);
    EXPECT_THAT(udp_mr_del2.user_reference, &mr_mock);
    EXPECT_THAT(udp_mr_del2.deallocate, NotNull());
}

TEST_F(TestUdpDelegate, allocateMemoryForUdpard_deallocateMemoryForUdpard)
{
    StrictMock<MemoryResourceMock> mr_mock{};
    mr_mock.redirectExpectedCallsTo(mr_);

    const TransportDelegateImpl delegate{mr_mock};

    const auto& fragment_mr = delegate.memoryResources().fragment;

    auto* mem_ptr = fragment_mr.allocate(fragment_mr.user_reference, 1);
    EXPECT_THAT(mem_ptr, NotNull());

    fragment_mr.deallocate(fragment_mr.user_reference, 1, mem_ptr);
}

TEST_F(TestUdpDelegate, allocateMemoryForUdpard_no_memory)
{
    StrictMock<MemoryResourceMock> mr_mock{};

    const TransportDelegateImpl delegate{mr_mock};

    // Emulate that there is no memory at all.
    EXPECT_CALL(mr_mock, do_allocate(1, _)).WillOnce(Return(nullptr));

    const auto& session_mr = delegate.memoryResources().session;
    EXPECT_THAT(session_mr.allocate(session_mr.user_reference, 1), IsNull());
}

// NOLINTEND(cppcoreguidelines-avoid-magic-numbers, readability-magic-numbers)

}  // namespace
