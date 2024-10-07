/// @copyright
/// Copyright (C) OpenCyphal Development Team  <opencyphal.org>
/// Copyright Amazon.com Inc. or its affiliates.
/// SPDX-License-Identifier: MIT

#ifndef LIBCYPHAL_APPLICATION_REGISTRY_VALUE_HPP_INCLUDED
#define LIBCYPHAL_APPLICATION_REGISTRY_VALUE_HPP_INCLUDED

#include <cetl/cetl.hpp>
#include <cetl/pf17/cetlpf.hpp>
#include <cetl/pf20/cetlpf.hpp>

#include <cassert>
#include <uavcan/_register/Value_1_0.hpp>

#include <cstdlib>
#include <cstring>
#include <limits>
#include <type_traits>

namespace libcyphal
{
namespace application
{
namespace registry
{

/// Defines the value of a register.
///
/// Internally, it's implemented as a variant of all possible types (see `union_value` member).
///
using Value = uavcan::_register::Value_1_0;

/// Internal implementation details of the Presentation layer.
/// Not supposed to be used directly by the users of the library.
///
namespace detail
{

/// True iff the value is of a resizable type such as string or unstructured.
template <typename T>
constexpr bool isVariableSize()
{
    using A = std::decay_t<T>;
    return std::is_same<A, uavcan::primitive::String_1_0>::value ||  //
           std::is_same<A, uavcan::primitive::Unstructured_1_0>::value;
}
inline bool isVariableSize(const Value& v)
{
    return cetl::visit([](const auto& xxx) { return isVariableSize<decltype(xxx)>(); }, v.union_value);
}

struct Coercer final
{
    /// Copy the contents of one array into the other, with an explicit cast on each element.
    /// If the destination array is longer, leave the extra elements untouched.
    /// If the source array is longer, truncate the copy.
    ///
    template <typename A,
              typename B,
              typename AE = typename A::_traits_::TypeOf::value::value_type,
              typename    = typename B::_traits_::TypeOf::value::value_type>
    std::enable_if_t<(!isVariableSize<A>()) && (!isVariableSize<B>()), bool>  //
    operator()(A & a, const B & b) const
    {
        const std::size_t size = std::min(a.value.size(), b.value.size());
        for (std::size_t i = 0; i < size; i++)
        {
            a.value[i] = convert<AE>(b.value[i]);
        }
        return true;
    }

    template <typename U>
    std::enable_if_t<std::is_same<U, uavcan::primitive::String_1_0>::value ||
                         std::is_same<U, uavcan::primitive::Unstructured_1_0>::value ||
                         std::is_same<U, uavcan::primitive::Empty_1_0>::value,
                     bool>
    operator()(U& a, const U& b) const
    {
        a = b;
        return true;
    }

    bool operator()(uavcan::primitive::Unstructured_1_0& a, const uavcan::primitive::String_1_0& b) const
    {
        a.value.clear();
        a.value.reserve(b.value.size());
        for (const auto c : b.value)
        {
            a.value.push_back(static_cast<uavcan::primitive::Unstructured_1_0::_traits_::TypeOf::value::value_type>(c));
        }
        return true;
    }

    template <typename... A>       // Catch-all dummy -- variadic templates have the lowest priority.
    bool operator()(A&&...) const  // NOLINT cppcoreguidelines-missing-std-forward
    {
        return false;
    }

    template <typename To, typename From>
    static auto convert(const From& src)
        -> std::enable_if_t<std::is_same<To, bool>::value && std::is_floating_point<From>::value, To>
    {
        return std::abs(src) >= std::numeric_limits<From>::min();
    }

    template <typename To, typename From>
    static auto convert(const From& src)
        -> std::enable_if_t<!std::is_same<To, bool>::value || !std::is_floating_point<From>::value, To>
    {
        // NOLINTNEXTLINE(bugprone-signed-char-misuse,cert-str34-c)
        return static_cast<To>(src);
    }

};  // Coercer

/// Deduces the index of the numerical array-typed Value union member that can hold elements of type T.
///
/// It intentionally skips variable-size types (string and unstructured) because they are handled separately.
///
template <typename T, std::size_t N = 0, typename = void>
struct ArraySelector final
{
    static constexpr std::size_t Index = ArraySelector<T, N + 1>::Index;
};
static const cetl::pmr::polymorphic_allocator<void> xxx_alloc{cetl::pmr::null_memory_resource()};

template <typename T, std::size_t N>
struct ArraySelector<
    T,
    N,
    std::enable_if_t<
        std::is_same<typename std::decay_t<decltype(std::declval<Value::VariantType>().emplace<N>(
                         xxx_alloc))>::_traits_::TypeOf::value::value_type,
                     T>::value &&
        !isVariableSize<std::decay_t<decltype(std::declval<Value::VariantType>().emplace<N>(xxx_alloc))>>()>> final
{
    static constexpr std::size_t Index = N;
};
static_assert(ArraySelector<bool>::Index == Value::VariantType::IndexOf::bit, "Expected bool index");
static_assert(ArraySelector<std::uint8_t>::Index == Value::VariantType::IndexOf::natural8, "Expected uint8 index");
static_assert(ArraySelector<std::uint32_t>::Index == Value::VariantType::IndexOf::natural32, "Expected uint32 index");
static_assert(ArraySelector<std::int8_t>::Index == Value::VariantType::IndexOf::integer8, "Expected int8 index");
static_assert(ArraySelector<std::int64_t>::Index == Value::VariantType::IndexOf::integer64, "Expected uint64 index");
static_assert(ArraySelector<double>::Index == Value::VariantType::IndexOf::real64, "Expected double index");

/// Callable that converts a Value into an array of the specified size and element type.
///
/// If the source array is longer, extra items are ignored (truncated);
/// if shorter, the remaining items are zeroed.
///
template <typename T, std::size_t N>
struct ArrayGetter final
{
    using Result = cetl::optional<std::array<T, N>>;

    template <typename S, typename = decltype(Coercer::convert<T>(std::declval<S>().value[0]))>
    Result operator()(const S& src) const
    {
        std::array<T, N> out{};
        for (std::size_t i = 0; i < N; i++)
        {
            out.at(i) = (i < src.value.size()) ? Coercer::convert<T>(src.value[i]) : T{};
        }
        return out;
    }

    Result operator()(const uavcan::primitive::Empty_1_0&) const
    {
        return cetl::nullopt;
    }

    Result operator()(const uavcan::primitive::Unstructured_1_0&) const
    {
        return cetl::nullopt;
    }

    Result operator()(const uavcan::primitive::String_1_0&) const
    {
        return cetl::nullopt;
    }
};
template <typename T>
struct ArrayGetter<T, 0> final
{
    using Result = cetl::optional<std::array<T, 0>>;

    template <typename S>
    Result operator()(const S&) const
    {
        return std::array<T, 0>{};
    }

    Result operator()(const uavcan::primitive::Empty_1_0&) const
    {
        return cetl::nullopt;
    }

    Result operator()(const uavcan::primitive::Unstructured_1_0&) const
    {
        return cetl::nullopt;
    }

    Result operator()(const uavcan::primitive::String_1_0&) const
    {
        return cetl::nullopt;
    }

};  // ArrayGetter

/// Add more specializations here to support additional return types.
template <typename T, std::size_t N>
CETL_NODISCARD bool get(const Value& src, std::array<T, N>& dst)
{
    if (const auto res = visit(ArrayGetter<T, N>(), src.union_value))
    {
        dst = *res;
        return true;
    }
    return false;
}

template <typename T, typename = std::enable_if_t<std::is_arithmetic<T>::value>>
CETL_NODISCARD bool get(const Value& src, T& dst)
{
    std::array<T, 1> tmp{};
    if (get(src, tmp))
    {
        dst = tmp.front();
        return true;
    }
    return false;
}

}  // namespace detail

/// Convert the value stored in a source to the same type and dimensionality as destination; update destination
/// in-place. The function performs aggressive conversions which may result in a loss of precision or truncation.
///
/// @return True on success, false if no suitable conversion is known (in which case, the destination is not modified).
///         Empty is not convertible -- false is always returned if either (not both) of the registers are empty.
///         If the destination is a string or unstructured, its length is set to that of the source.
///         Otherwise, the length of the destination is not modified, and the source is truncated if necessary;
///         if the source is shorter, the last elements of the destination are left unmodified.
///
inline bool coerce(Value& destination, const Value& source)
{
    return visit(detail::Coercer(), destination.union_value, source.union_value);
}

// MARK: - Get

/// Applies best effort to convert the contained value to the specified type,
/// which may be a scalar or a std::array<>, and returns it by value (lifetime detached).
/// An empty option is returned if the value is not convertible to the specified type.
///
/// The type may also be a `const char*` (works only for 'string'),
/// in which case the result is a reference whose lifetime is tied to the original value.
///
/// The elements will be converted as necessary (e.g., float to uint8), which may cause overflow or truncation.
/// For arrays, extra elements will be truncated, and missing elements will be default-initialized (zeroed).
///
template <typename T>
cetl::optional<T> get(const Value& src)
{
    T out{};
    if (detail::get(src, out))
    {
        return out;
    }
    return cetl::nullopt;
}

// MARK: - Set

/// Assigns an unstructured value by copying the raw memory contents into the value.
///
/// Extra data truncated.
///
inline void set(Value& dst, const cetl::span<const cetl::byte> value)
{
    auto& unstructured = dst.set_unstructured();
    // TODO: Fix Nunavut to expose `ARRAY_CAPACITY` so we can use it here instead of 256.
    // NOLINTNEXTLINE(cppcoreguidelines-avoid-magic-numbers, readability-magic-numbers)
    unstructured.value.resize(std::min<std::size_t>(256U, value.size_bytes()));
    (void) std::memmove(unstructured.value.data(), value.data(), unstructured.value.size());
}

/// Assigns string to the value, truncating if necessary.
///
/// The existing content of the value is discarded.
///
inline void set(Value& dst, const char* const string)
{
    auto& str = dst.set_string();

    const std::size_t str_len = std::strlen(string);
    str.value.reserve(str_len);
    for (std::size_t i = 0; i < std::min(str_len, str.value.capacity()); i++)
    {
        // NOLINTNEXTLINE(cppcoreguidelines-pro-bounds-pointer-arithmetic)
        str.value.push_back(static_cast<std::uint8_t>(string[i]));
    }
}

/// Assigns numerical arrays/vectors of various arithmetic types to the value.
///
/// E.g., passing a std::array<float, 7> here will switch the value to real32[7].
/// The existing content of the value is discarded.
///
template <typename Container,
          typename T        = std::decay_t<decltype(*std::begin(std::declval<Container>()))>,
          std::size_t Index = detail::ArraySelector<T>::Index>
void set(Value& dst, const Container& src)
{
    auto& arr = dst.union_value.emplace<Index>(dst.allocator_);
    // TODO: use reserve() if the iterator is random access or size() is defined to improve performance and heap use.
    std::copy(std::begin(src), std::end(src), std::back_inserter(arr.value));
}

/// Assigns scalar values of various arithmetic types to the value.
///
/// E.g., passing a float here will switch the value to real32[1].
/// The existing content of the value is discarded.
///
template <typename T>
std::enable_if_t<std::is_arithmetic<T>::value> set(Value& dst, const T src)
{
    set(dst, std::array<T, 1>{{src}});
}

/// Assigns one value to another.
///
inline void set(Value& dst, const Value& src)
{
    dst = src;
}

// MARK: - Factories

/// Makes a new value with the specified string content.
///
/// @param allocator The PMR allocator to allocate storage for the value.
/// @param str The string to copy into the value.
///
inline Value makeValue(const Value::allocator_type& allocator, const char* const str)
{
    Value out{allocator};
    set(out, str);
    return out;
}

/// Makes a new value with the specified content.
///
/// This factory method automatically selects the appropriate type of the value based on the source type.
///
/// @tparam T Source type of the content.
/// @param allocator The PMR allocator to allocate storage for the value.
/// @param src The string to copy into the value.
///
template <typename T>
Value makeValue(const Value::allocator_type& allocator, const T& src)
{
    Value out{allocator};
    set(out, src);
    return out;
}

/// Makes a new array value with the specified content which is provided as a list of arguments.
///
/// This factory method automatically selects the appropriate type of the value based on a source type.
/// The source type is deduced from the arguments as a common type to which all arguments can be cast-ed.
///
/// @tparam Ts Source types of the arguments.
/// @param allocator The PMR allocator to allocate storage for the value.
/// @param args The list of arguments to copy into the array value.
///
template <typename... Ts, typename = std::enable_if_t<(sizeof...(Ts) > 1)>>
Value makeValue(const Value::allocator_type& allocator, const Ts&... args)
{
    Value out{allocator};
    set(out, std::array<std::common_type_t<Ts...>, sizeof...(Ts)>{{static_cast<std::common_type_t<Ts...>>(args)...}});
    return out;
}

}  // namespace registry
}  // namespace application
}  // namespace libcyphal

#endif  // LIBCYPHAL_APPLICATION_REGISTRY_VALUE_HPP_INCLUDED
