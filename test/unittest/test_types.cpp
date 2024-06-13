/// @copyright
/// Copyright (C) OpenCyphal Development Team  <opencyphal.org>
/// Copyright Amazon.com Inc. or its affiliates.
/// SPDX-License-Identifier: MIT

#include <cetl/rtti.hpp>
#include <cetl/unbounded_variant.hpp>
#include <libcyphal/types.hpp>

#include <gmock/gmock.h>
#include <gtest/gtest.h>
#include <string>
#include <utility>

namespace
{

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
        explicit MyConcrete(std::string name) : name_{std::move(name)} {}
    private:
        std::string what() const override { return "MyConcrete " + name_; }
        std::string name_;
    };
};

// MARK: Tests:

TEST_F(TestTypes, ImplementationCell)
{
    using ub_var = cetl::unbounded_variant<sizeof(MyConcrete)>;
    using MyCell = libcyphal::ImplementationCell<MyInterface, ub_var>;

    const MyConcrete my_concrete{"A"};
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

}  // namespace
