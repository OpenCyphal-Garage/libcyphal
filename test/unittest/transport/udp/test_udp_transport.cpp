/// @copyright
/// Copyright (C) OpenCyphal Development Team  <opencyphal.org>
/// Copyright Amazon.com Inc. or its affiliates.
/// SPDX-License-Identifier: MIT

#include "../../tracking_memory_resource.hpp"
#include "../multiplexer_mock.hpp"
#include "media_mock.hpp"

#include <cetl/pf17/cetlpf.hpp>
#include <libcyphal/transport/errors.hpp>
#include <libcyphal/transport/udp/media.hpp>
#include <libcyphal/transport/udp/transport.hpp>
#include <udpard.h>

#include <gmock/gmock.h>
#include <gtest/gtest.h>

#include <array>
#include <cstddef>
#include <utility>

namespace
{

using libcyphal::UniquePtr;
using namespace libcyphal::transport;  // NOLINT This our main concern here in the unit tests.

using testing::_;
using testing::Eq;
using testing::Return;
using testing::IsEmpty;
using testing::NotNull;
using testing::Optional;
using testing::StrictMock;
using testing::VariantWith;

// NOLINTBEGIN(cppcoreguidelines-avoid-magic-numbers, readability-magic-numbers)

class TestUpdTransport : public testing::Test
{
protected:
    void TearDown() override
    {
        EXPECT_THAT(mr_.allocations, IsEmpty());
        EXPECT_THAT(mr_.total_allocated_bytes, mr_.total_deallocated_bytes);
    }

    UniquePtr<udp::IUdpTransport> makeTransport(cetl::pmr::memory_resource& mr,
                                                udp::IMedia*                extra_media = nullptr,
                                                const std::size_t           tx_capacity = 16)
    {
        std::array<udp::IMedia*, 2> media_array{&media_mock_, extra_media};

        auto maybe_transport = udp::makeTransport({mr}, mux_mock_, media_array, tx_capacity);
        EXPECT_THAT(maybe_transport, VariantWith<UniquePtr<udp::IUdpTransport>>(NotNull()));
        return cetl::get<UniquePtr<udp::IUdpTransport>>(std::move(maybe_transport));
    }

    // MARK: Data members:

    // NOLINTBEGIN
    TrackingMemoryResource      mr_;
    StrictMock<MultiplexerMock> mux_mock_{};
    StrictMock<udp::MediaMock>  media_mock_{};
    // NOLINTEND
};

// MARK: Tests:

TEST_F(TestUpdTransport, makeTransport_getLocalNodeId)
{
    // Anonymous node
    {
        std::array<udp::IMedia*, 1> media_array{&media_mock_};
        auto                        maybe_transport = udp::makeTransport({mr_}, mux_mock_, media_array, 0);
        ASSERT_THAT(maybe_transport, VariantWith<UniquePtr<udp::IUdpTransport>>(NotNull()));

        auto transport = cetl::get<UniquePtr<udp::IUdpTransport>>(std::move(maybe_transport));
        EXPECT_THAT(transport->getLocalNodeId(), Eq(cetl::nullopt));
    }

    // Node with ID
    {
        std::array<udp::IMedia*, 1> media_array{&media_mock_};
        auto                        maybe_transport = udp::makeTransport({mr_}, mux_mock_, media_array, 0);
        ASSERT_THAT(maybe_transport, VariantWith<UniquePtr<udp::IUdpTransport>>(NotNull()));

        auto transport = cetl::get<UniquePtr<udp::IUdpTransport>>(std::move(maybe_transport));
        transport->setLocalNodeId(42);

        EXPECT_THAT(transport->getLocalNodeId(), Optional(42));
    }

    // Two media interfaces
    {
        StrictMock<udp::MediaMock> media_mock2;
        EXPECT_CALL(media_mock2, getMtu()).WillRepeatedly(Return(UDPARD_MTU_DEFAULT));

        std::array<udp::IMedia*, 3> media_array{&media_mock_, nullptr, &media_mock2};
        auto                        maybe_transport = udp::makeTransport({mr_}, mux_mock_, media_array, 0);
        EXPECT_THAT(maybe_transport, VariantWith<UniquePtr<udp::IUdpTransport>>(NotNull()));
    }

    // All 3 maximum number of media interfaces
    {
        StrictMock<udp::MediaMock> media_mock2{};
        StrictMock<udp::MediaMock> media_mock3{};
        EXPECT_CALL(media_mock2, getMtu()).WillRepeatedly(Return(UDPARD_MTU_DEFAULT));
        EXPECT_CALL(media_mock3, getMtu()).WillRepeatedly(Return(UDPARD_MTU_DEFAULT));

        std::array<udp::IMedia*, 3> media_array{&media_mock_, &media_mock2, &media_mock3};
        auto                        maybe_transport = udp::makeTransport({mr_}, mux_mock_, media_array, 0);
        EXPECT_THAT(maybe_transport, VariantWith<UniquePtr<udp::IUdpTransport>>(NotNull()));
    }
}

// NOLINTEND(cppcoreguidelines-avoid-magic-numbers, readability-magic-numbers)

}  // namespace
