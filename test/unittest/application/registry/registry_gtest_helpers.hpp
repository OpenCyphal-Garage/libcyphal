/// @copyright
/// Copyright (C) OpenCyphal Development Team  <opencyphal.org>
/// Copyright Amazon.com Inc. or its affiliates.
/// SPDX-License-Identifier: MIT

#ifndef LIBCYPHAL_APPLICATION_REGISTRY_GTEST_HELPERS_HPP_INCLUDED
#define LIBCYPHAL_APPLICATION_REGISTRY_GTEST_HELPERS_HPP_INCLUDED

#include <libcyphal/application/registry/registry.hpp>

#include <uavcan/_register/Value_1_0.hpp>

#include <gtest/gtest-matchers.h>
#include <gtest/gtest-printers.h>

#include <ostream>

// NOLINTBEGIN(cppcoreguidelines-avoid-magic-numbers, readability-magic-numbers)

namespace uavcan
{
namespace primitive
{

inline void PrintTo(const Empty_1_0&, std::ostream* os)
{
    *os << "Empty_1_0";
}
inline void PrintTo(const String_1_0& str, std::ostream* os)
{
    *os << "String_1_0{'" << cetl::string_view{reinterpret_cast<const char*>(str.value.data()), str.value.size()}
        << "'}";
    ;
}
inline void PrintTo(const Unstructured_1_0& data, std::ostream* os)
{
    *os << "Unstructured_1_0{cnt=" << data.value.size() << "}";
}

namespace array
{

inline void PrintTo(const Bit_1_0&, std::ostream* os)
{
    *os << "Bit_1_0[]";
}
inline void PrintTo(const Integer64_1_0&, std::ostream* os)
{
    *os << "Integer64_1_0[]";
}
inline void PrintTo(const Integer32_1_0&, std::ostream* os)
{
    *os << "Integer32_1_0[]";
}
inline void PrintTo(const Integer16_1_0&, std::ostream* os)
{
    *os << "Integer16_1_0[]";
}
inline void PrintTo(const Integer8_1_0&, std::ostream* os)
{
    *os << "Integer8_1_0[]";
}
inline void PrintTo(const Natural64_1_0&, std::ostream* os)
{
    *os << "Natural64_1_0[]";
}
inline void PrintTo(const Natural32_1_0&, std::ostream* os)
{
    *os << "Natural32_1_0[]";
}
inline void PrintTo(const Natural16_1_0&, std::ostream* os)
{
    *os << "Natural16_1_0[]";
}
inline void PrintTo(const Natural8_1_0&, std::ostream* os)
{
    *os << "Natural8_1_0[]";
}
inline void PrintTo(const Real64_1_0&, std::ostream* os)
{
    *os << "Real64_1_0[]";
}
inline void PrintTo(const Real32_1_0&, std::ostream* os)
{
    *os << "Real32_1_0[]";
}
inline void PrintTo(const Real16_1_0&, std::ostream* os)
{
    *os << "Real16_1_0[]";
}

}  // namespace array
}  // namespace primitive

namespace _register
{

inline void PrintTo(const Value_1_0& value, std::ostream* os)
{
    *os << "Value_1_0{";
    cetl::visit([os](const auto& v) { PrintTo(v, os); }, value.union_value);
    *os << "}";
}

}  // namespace _register
}  // namespace uavcan

namespace libcyphal
{
namespace application
{
namespace registry
{

// MARK: - GTest Printers:

// MARK: - GTest Matchers:

class RegisterValueMatcher
{
public:
    using is_gtest_matcher = void;

    explicit RegisterValueMatcher(const IRegister::Value& value)
        : value_{value}
    {
    }

    bool MatchAndExplain(const IRegister::Value& value, std::ostream*) const
    {
        return value_.union_value.index() == value.union_value.index();
    }
    void DescribeTo(std::ostream* os) const
    {
        *os << "is " << testing::PrintToString(value_);
    }
    void DescribeNegationTo(std::ostream* os) const
    {
        *os << "is NOT " << testing::PrintToString(value_);
    }

private:
    const IRegister::Value value_;
};
inline testing::Matcher<const IRegister::Value&> RegisterValueEq(const IRegister::Value& value)
{
    return RegisterValueMatcher(value);

}  // RegisterValueMatcher

}  // namespace registry
}  // namespace application
}  // namespace libcyphal

// NOLINTEND(cppcoreguidelines-avoid-magic-numbers, readability-magic-numbers)

#endif  // LIBCYPHAL_APPLICATION_REGISTRY_GTEST_HELPERS_HPP_INCLUDED
