/// @copyright
/// Copyright (C) OpenCyphal Development Team  <opencyphal.org>
/// Copyright Amazon.com Inc. or its affiliates.
/// SPDX-License-Identifier: MIT

#include <libcyphal/transport/scattered_buffer.hpp>

#include <gmock/gmock.h>

namespace
{
using cetl::type_id_type;
using cetl::rtti_helper;

using ScatteredBuffer = libcyphal::transport::ScatteredBuffer;

using testing::Eq;
using testing::Return;
using testing::StrictMock;

// Just random id: 277C3545-564C-4617-993D-27B1043ECEBA
using TestTypeIdType =
    cetl::type_id_type<0x27, 0x7C, 0x35, 0x45, 0x56, 0x4C, 0x46, 0x17, 0x99, 0x3D, 0x27, 0xB1, 0x04, 0x3E, 0xCE, 0xBA>;

class InterfaceMock : public ScatteredBuffer::Interface
{
public:
    MOCK_METHOD(void, moved, ());
    MOCK_METHOD(void, deinit, ());

    MOCK_METHOD(std::size_t, size, (), (const, noexcept, override));
    MOCK_METHOD(std::size_t, copy, (const std::size_t, void* const, const std::size_t), (const, override));
};
class InterfaceWrapper final : public rtti_helper<TestTypeIdType, ScatteredBuffer::Interface>
{
public:
    explicit InterfaceWrapper(InterfaceMock* mock)
        : mock_{mock}
    {
    }
    InterfaceWrapper(InterfaceWrapper&& other) noexcept
    {
        move_from(std::move(other));
    }
    InterfaceWrapper& operator=(InterfaceWrapper&& other) noexcept
    {
        move_from(std::move(other));
        return *this;
    }
    ~InterfaceWrapper() override
    {
        if (mock_ != nullptr)
        {
            mock_->deinit();
            mock_ = nullptr;
        }
    }

    // ScatteredBuffer::Interface

    CETL_NODISCARD std::size_t size() const noexcept override
    {
        return mock_ ? mock_->size() : 0;
    }
    CETL_NODISCARD std::size_t copy(const std::size_t offset_bytes,
                                    void* const       destination,
                                    const std::size_t length_bytes) const override
    {
        return mock_ ? mock_->copy(offset_bytes, destination, length_bytes) : 0;
    }

private:
    InterfaceMock* mock_ = nullptr;

    void move_from(InterfaceWrapper&& other)
    {
        mock_       = other.mock_;
        other.mock_ = nullptr;

        if (mock_ != nullptr)
        {
            mock_->moved();
        }
    }

};  // InterfaceWrapper

// MARK: Tests:

TEST(TestScatteredBuffer, move_ctor_assign_size)
{
    StrictMock<InterfaceMock> interface_mock{};
    EXPECT_CALL(interface_mock, deinit()).Times(1);
    EXPECT_CALL(interface_mock, moved()).Times(1 + 2 + 2);
    EXPECT_CALL(interface_mock, size()).Times(3).WillRepeatedly(Return(42));
    {
        ScatteredBuffer src{InterfaceWrapper{&interface_mock}};  //< +1 move
        EXPECT_THAT(src.size(), Eq(42));

        ScatteredBuffer dst{std::move(src)};  //< +2 moves b/c of `cetl::any` specifics (via swap with tmp)
        EXPECT_THAT(src.size(), Eq(0));
        EXPECT_THAT(dst.size(), Eq(42));

        src = std::move(dst);  //< +2 moves
        EXPECT_THAT(src.size(), Eq(42));
        EXPECT_THAT(dst.size(), Eq(0));
    }
}

TEST(TestScatteredBuffer, copy_reset)
{
    std::array<std::uint8_t, 16> test_dst{};

    StrictMock<InterfaceMock> interface_mock{};
    EXPECT_CALL(interface_mock, deinit()).Times(1);
    EXPECT_CALL(interface_mock, moved()).Times(1);
    EXPECT_CALL(interface_mock, copy(13, test_dst.data(), test_dst.size())).WillOnce(Return(7));
    {
        ScatteredBuffer buffer{InterfaceWrapper{&interface_mock}};

        auto copied_bytes = buffer.copy(13, test_dst.data(), test_dst.size());
        EXPECT_THAT(copied_bytes, Eq(7));

        buffer.reset();
        copied_bytes = buffer.copy(13, test_dst.data(), test_dst.size());
        EXPECT_THAT(copied_bytes, Eq(0));
    }
}

}  // namespace
