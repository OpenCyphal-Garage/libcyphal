/// @copyright
/// Copyright (C) OpenCyphal Development Team  <opencyphal.org>
/// Copyright Amazon.com Inc. or its affiliates.
/// SPDX-License-Identifier: MIT

#ifndef LIBCYPHAL_TRANSPORT_SCATTERED_BUFFER_STORAGE_MOCK_HPP_INCLUDED
#define LIBCYPHAL_TRANSPORT_SCATTERED_BUFFER_STORAGE_MOCK_HPP_INCLUDED

#include <cetl/pf17/cetlpf.hpp>
#include <cetl/rtti.hpp>
#include <libcyphal/transport/scattered_buffer.hpp>

#include <gmock/gmock.h>

#include <cstddef>

namespace libcyphal
{
namespace transport
{

class ScatteredBufferStorageMock : public ScatteredBuffer::IStorage
{
public:
    class Wrapper final : public IStorage
    {
        // 277C3545-564C-4617-993D-27B1043ECEBA
        // clang-format off
        using TypeIdType = cetl::type_id_type<
            // NOLINTNEXTLINE(cppcoreguidelines-avoid-magic-numbers, readability-magic-numbers)
            0x27, 0x7C, 0x35, 0x45, 0x56, 0x4C, 0x46, 0x17, 0x99, 0x3D, 0x27, 0xB1, 0x04, 0x3E, 0xCE, 0xBA>;
        // clang-format on

    public:
        explicit Wrapper(ScatteredBufferStorageMock* mock)
            : mock_{mock}
        {
        }
        Wrapper(Wrapper&& other) noexcept
        {
            move_from(other);
        }
        Wrapper& operator=(Wrapper&& other) noexcept
        {
            move_from(other);
            return *this;
        }

        Wrapper(const Wrapper& other)            = delete;
        Wrapper& operator=(const Wrapper& other) = delete;

        ~Wrapper()
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
        ScatteredBufferStorageMock* mock_{nullptr};

        void move_from(Wrapper& other) noexcept
        {
            mock_       = other.mock_;
            other.mock_ = nullptr;

            if (mock_ != nullptr)
            {
                mock_->moved();
            }
        }

    };  // Wrapper

    ScatteredBufferStorageMock()                                                 = default;
    ScatteredBufferStorageMock(const ScatteredBufferStorageMock&)                = delete;
    ScatteredBufferStorageMock(ScatteredBufferStorageMock&&) noexcept            = delete;
    ScatteredBufferStorageMock& operator=(const ScatteredBufferStorageMock&)     = delete;
    ScatteredBufferStorageMock& operator=(ScatteredBufferStorageMock&&) noexcept = delete;

    virtual ~ScatteredBufferStorageMock() = default;

    MOCK_METHOD(void, moved, (), (noexcept));   // NOLINT(bugprone-exception-escape)
    MOCK_METHOD(void, deinit, (), (noexcept));  // NOLINT(bugprone-exception-escape)

    MOCK_METHOD(std::size_t, size, (), (const, noexcept, override));  // NOLINT(bugprone-exception-escape)
    MOCK_METHOD(std::size_t, copy, (const std::size_t, cetl::byte* const, const std::size_t), (const, override));

};  // ScatteredBufferStorageMock

}  // namespace transport
}  // namespace libcyphal

#endif  // LIBCYPHAL_TRANSPORT_SCATTERED_BUFFER_STORAGE_MOCK_HPP_INCLUDED
