/// @copyright
/// Copyright (C) OpenCyphal Development Team  <opencyphal.org>
/// Copyright Amazon.com Inc. or its affiliates.
/// SPDX-License-Identifier: MIT

#include <libcyphal/transport/scattered_buffer.hpp>

#include <cetl/rtti.hpp>

#include <gmock/gmock.h>
#include <gtest/gtest.h>

#include <array>
#include <cstddef>
#include <cstdint>
#include <utility>

namespace
{

using namespace libcyphal::transport;  // NOLINT This our main concern here in the unit tests.

using testing::Return;
using testing::StrictMock;

// NOLINTBEGIN(cppcoreguidelines-avoid-magic-numbers, readability-magic-numbers)

// Just random id: 277C3545-564C-4617-993D-27B1043ECEBA
using StorageWrapperTypeIdType =
    cetl::type_id_type<0x27, 0x7C, 0x35, 0x45, 0x56, 0x4C, 0x46, 0x17, 0x99, 0x3D, 0x27, 0xB1, 0x04, 0x3E, 0xCE, 0xBA>;

class StorageMock : public ScatteredBuffer::IStorage
{
public:
    MOCK_METHOD(void, moved, (), (noexcept));   // NOLINT(bugprone-exception-escape)
    MOCK_METHOD(void, deinit, (), (noexcept));  // NOLINT(bugprone-exception-escape)

    MOCK_METHOD(std::size_t, size, (), (const, noexcept, override));  // NOLINT(bugprone-exception-escape)
    MOCK_METHOD(std::size_t, copy, (const std::size_t, cetl::byte* const, const std::size_t), (const, override));
};
class StorageWrapper final : public cetl::rtti_helper<StorageWrapperTypeIdType, ScatteredBuffer::IStorage>
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

    ~StorageWrapper() override
    {
        if (mock_ != nullptr)
        {
            mock_->deinit();
            mock_ = nullptr;
        }
    }

    // ScatteredBuffer::IStorage

    std::size_t size() const noexcept override
    {
        return (mock_ != nullptr) ? mock_->size() : 0;
    }
    std::size_t copy(const std::size_t offset_bytes,
                     cetl::byte* const destination,
                     const std::size_t length_bytes) const override
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
    EXPECT_CALL(storage_mock, moved()).Times(1 + 1 + 2);
    EXPECT_CALL(storage_mock, size()).Times(3).WillRepeatedly(Return(42));
    {
        ScatteredBuffer src{StorageWrapper{&storage_mock}};  //< +1 move
        EXPECT_THAT(src.size(), 42);

        ScatteredBuffer dst{std::move(src)};  //< +1 move
        // NOLINTNEXTLINE(clang-analyzer-cplusplus.Move,bugprone-use-after-move,hicpp-invalid-access-moved)
        EXPECT_THAT(src.size(), 0);
        EXPECT_THAT(dst.size(), 42);

        src = std::move(dst);  //< +2 moves b/c of `cetl::unbounded_variant` specifics (via swap with tmp)
        EXPECT_THAT(src.size(), 42);
        // NOLINTNEXTLINE(clang-analyzer-cplusplus.Move,bugprone-use-after-move,hicpp-invalid-access-moved)
        EXPECT_THAT(dst.size(), 0);
    }
}

TEST(TestScatteredBuffer, copy_reset)
{
    std::array<cetl::byte, 16> test_dst{};

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
