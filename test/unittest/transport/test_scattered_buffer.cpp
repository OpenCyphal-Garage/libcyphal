/// @copyright
/// Copyright (C) OpenCyphal Development Team  <opencyphal.org>
/// Copyright Amazon.com Inc. or its affiliates.
/// SPDX-License-Identifier: MIT

#include <libcyphal/transport/scattered_buffer.hpp>

#include <gmock/gmock.h>
#include <gtest/gtest.h>

#include <array>
#include <cstddef>
#include <cstdint>
#include <utility>

namespace
{
using cetl::type_id_type;
using cetl::rtti_helper;

using ScatteredBuffer = libcyphal::transport::ScatteredBuffer;

using testing::Return;
using testing::StrictMock;

// NOLINTBEGIN(cppcoreguidelines-avoid-magic-numbers, readability-magic-numbers)

// Just random id: 277C3545-564C-4617-993D-27B1043ECEBA
using StorageWrapperTypeIdType =
    cetl::type_id_type<0x27, 0x7C, 0x35, 0x45, 0x56, 0x4C, 0x46, 0x17, 0x99, 0x3D, 0x27, 0xB1, 0x04, 0x3E, 0xCE, 0xBA>;

class StorageMock : public ScatteredBuffer::IStorage
{
public:
    MOCK_METHOD(void, moved, (), (noexcept)); // NOLINT(bugprone-exception-escape)
    MOCK_METHOD(void, deinit, (), (noexcept)); // NOLINT(bugprone-exception-escape)

    MOCK_METHOD(std::size_t, size, (), (const, noexcept, override)); // NOLINT(bugprone-exception-escape)
    MOCK_METHOD(std::size_t, copy, (const std::size_t, void* const, const std::size_t), (const, override));
};
class StorageWrapper final : public rtti_helper<StorageWrapperTypeIdType, ScatteredBuffer::IStorage>
{
public:
    explicit StorageWrapper(StorageMock* mock)
        : mock_{mock}
    {
    }
    StorageWrapper(StorageWrapper&& other) noexcept
    {
        move_from(other);
    }
    StorageWrapper& operator=(StorageWrapper&& other) noexcept
    {
        move_from(other);
        return *this;
    }

    StorageWrapper(const StorageWrapper& other)            = delete;
    StorageWrapper& operator=(const StorageWrapper& other) = delete;

    ~StorageWrapper() final
    {
        if (mock_ != nullptr)
        {
            mock_->deinit();
            mock_ = nullptr;
        }
    }

    // ScatteredBuffer::IStorage

    std::size_t size() const noexcept final
    {
        return (mock_ != nullptr) ? mock_->size() : 0;
    }
    std::size_t copy(const std::size_t offset_bytes,
                     void* const       destination,
                     const std::size_t length_bytes) const final
    {
        return (mock_ != nullptr) ? mock_->copy(offset_bytes, destination, length_bytes) : 0;
    }

private:
    StorageMock* mock_{nullptr};

    void move_from(StorageWrapper& other) noexcept
    {
        mock_       = other.mock_;
        other.mock_ = nullptr;

        if (mock_ != nullptr)
        {
            mock_->moved();
        }
    }

};  // StorageWrapper

// MARK: Tests:

TEST(TestScatteredBuffer, move_ctor_assign_size)
{
    StrictMock<StorageMock> storage_mock{};
    EXPECT_CALL(storage_mock, deinit()).Times(1);
    EXPECT_CALL(storage_mock, moved()).Times(1 + 2);
    EXPECT_CALL(storage_mock, size()).Times(3).WillRepeatedly(Return(42));
    {
        ScatteredBuffer src{StorageWrapper{&storage_mock}};  //< +1 move
        EXPECT_THAT(src.size(), 42);

        ScatteredBuffer dst{std::move(src)};  //< +2 moves b/c of `cetl::any` specifics (via swap with tmp)
        EXPECT_THAT(dst.size(), 42);

        auto src2 = std::move(dst);  //< +2 moves
        EXPECT_THAT(src2.size(), 42);
    }
}

TEST(TestScatteredBuffer, copy_reset)
{
    std::array<std::uint8_t, 16> test_dst{};

    StrictMock<StorageMock> storage_mock{};
    EXPECT_CALL(storage_mock, deinit()).Times(1);
    EXPECT_CALL(storage_mock, moved()).Times(1);
    EXPECT_CALL(storage_mock, copy(13, test_dst.data(), test_dst.size())).WillOnce(Return(7));
    {
        ScatteredBuffer buffer{StorageWrapper{&storage_mock}};

        auto copied_bytes = buffer.copy(13, test_dst.data(), test_dst.size());
        EXPECT_THAT(copied_bytes, 7);

        buffer.reset();
        copied_bytes = buffer.copy(13, test_dst.data(), test_dst.size());
        EXPECT_THAT(copied_bytes, 0);
    }
}

// NOLINTEND(cppcoreguidelines-avoid-magic-numbers, readability-magic-numbers)

}  // namespace
