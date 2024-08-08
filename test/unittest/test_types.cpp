/// @copyright
/// Copyright (C) OpenCyphal Development Team  <opencyphal.org>
/// Copyright Amazon.com Inc. or its affiliates.
/// SPDX-License-Identifier: MIT

#include "tracking_memory_resource.hpp"

#include <cetl/pf17/cetlpf.hpp>
#include <cetl/rtti.hpp>
#include <cetl/unbounded_variant.hpp>
#include <libcyphal/types.hpp>

#include <gmock/gmock.h>
#include <gtest/gtest.h>

#include <cstddef>
#include <memory>
#include <string>
#include <utility>

namespace
{

using cetl::byte;

using testing::IsNull;
using testing::IsEmpty;

// NOLINTBEGIN(cppcoreguidelines-avoid-magic-numbers, readability-magic-numbers)

class TestTypes : public testing::Test
{
protected:
    // 582F97AF-8B0A-4C22-8369-2A2B39CCE2AD
    using MyInterfaceTypeIdType = cetl::
        // NOLINTNEXTLINE(cppcoreguidelines-avoid-magic-numbers, readability-magic-numbers)
        type_id_type<0x58, 0x2F, 0x97, 0xAF, 0x8B, 0x0A, 0x4C, 0x22, 0x83, 0x69, 0x2A, 0x2B, 0x39, 0xCC, 0xE2, 0xAD>;

    // B6F48C7E-FDF5-4CDF-845F-92E555BE49FF
    using MyConcreteTypeIdType = cetl::
        // NOLINTNEXTLINE(cppcoreguidelines-avoid-magic-numbers, readability-magic-numbers)
        type_id_type<0xB6, 0xF4, 0x8C, 0x7E, 0xFD, 0xF5, 0x4C, 0xDF, 0x84, 0x5F, 0x92, 0xE5, 0x55, 0xBE, 0x49, 0xFF>;

    class MyInterface : public cetl::rtti_helper<MyInterfaceTypeIdType>
    {
    public:
        virtual std::string what() const = 0;
    };

    class MyConcrete final : public cetl::rtti_helper<MyConcreteTypeIdType, MyInterface>
    {
    public:
        explicit MyConcrete(std::string name)
            : name_{std::move(name)}
        {
        }

    private:
        std::string what() const override
        {
            return "MyConcrete " + name_;
        }
        std::string name_;
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
};

// MARK: - Tests:

TEST_F(TestTypes, ImplementationCell)
{
    using ub_var = cetl::unbounded_variant<sizeof(MyConcrete)>;
    using MyCell = libcyphal::ImplementationCell<MyInterface, ub_var>;

    const MyConcrete                                         my_concrete{"A"};
    const libcyphal::ImplementationCell<MyInterface, ub_var> my_cell0{my_concrete};
    EXPECT_THAT(!!my_cell0, true);
    EXPECT_THAT(my_cell0->what(), "MyConcrete A");

    MyCell my_cell1{MyConcrete{"B"}};
    EXPECT_THAT(!!my_cell1, true);
    EXPECT_THAT(my_cell1->what(), "MyConcrete B");

    const MyCell my_cell2{std::move(my_cell1)};
    // NOLINTNEXTLINE(clang-analyzer-cplusplus.Move,bugprone-use-after-move,hicpp-invalid-access-moved)
    EXPECT_THAT(!!my_cell1, false);
    EXPECT_THAT(!!my_cell2, true);
    EXPECT_THAT(my_cell2->what(), "MyConcrete B");
}

TEST_F(TestTypes, PmrRawBytesDeleter)
{
    using RawBytesUniquePtr = std::unique_ptr<byte, libcyphal::PmrRawBytesDeleter>;

    constexpr std::size_t size_bytes = 13;

    const libcyphal::PmrRawBytesDeleter deleter(size_bytes, &mr_);
    EXPECT_THAT(deleter.size(), size_bytes);
    EXPECT_THAT(deleter.resource(), &mr_);

    RawBytesUniquePtr buffer_ptr{};
    EXPECT_THAT(buffer_ptr.get_deleter().size(), 0);
    EXPECT_THAT(buffer_ptr.get_deleter().resource(), IsNull());

    buffer_ptr = RawBytesUniquePtr(static_cast<byte*>(mr_.allocate(size_bytes)), deleter);
    buffer_ptr.reset();

    const auto deleter_copy = buffer_ptr.get_deleter();
    EXPECT_THAT(deleter_copy.size(), size_bytes);
    EXPECT_THAT(deleter_copy.resource(), &mr_);

    byte* const raw_buffer = static_cast<byte*>(mr_.allocate(size_bytes));
    deleter_copy(raw_buffer);
}

TEST_F(TestTypes, PmrRawBytesDeleter_corner_cases)
{
    using RawBytesUniquePtr = std::unique_ptr<byte, libcyphal::PmrRawBytesDeleter>;

    // Try zero size buffer.
    RawBytesUniquePtr zero_buffer_ptr{static_cast<byte*>(mr_.allocate(0)), libcyphal::PmrRawBytesDeleter{0, &mr_}};
    zero_buffer_ptr.reset();

    // It's ok to invoke PMR deleter with `nullptr` buffer.
    RawBytesUniquePtr no_buffer_ptr{nullptr, libcyphal::PmrRawBytesDeleter{42, &mr_}};
    no_buffer_ptr.get_deleter()(nullptr);

    // It's ok to invoke default `nullptr` PMR deleter with `nullptr` buffer.
    RawBytesUniquePtr buffer_ptr{};
    buffer_ptr.get_deleter()(nullptr);
}

// NOLINTEND(cppcoreguidelines-avoid-magic-numbers, readability-magic-numbers)

}  // namespace
