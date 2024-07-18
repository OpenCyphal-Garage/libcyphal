/// @copyright
/// Copyright (C) OpenCyphal Development Team  <opencyphal.org>
/// Copyright Amazon.com Inc. or its affiliates.
/// SPDX-License-Identifier: MIT

#ifndef LIBCYPHAL_CETL_GTEST_HELPERS_HPP_INCLUDED
#define LIBCYPHAL_CETL_GTEST_HELPERS_HPP_INCLUDED

#include <cetl/pf17/cetlpf.hpp>
#include <cetl/pf20/cetlpf.hpp>
#include <cetl/rtti.hpp>
#include <cetl/unbounded_variant.hpp>

#include <gmock/gmock.h>
#include <gtest/gtest-matchers.h>

#include <cstddef>
#include <cstdint>
#include <iomanip>
#include <ios>
#include <ostream>
#include <string>

// MARK: - GTest Printers:

namespace std
{
#if (__cplusplus >= CETL_CPP_STANDARD_17)
inline std::ostream& operator<<(std::ostream& os, const std::byte& b)
{
    return os << "0x" << std::uppercase << std::hex << std::setw(2) << std::setfill('0')
              << static_cast<std::uint32_t>(b);
}
#endif
#if (__cplusplus >= CETL_CPP_STANDARD_20)
template <typename T>
inline std::ostream& operator<<(std::ostream& os, const std::span<T>& items)
{
    os << "{size=" << items.size() << ", data=[";
    for (const auto& item : items)
    {
        os << testing::PrintToString(item) << ", ";
    }
    return os << ")}";
}
#endif
}  // namespace std

namespace cetl
{

namespace pf17
{
inline std::ostream& operator<<(std::ostream& os, const cetl::byte& b)
{
    return os << "0x" << std::uppercase << std::hex << std::setw(2) << std::setfill('0')
              << static_cast<std::uint32_t>(b);
}
}  // namespace pf17
namespace pf20
{
template <typename T>
inline std::ostream& operator<<(std::ostream& os, const cetl::span<T>& items)
{
    os << "{size=" << items.size() << ", data=[";
    for (const auto& item : items)
    {
        os << testing::PrintToString(item) << ", ";
    }
    return os << ")}";
}
}  // namespace pf20

inline void PrintTo(const type_id& id, std::ostream* os)
{
    // Prints the type ID as a hexadecimal number.
    // Like "344D3B67-4E67-4CD5-9F80-E5F069BB563E".
    //
    int cnt = 0;
    *os << std::uppercase << std::hex << std::setfill('0');
    for (const std::uint32_t item : id)
    {
        *os << std::setw(2) << item;
        // NOLINTNEXTLINE(cppcoreguidelines-avoid-magic-numbers, readability-magic-numbers)
        if ((cnt == 3) || (cnt == 5) || (cnt == 7) || (cnt == 9))
        {
            *os << '-';
        }
        ++cnt;
    }
}

template <std::size_t Footprint, bool Copyable, bool Movable, std::size_t Alignment, typename Pmr>
inline void PrintTo(const unbounded_variant<Footprint, Copyable, Movable, Alignment, Pmr>& ub_var, std::ostream* os)
{
    if (ub_var.valueless_by_exception())
    {
        *os << "{valueless_by_exception}";
        return;
    }

    *os << "{has_value=" << std::boolalpha << ub_var.has_value();
    *os << ", size=" << ub_var.type_size();
    *os << ", type_id='";
    PrintTo(ub_var.type_id(), os);
    *os << "'}";
}

}  // namespace cetl

// MARK: - GTest Matchers:

template <typename ValueType>
class UbVariantMatcher final
{
public:
    explicit UbVariantMatcher(testing::Matcher<const ValueType&> matcher)
        : matcher_(std::move(matcher))
    {
    }

    template <typename UbVariant>
    bool MatchAndExplain(const UbVariant& ub_var, testing::MatchResultListener* listener) const
    {
        auto value_ptr = cetl::get_if<ValueType>(&ub_var);

        if (!listener->IsInterested())
        {
            return value_ptr != nullptr && matcher_.Matches(*value_ptr);
        }

        if (value_ptr == nullptr)
        {
            *listener << "whose value is not of type_id='";
            cetl::PrintTo(cetl::type_id_value<ValueType>, listener->stream());
            *listener << "'";
            return false;
        }

        const ValueType&                   value = *value_ptr;
        testing::StringMatchResultListener value_listener;
        const bool                         match = matcher_.MatchAndExplain(value, &value_listener);
        *listener << "whose value " << testing::PrintToString(value) << (match ? " matches" : " doesn't match");
        PrintIfNotEmpty(value_listener.str(), listener->stream());
        return match;
    }

    void DescribeTo(std::ostream* os) const
    {
        *os << "is a unbounded_variant<> with value of type_id='";
        cetl::PrintTo(cetl::type_id_value<ValueType>, os);
        *os << "' and the value ";
        matcher_.DescribeTo(os);
    }

    void DescribeNegationTo(std::ostream* os) const
    {
        *os << "is a unbounded_variant<> with value of type_id other than '";
        cetl::PrintTo(cetl::type_id_value<ValueType>, os);
        *os << "' or the value ";
        matcher_.DescribeNegationTo(os);
    }

private:
    // If the explanation is not empty, prints it to the ostream.
    static void PrintIfNotEmpty(const std::string& explanation, std::ostream* os)
    {
        if (explanation != "" && os != nullptr)
        {
            *os << ", " << explanation;
        }
    }

    const testing::Matcher<const ValueType&> matcher_;
};
template <typename T>
testing::PolymorphicMatcher<UbVariantMatcher<T>> UbVariantWith(const testing::Matcher<const T&>& matcher)
{
    return testing::MakePolymorphicMatcher(UbVariantMatcher<T>(matcher));
}

class UbVariantWithoutValueMatcher final
{
public:
    UbVariantWithoutValueMatcher() = default;

    template <typename UbVariant>
    bool MatchAndExplain(const UbVariant& ub_var, testing::MatchResultListener* listener) const
    {
        if (!listener->IsInterested())
        {
            return !ub_var.has_value();
        }

        if (ub_var.has_value())
        {
            *listener << "whose is not valueless";
            return false;
        }

        return true;
    }

    void DescribeTo(std::ostream* os) const
    {
        *os << "is a unbounded_variant<> without value";
    }

    void DescribeNegationTo(std::ostream* os) const
    {
        *os << "is a unbounded_variant<> with a value";
    }
};
inline testing::PolymorphicMatcher<UbVariantWithoutValueMatcher> UbVariantWithoutValue()
{
    return testing::MakePolymorphicMatcher(UbVariantWithoutValueMatcher());
}

#endif  // LIBCYPHAL_CETL_GTEST_HELPERS_HPP_INCLUDED
