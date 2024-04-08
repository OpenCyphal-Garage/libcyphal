/// @copyright
/// Copyright (C) OpenCyphal Development Team  <opencyphal.org>
/// Copyright Amazon.com Inc. or its affiliates.
/// SPDX-License-Identifier: MIT

#include "libcyphal/transport/dynamic_buffer.hpp"

#include <gmock/gmock.h>

using cetl::type_id_type;
using cetl::rtti_helper;

using DynamicBuffer = libcyphal::transport::DynamicBuffer;

using testing::Return;
using testing::StrictMock;

namespace
{

class InterfaceMock : public DynamicBuffer::Interface
{
public:
    MOCK_METHOD(void, moved, ());
    MOCK_METHOD(void, deinit, ());

    MOCK_METHOD(std::size_t, size, (), (const override));
    MOCK_METHOD(std::size_t, copy, (const std::size_t, void* const, const std::size_t), (const override));
};
class InterfaceWrapper : public rtti_helper<type_id_type<0x01>, DynamicBuffer::Interface>
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

    // DynamicBuffer::Interface

    CETL_NODISCARD std::size_t size() const override
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

TEST(test_dynamic_buffer, move_ctor_assign__size)
{
    StrictMock<InterfaceMock> interface_mock{};
    EXPECT_CALL(interface_mock, deinit()).Times(1);
    EXPECT_CALL(interface_mock, moved()).Times(1 + 2 + 2);
    EXPECT_CALL(interface_mock, size()).Times(3).WillRepeatedly(Return(42));
    {
        DynamicBuffer src{InterfaceWrapper{&interface_mock}};  //< +1 move
        EXPECT_EQ(42, src.size());

        DynamicBuffer dst{std::move(src)};  //< +2 moves b/c of `cetl::any` specifics (via swap with tmp)
        EXPECT_EQ(0, src.size());
        EXPECT_EQ(42, dst.size());

        src = std::move(dst);  //< +2 moves
        EXPECT_EQ(42, src.size());
        EXPECT_EQ(0, dst.size());
    }
}

TEST(test_dynamic_buffer, copy__reset)
{
    std::array<uint8_t, 16> test_dst{};

    StrictMock<InterfaceMock> interface_mock{};
    EXPECT_CALL(interface_mock, deinit()).Times(1);
    EXPECT_CALL(interface_mock, moved()).Times(1);
    EXPECT_CALL(interface_mock, copy(13, test_dst.data(), test_dst.size())).WillOnce(Return(7));
    {
        DynamicBuffer buffer{InterfaceWrapper{&interface_mock}};

        auto copied_bytes = buffer.copy(13, test_dst.data(), test_dst.size());
        EXPECT_EQ(7, copied_bytes);

        buffer.reset();
        copied_bytes = buffer.copy(13, test_dst.data(), test_dst.size());
        EXPECT_EQ(0, copied_bytes);
    }
}

}  // namespace
