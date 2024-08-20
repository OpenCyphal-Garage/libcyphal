/// @copyright
/// Copyright (C) OpenCyphal Development Team  <opencyphal.org>
/// Copyright Amazon.com Inc. or its affiliates.
/// SPDX-License-Identifier: MIT

#include "scattered_buffer_storage_mock.hpp"

#include <libcyphal/transport/errors.hpp>
#include <libcyphal/transport/scattered_buffer.hpp>

#include <cetl/pf17/cetlpf.hpp>
#include <cetl/rtti.hpp>

#include <gmock/gmock.h>
#include <gtest/gtest.h>

#include <array>
#include <utility>

namespace
{

using namespace libcyphal::transport;  // NOLINT This our main concern here in the unit tests.

using testing::IsNull;
using testing::Return;
using testing::NotNull;
using testing::StrictMock;

// NOLINTBEGIN(cppcoreguidelines-avoid-magic-numbers, readability-magic-numbers)

// MARK: - Tests:

TEST(TestScatteredBuffer, rtti)
{
    // mutable
    {
        StrictMock<ScatteredBufferStorageMock> storage_mock;
        EXPECT_CALL(storage_mock, deinit());

        ScatteredBufferStorageMock::Wrapper storage{&storage_mock};
        EXPECT_THAT(cetl::rtti_cast<ScatteredBuffer::IStorage*>(&storage), NotNull());
        EXPECT_THAT(cetl::rtti_cast<IPlatformError*>(&storage), IsNull());
    }
    // const
    {
        StrictMock<ScatteredBufferStorageMock> storage_mock;
        EXPECT_CALL(storage_mock, deinit());

        const ScatteredBufferStorageMock::Wrapper storage{&storage_mock};
        EXPECT_THAT(cetl::rtti_cast<ScatteredBuffer::IStorage*>(&storage), NotNull());
        EXPECT_THAT(cetl::rtti_cast<IPlatformError*>(&storage), IsNull());
    }
}

TEST(TestScatteredBuffer, move_ctor_assign_size)
{
    StrictMock<ScatteredBufferStorageMock> storage_mock;
    EXPECT_CALL(storage_mock, deinit()).Times(1);
    EXPECT_CALL(storage_mock, moved()).Times(1 + 1 + 1);
    EXPECT_CALL(storage_mock, size())  //
        .Times(3)
        .WillRepeatedly(Return(42));
    {
        ScatteredBuffer src{ScatteredBufferStorageMock::Wrapper{&storage_mock}};  //< +1 move
        EXPECT_THAT(src.size(), 42);

        ScatteredBuffer dst{std::move(src)};  //< +1 move
        // NOLINTNEXTLINE(clang-analyzer-cplusplus.Move,bugprone-use-after-move,hicpp-invalid-access-moved)
        EXPECT_THAT(src.size(), 0);
        EXPECT_THAT(dst.size(), 42);

        src = std::move(dst);  //< +1 move
        EXPECT_THAT(src.size(), 42);
        // NOLINTNEXTLINE(clang-analyzer-cplusplus.Move,bugprone-use-after-move,hicpp-invalid-access-moved)
        EXPECT_THAT(dst.size(), 0);
    }
}

TEST(TestScatteredBuffer, copy_reset)
{
    std::array<cetl::byte, 16> test_dst{};

    StrictMock<ScatteredBufferStorageMock> storage_mock;
    EXPECT_CALL(storage_mock, deinit()).Times(1);
    EXPECT_CALL(storage_mock, moved()).Times(1);
    EXPECT_CALL(storage_mock, copy(13, test_dst.data(), test_dst.size()))  //
        .WillOnce(Return(7));
    {
        ScatteredBuffer buffer{ScatteredBufferStorageMock::Wrapper{&storage_mock}};

        auto copied_bytes = buffer.copy(13, test_dst.data(), test_dst.size());
        EXPECT_THAT(copied_bytes, 7);

        buffer.reset();
        copied_bytes = buffer.copy(13, test_dst.data(), test_dst.size());
        EXPECT_THAT(copied_bytes, 0);
    }
}

// NOLINTEND(cppcoreguidelines-avoid-magic-numbers, readability-magic-numbers)

}  // namespace
