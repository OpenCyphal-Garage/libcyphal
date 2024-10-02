/// @copyright
/// Copyright (C) OpenCyphal Development Team  <opencyphal.org>
/// Copyright Amazon.com Inc. or its affiliates.
/// SPDX-License-Identifier: MIT

#ifndef LIBCYPHAL_APPLICATION_REGISTRY_VALUE_HPP_INCLUDED
#define LIBCYPHAL_APPLICATION_REGISTRY_VALUE_HPP_INCLUDED

#include <cetl/cetl.hpp>
#include <cetl/pf17/cetlpf.hpp>

#include <cassert>
#include <uavcan/_register/Value_1_0.hpp>

#include <cstring>
#include <type_traits>

namespace libcyphal
{
namespace application
{
namespace registry
{

using Value = uavcan::_register::Value_1_0;

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
        for (std::size_t i = 0; i < std::min(a.value.size(), b.value.size()); i++)
        {
            // NOLINTNEXTLINE(bugprone-signed-char-misuse,cert-str34-c)
            a.value[i] = static_cast<AE>(b.value[i]);
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

    template <typename... A>  // Catch-all dummy -- variadic templates have the lowest priority.
    bool operator()(A&&...) const
    {
        return false;
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
template <typename T, std::size_t N>
struct ArraySelector<
    T,
    N,
    std::enable_if_t<std::is_same<typename std::decay_t<decltype(std::declval<Value::VariantType>().emplace<N>())>::
                                      _traits_::TypeOf::value::value_type,
                                  T>::value &&
                     !isVariableSize<std::decay_t<decltype(std::declval<Value::VariantType>().emplace<N>())>>()>>
    final
{
    static constexpr std::size_t Index = N;
};
static_assert(ArraySelector<bool>::Index == Value::VariantType::IndexOf::bit);
static_assert(ArraySelector<std::uint8_t>::Index == Value::VariantType::IndexOf::natural8);
static_assert(ArraySelector<std::uint32_t>::Index == Value::VariantType::IndexOf::natural32);
static_assert(ArraySelector<std::int8_t>::Index == Value::VariantType::IndexOf::integer8);
static_assert(ArraySelector<std::int64_t>::Index == Value::VariantType::IndexOf::integer64);
static_assert(ArraySelector<double>::Index == Value::VariantType::IndexOf::real64);

/// Callable that converts a Value into an array of the specified size and element type.
///
/// If the source array is longer, extra items are ignored (truncated);
/// if shorter, the remaining items are zeroed.
///
template <typename T, std::size_t N>
struct ArrayGetter final
{
    using Result = cetl::optional<std::array<T, N>>;
    template <typename S, typename = decltype(static_cast<T>(std::declval<S>().value[0]))>
    Result operator()(const S& src) const
    {
        std::array<T, N> out{};
        if constexpr (N > 0)
        {
            for (std::size_t i = 0; i < N; i++)
            {
                out.at(i) = (i < src.value.size()) ? static_cast<T>(src.value[i]) : T{};
            }
        }
        (void) src;
        return out;
    }
    Result operator()(const uavcan::primitive::Empty_1_0&) const
    {
        return std::nullopt;
    }
    Result operator()(const uavcan::primitive::Unstructured_1_0&) const
    {
        return std::nullopt;
    }
    Result operator()(const uavcan::primitive::String_1_0&) const
    {
        return std::nullopt;
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
template <typename T, typename = std::enable_if_t<std::is_arithmetic_v<T>>>
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
    auto& arr = dst.union_value.emplace<Index>();
    // TODO: use reserve() if the iterator is random access or size() is defined to improve performance and heap use.
    std::copy(std::begin(src), std::end(src), std::back_inserter(arr.value));
}

/// Assigns scalar values of various arithmetic types to the value.
///
/// E.g., passing a float here will switch the value to real32[1].
/// The existing content of the value is discarded.
///
template <typename T>
std::enable_if_t<std::is_arithmetic_v<T>> set(Value& dst, const T src)
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

template <typename T>
Value makeValue(Value::allocator_type& allocator, const T& src)
{
    Value out{allocator};
    set(out, src);
    return out;
}

template <typename... Ts, typename = std::enable_if_t<(sizeof...(Ts) > 1)>>
Value makeValue(Value::allocator_type& allocator, const Ts&... src)
{
    Value out{allocator};
    set(out, std::array<std::common_type_t<Ts...>, sizeof...(Ts)>{{static_cast<std::common_type_t<Ts...>>(src)...}});
    return out;
}

}  // namespace registry
}  // namespace application
}  // namespace libcyphal

#endif  // LIBCYPHAL_APPLICATION_REGISTRY_VALUE_HPP_INCLUDED
