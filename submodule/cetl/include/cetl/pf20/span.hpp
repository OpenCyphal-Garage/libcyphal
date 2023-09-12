/// @file
/// Defines a span type that is mostly compliant to ISO/IEC 14882:2020(E) but compatible with C++14 and newer.
///
/// @copyright
/// Copyright (C) OpenCyphal Development Team  <opencyphal.org>
/// Copyright Amazon.com Inc. or its affiliates.
/// SPDX-License-Identifier: MIT
///

#ifndef CETL_PF20_SPAN_H_INCLUDED
#define CETL_PF20_SPAN_H_INCLUDED

#include <array>
#include <cstdint>
#include <iterator>
#include <limits>
#include <type_traits>

#ifndef CETL_H_ERASE
#    include "cetl/cetl.hpp"
#endif

namespace cetl
{
namespace pf20
{

///
/// Used by span to indicate that the span size is not fixed.
/// @see std::dynamic_extent
constexpr std::size_t dynamic_extent = std::numeric_limits<std::size_t>::max();

/// A borrowed view into a contiguous set of objects. Spans can either be static, where the set of objects is fixed
/// and known, or dynamic where the number of objects in the contiguous set may change.
/// This template is compatible with class: std::span available in C++20.
template <typename T, std::size_t Extent = dynamic_extent>
class span;

///
/// This version is for spans where the extent is static (see span< T, dynamic_extent > for the dynamic
/// extent specialization)
///
/// @par Example
/// Creating a stream operator for `cetl::span`...
/// @snippet{trimleft} example_01_span_static.cpp global
/// ...enables trivial printing of substrings without allocation of a new buffer.
/// @snippet{trimleft} example_01_span_static.cpp main
///
/// @tparam T       The element type.
/// @tparam Extent  The extent type of this span; either dynamic or static.
template <typename T, std::size_t Extent>
class span
{
public:
    //
    // +----------------------------------------------------------------------+
    // | Member Constants
    // +----------------------------------------------------------------------+
    ///
    /// The value of Extent for this template instantiation.
    static constexpr std::size_t extent = Extent;

    // +----------------------------------------------------------------------+
    // | Member Types
    // +----------------------------------------------------------------------+
    using element_type     = T;                                 //!< The element type over which the span operates
    using value_type       = typename std::remove_cv<T>::type;  //!< The non-cv type of elements
    using pointer          = T*;                                //!< The element pointer type
    using const_pointer    = const T*;                          //!< A constant pointer to the element type
    using reference        = T&;                                //!< A reference type to the element
    using const_reference  = const T&;                          //!< A constant reference type to the element
    using size_type        = std::size_t;                       //!< Used to track the size in bytes
    using difference_type  = std::ptrdiff_t;                    //!< Type used to compare two locations in the span.
    using iterator         = pointer;                           //!< Type for iterating through the span.
    using reverse_iterator = std::reverse_iterator<iterator>;   //!< Reverse iterator type.

    // +----------------------------------------------------------------------+
    // | Required Properties
    // +----------------------------------------------------------------------+
    static_assert(not std::is_pointer<T>::value, "Can not be a pointer type");
    static_assert(sizeof(element_type) != 0u, "Must have non zero element size");

    // +----------------------------------------------------------------------+
    ///@{ @name Constructors
    // +----------------------------------------------------------------------+

    /// Default constructor
    /// @tparam DeducedExtent   The Extent deduced by the compiler.
    /// @tparam type            SFNAE enablement of this constructor only where the extent > 0.
    /// @see std::span::span()
    template <std::size_t DeducedExtent = Extent, typename std::enable_if<(DeducedExtent == 0), bool>::type = true>
    constexpr span() noexcept
        : data_{nullptr}
    {
    }

    /// Creates a span starting at an element for a given length.
    /// The span's view into the data starts at the element pointed to by the first pointer. The size of the
    /// span is provided by the count where it is undefined behavior to provide a count value that is not the
    /// same as span::extent.
    /// @param first The first element in the span.
    /// @param count The number of elements in the span.
    /// @see std::span::span()
    explicit constexpr span(iterator first, size_type count)
        : data_{first}
    {
        (void) count;  // Count isn't actually used in static spans.
        CETL_DEBUG_ASSERT(count == extent,
                          "CDE_span_001: Constructing a fixed span where the Extent parameter is different from "
                          "the count passed into this constructor.");
    }

    /// Creates a span starting at an element to the element before the given end.
    /// That is, `span == [first, end)`. It is undefined to provide iterators where `end - first != Extent`
    /// @tparam EndType End iterator type is deduced to support SFNAE pattern.
    /// @tparam type Participates in overload resolution only if the EndType cannot be converted to size_type.
    /// @param first    The first element in the span.
    /// @param end      The element after the last element in the span.
    /// @see std::span::span()
    template <typename EndType,
              typename std::enable_if<!std::is_convertible<EndType, std::size_t>::value, bool>::type = true>
    explicit constexpr span(pointer first, EndType end)
        : data_(first)
    {
        (void) end;  // Last isn't actually used in static spans.
        CETL_DEBUG_ASSERT(std::distance(first, end) == Extent, "CDE_span_002: Iterator range does not equal Extent.");
    }

    /// Creates a span starting at the first element of a c-style array through to the end of that array.
    /// @param arr  Reference to a C-style array.
    /// @see std::span::span()
    template <std::size_t DeducedExtent = Extent, typename std::enable_if<(DeducedExtent != 0), bool>::type = true>
    constexpr span(element_type (&arr)[DeducedExtent]) noexcept
        : data_(arr)
    {
    }

    /// Creates a span over an entire `std::array` starting at the array's first element.
    /// @tparam ArrayElementType Deduced array element type to support SFNAE enablement.
    /// @tparam type    Enables this override only if the array's element type is the same as the span's or if the
    ///                 conversion is a simple [qualification
    ///                 conversion](https://en.cppreference.com/w/cpp/language/implicit_conversion#Qualification_conversions).
    /// @param arr      The array to create the view into.
    /// @see std::span::span()
    template <typename ArrayElementType,
              typename std::enable_if<std::is_convertible<ArrayElementType (*)[], T (*)[]>::value, bool>::type = true>
    constexpr span(std::array<ArrayElementType, Extent>& arr) noexcept
        : data_(arr.data())
    {
    }

    /// Creates a span over an entire `const std::array` starting at the array's first element.
    /// @tparam ArrayElementType Deduced array element type to support SFNAE enablement.
    /// @tparam type    Enables this override only if the array's element type is the same as the span's or if the
    ///                 conversion is a simple [qualification
    ///                 conversion](https://en.cppreference.com/w/cpp/language/implicit_conversion#Qualification_conversions).
    /// @param arr      The array to create the view into.
    /// @see std::span::span()
    template <
        typename ArrayElementType,
        typename std::enable_if<std::is_convertible<const ArrayElementType (*)[], T (*)[]>::value, bool>::type = true>
    constexpr span(const std::array<ArrayElementType, Extent>& arr) noexcept
        : data_(arr.data())
    {
    }

    // TODO: C++20 range constructor.
    // template< class R >
    // explicit(extent != std::dynamic_extent)
    // constexpr span( R&& range );

    /// Copy constructor to create a span from another span. This overload allows conversion of a dynamic span of size()
    /// N to a static span with an extent of N.
    /// It is undefined to provide a source span with a size() != this span's size.
    /// @tparam DeducedElementType  The element type of the source span.
    /// @tparam type                Enables this override only if the element conversion is a simple [qualification
    ///                 conversion](https://en.cppreference.com/w/cpp/language/implicit_conversion#Qualification_conversions).
    /// @param source The span to copy from. The resulting span has `size() == source.size()` and `data() ==
    ///               source.data()`
    /// @see std::span(std::span&)
    template <typename DeducedElementType,
              typename std::enable_if<std::is_convertible<DeducedElementType (*)[], element_type (*)[]>::value,
                                      bool>::type = true>
    explicit constexpr span(const span<DeducedElementType, dynamic_extent>& source) noexcept
        : data_(source.data())
    {
        CETL_DEBUG_ASSERT(extent == source.size(),
                          "CDE_span_003: providing a dynamic span with a size different from this static span's.");
    }

    /// Copy constructor to create a span from another span.
    /// @tparam DeducedElementType  The element type of the source span.
    /// @tparam type                Enables this override only if the element conversion is a simple [qualification
    ///                 conversion](https://en.cppreference.com/w/cpp/language/implicit_conversion#Qualification_conversions).
    /// @param source The span to copy from. The resulting span has `size() == source.size()` and
    ///               `data() == source.data()`. For this overload `extent == source::extent` is also true and
    ///               `extent != dynamic_extent`.
    /// @see std::span()
    template <typename DeducedElementType,
              typename std::enable_if<std::is_convertible<DeducedElementType (*)[], element_type (*)[]>::value,
                                      bool>::type = true>
    constexpr span(const span<DeducedElementType, Extent>& source) noexcept
        : data_(source.data())
    {
    }

    /// Default copy constructor
    /// @see std::span::span()
    constexpr span(const span&) noexcept = default;

    ///@}
    // +----------------------------------------------------------------------+
    ///@{ @name Operators
    // +----------------------------------------------------------------------+
    /// Default assignment operator.
    /// @param rhs    Right-hand-side of the assignment expression.
    /// @return reference to `*this`
    /// @see std::span::operator=()
    constexpr span& operator=(const span& rhs) noexcept = default;

    ///
    /// Reference to an element in the span.
    /// @param idx  The 0-based index in the span. The behavior of this method is
    ///             undefined if idx is >= to size.
    /// @return A reference to an element.
    /// @see std::span::operator[]()
    constexpr reference operator[](size_type idx) const
    {
        CETL_DEBUG_ASSERT((idx < Extent), "CDE_span_004: Indexing outside of the span is undefined.");
        CETL_DEBUG_ASSERT((data_ != nullptr), "CDE_span_005: Indexing with non data (nullptr) is undefined.");
        return data_[idx];
    }

    ///@}
    // +----------------------------------------------------------------------+
    ///@{ @name Iterators
    // +----------------------------------------------------------------------+

    /// Iterator to the first element in the span. This is the same as span::end()
    /// if the size of the span is 0.
    /// @return normal iterator.
    /// @see std::span::begin
    constexpr iterator begin() const noexcept
    {
        return iterator(&data_[0]);
    }

    /// Iterator to the address after the last element in the span.
    /// @code{.unparsed}
    ///                     end
    ///                      v
    ///    +-----------------------+
    ///    | 0 | 1 | 2 | 3 | x | x |
    ///    +-----------------------+
    ///
    /// @endcode
    /// @return normal iterator.
    /// @see std::span::end
    constexpr iterator end() const noexcept
    {
        return iterator(&data_[Extent]);
    }

    /// Reverse iterator to the last element in the span.
    /// This is the same as span::rend if the span size is 0.
    /// @return reverse iterator after the end of the span.
    /// @see std::span::rbegin
    constexpr reverse_iterator rbegin() const noexcept
    {
        return reverse_iterator(end());
    }

    /// Reverse iterator to the address before the first element in the span.
    /// @code{.unparsed}
    /// rend
    ///  v
    ///    +-----------------------+
    ///    | 0 | 1 | 2 | 3 | x | x |
    ///    +-----------------------+
    ///
    /// @endcode
    /// @return reverse iterator from the begining of the span.
    /// @see std::span::rend
    constexpr reverse_iterator rend() const noexcept
    {
        return reverse_iterator(begin());
    }

    ///@}
    // +----------------------------------------------------------------------+
    ///@{ @name Element Access
    // +----------------------------------------------------------------------+

    ///
    /// Returns a reference to the first element.
    /// Calling this method on an empty span is undefined.
    /// @return Reference to the first element. This is the same as `*obj.begin()`.
    /// @see std::span::front
    constexpr reference front() const
    {
        CETL_DEBUG_ASSERT((Extent != 0), "CDE_span_006: Calling front on an empty span is undefined.");
        return *begin();
    }

    ///
    /// Returns a reference to the last element.
    /// Calling this method on an empty span is undefined.
    /// @return Reference to the last element. This is the same as `*(obj.end() - 1)`.
    /// @see std::span::back
    constexpr reference back() const
    {
        CETL_DEBUG_ASSERT((Extent != 0), "CDE_span_007: Calling back on an empty span is undefined.");
        return data_[Extent - 1];
    }

    ///
    /// Provides access to the internal data the span is a view into.
    /// @return A pointer to the begining of the span sequence.
    /// @see std::span::data
    constexpr pointer data() const noexcept
    {
        return data_;
    }

    ///@}
    // +----------------------------------------------------------------------+
    ///@{ @name Observers
    // +----------------------------------------------------------------------+

    ///
    /// If the span has a zero size or not.
    /// @return true if the span size is 0 where "size" is the same as span::extent
    /// for this specialization.
#if (__cplusplus >= 201703L)
    [[nodiscard]]
#endif
    constexpr bool
    empty() const noexcept
    {
        return (Extent == 0);
    }

    ///
    /// The size of the span.
    /// @return The number of elements in the span which is equal to `span::extent`.
    /// @see std::span::size
    constexpr size_type size() const noexcept
    {
        return Extent;
    }

    ///
    /// The size of the span in bytes.
    /// @return `sizeof(element_type) * size_`.
    /// @see std::span::size_bytes
    constexpr size_type size_bytes() const noexcept
    {
        return sizeof(element_type) * Extent;
    }
    ///@}
    // +----------------------------------------------------------------------+
    ///@{ @name Subviews
    // +----------------------------------------------------------------------+
    /// Create a new span from the start of the current span for `Count` elements.
    /// @tparam Count   Number of elements for the sub-span.
    /// @return A new span with the static extent `Count`.
    /// @see std::span::first
    template <size_type Count>
    constexpr span<element_type, Count> first() const
    {
        static_assert(Count <= Extent, "subviews beyond the size of the span's view are not allowed.");
        return span<element_type, Count>(data_, Count);
    }

    /// Create a new span from the start of the current span for count elements.
    /// @param count  The number of elements for the sub-span. Where `count` is greater than the
    ///               current span's size the behavior is undefined.
    /// @return A new span with a dynamic extent extent and a size of `count`.
    /// @see std::span::first
    constexpr span<element_type, dynamic_extent> first(size_type count) const
    {
        CETL_DEBUG_ASSERT(count <= Extent,
                          "CDE_span_008: Dynamic subviews beyond the size of the span's view are undefined.");
        return span<element_type, dynamic_extent>(data_, count);
    }

    /// Create a new span `Count` elements from the last item of the current span to its end.
    /// @tparam Count   Number of elements for the sub-span.
    /// @return A new span with the static extent `Count`.
    /// @see std::span::last
    template <size_type Count>
    constexpr span<element_type, Count> last() const
    {
        static_assert(Count <= extent, "subviews beyond the size of the span's view are not allowed.");
        return span<element_type, Count>{&data_[extent - Count], Count};
    }

    /// Create a new span `count` elements from the last item of the current span to its end.
    /// @param count   Number of elements for the sub-span. The behavior of this method is undefined where
    ///                `count` is greater than the current span's size.
    /// @return A new span with a size of `count` and a dynamic extent.
    /// @see std::span::last
    constexpr span<element_type, dynamic_extent> last(size_type count) const
    {
        CETL_DEBUG_ASSERT(count <= extent,
                          "CDE_span_009: Dynamic subviews beyond the size of the span's view are undefined.");
        return span<element_type, dynamic_extent>(&data_[extent - count], count);
    }

    /// Create a new span `Offset` elements from the start of the current span and for `Count` elements.
    /// @tparam Offset  Number of elements from the start of the current span for the subspan.
    /// @tparam Count   The number of elements from the Offset to include in the subspan. If this value is
    ///                 `dynamic_extent` then the Count is `size() - Offset`.
    /// @return A new span where the `Extent` is `Count` if this was not `dynamic_extent` otherwise the
    ///         new span's `Extent` is this span's `Extent` minus `Offset`.
    /// @see std::span::subspan()
    template <size_type Offset, size_type Count = dynamic_extent>
    constexpr span<element_type, (Count != dynamic_extent) ? Count : Extent - Offset> subspan() const
    {
        static_assert(Offset <= extent, "subspan Offsets > extent are ill-formed.");
        static_assert((Count == dynamic_extent) || (Count <= extent - Offset), "subspan Count argument is ill-formed");
        return span < element_type,
               Count != dynamic_extent
                   ? Count
                   : extent - Offset > {&data_[Offset], Count == dynamic_extent ? size() - Offset : Count};
    }

    /// Create a new span `Offset` elements from the start of the current span and for either `Count` elements or,
    /// if Count is `dynamic_extent`, for the remaining size of the span (i.e. `size() - Offset`).
    /// The behavior of this method is undefined where `Count` is greater-than the size of this span or if
    /// `Count` is not dynamic_extent but `Count` is greater-than `size() - Offset`.
    /// @param Offset  Number of elements from the start of the current span for the subspan.
    /// @param Count   The number of elements from the Offset to include in the subspan. If this value is
    ///                `dynamic_extent` then the Count is `size() - Offset`.
    /// @return A new span with a dynamic extent.
    /// @see std::span::subspan()
    constexpr span<element_type, dynamic_extent> subspan(size_type Offset, size_type Count = dynamic_extent) const
    {
        CETL_DEBUG_ASSERT(Offset <= extent, "CDE_span_010: subspan Offsets > size() are ill-formed.");
        if (Count == dynamic_extent)
        {
            return {&data_[Offset], extent - Offset};
        }
        else
        {
            CETL_DEBUG_ASSERT(Count <= (extent - Offset), "CDE_span_011: subspan Count argument is ill-formed");
            return {&data_[Offset], Count};
        }
    }

    ///@}
private:
    pointer data_;
};

// required till C++ 17. Redundant but allowed after that.
template <typename T, std::size_t Extent>
const std::size_t span<T, Extent>::extent;

/// Specialization of span where the extent is dynamic.
/// @snippet{trimleft} example_01_span_dynamic.cpp main
template <typename T>
class span<T, dynamic_extent>
{
public:
    // (Groupings per https://en.cppreference.com/w/cpp/container/span)
    // +----------------------------------------------------------------------+
    // | Member Constants
    // +----------------------------------------------------------------------+
    /// The value of Extent for this template instantiation.
    static constexpr std::size_t extent = dynamic_extent;

    // +----------------------------------------------------------------------+
    // | Member Types
    // +----------------------------------------------------------------------+
    using element_type     = T;                                 //!< The element type over which the span operates
    using value_type       = typename std::remove_cv<T>::type;  //!< The non-cv type of elements
    using pointer          = T*;                                //!< The element pointer type
    using const_pointer    = const T*;                          //!< A constant pointer to the element type
    using reference        = T&;                                //!< A reference type to the element
    using const_reference  = const T&;                          //!< A constant reference type to the element
    using size_type        = std::size_t;                       //!< Used to track the size in bytes
    using difference_type  = std::ptrdiff_t;                    //!< Type used to compare two locations in the span.
    using iterator         = pointer;                           //!< Type for iterating through the span.
    using reverse_iterator = std::reverse_iterator<iterator>;   //!< Reverse iterator type.

    // +----------------------------------------------------------------------+
    // | Required Properties
    // +----------------------------------------------------------------------+
    static_assert(not std::is_pointer<T>::value, "Can not be a pointer type");
    static_assert(sizeof(element_type) != 0u, "Must have non zero element size");

    // +----------------------------------------------------------------------+
    ///@{ @name Constructors
    // +----------------------------------------------------------------------+

    /// Default constructor
    /// @see std::span::span()
    constexpr span() noexcept
        : data_{nullptr}
        , size_{0}
    {
    }

    /// Creates a span starting at an element for a given length.
    /// The span's view into the data starts at the element pointed to by the first pointer. The size of the
    /// span is provided by the count.
    /// @param first The first element in the span.
    /// @param count The number of elements in the span.
    /// @see std::span::span()
    constexpr span(iterator first, size_type count)
        : data_{first}
        , size_{count}
    {
    }

    /// Creates a span starting at an element to the element before the given end.
    /// That is, `span == [first, end)`. The size of the span becomes end - first.
    /// @tparam EndType End iterator type is deduced to support SFNAE pattern.
    /// @tparam type Participates in overload resolution only if the EndType cannot be converted to size_type.
    /// @param first    The first element in the span.
    /// @param end      The element after the last element in the span.
    /// @see std::span::span()
    template <typename EndType,
              typename std::enable_if<!std::is_convertible<EndType, size_type>::value, bool>::type = true>
    constexpr span(pointer first, EndType end)
        : data_{first}
        , size_{static_cast<size_type>(std::distance(first, end))}
    {
        static_assert(sizeof(size_type) >= sizeof(typename std::iterator_traits<EndType>::difference_type),
                      "Signed conversion is not safe if the iterator difference type is larger than our size_type");
        CETL_DEBUG_ASSERT(std::distance(first, end) >= 0,
                          "CDE_span_012: Negative distance between first and end iterators is undefined.");
    }

    /// Creates a span starting at the first element of a c-style array through to the end of that array.
    /// @tparam ArrayLen Deduced length of the c array to become the size of the span.
    /// @param arr  Reference to a C-style array.
    /// @see std::span::span()
    template <std::size_t ArrayLen>
    constexpr span(element_type (&arr)[ArrayLen]) noexcept
        : data_(arr)
        , size_(ArrayLen)
    {
    }

    /// Creates a span over an entire `std::array` starting at the array's first element and with a size set to
    /// `ArrayLen`.
    /// @tparam ArrayElementType Deduced array element type to support SFNAE enablement.
    /// @tparam ArrayLen         The length of the array. span::size() is set to this value.
    /// @tparam type    Enables this override only if the array's element type is the same as the span's or if the
    ///                 conversion is a simple [qualification
    ///                 conversion](https://en.cppreference.com/w/cpp/language/implicit_conversion#Qualification_conversions).
    /// @param arr      The array to create the view into.
    /// @see std::span::span()
    template <typename ArrayElementType,
              std::size_t ArrayLen,
              typename std::enable_if<std::is_convertible<ArrayElementType (*)[], T (*)[]>::value, bool>::type = true>
    constexpr span(std::array<ArrayElementType, ArrayLen>& arr) noexcept
        : data_(arr.data())
        , size_(ArrayLen)
    {
    }

    /// Creates a span over an entire `const std::array` starting at the array's first element and with a size set to
    /// `ArrayLen`.
    /// @tparam ArrayElementType Deduced array element type to support SFNAE enablement.
    /// @tparam ArrayLen         The length of the array. span::size() is set to this value.
    /// @tparam type    Enables this override only if the array's element type is the same as the span's or if the
    ///                 conversion is a simple [qualification
    ///                 conversion](https://en.cppreference.com/w/cpp/language/implicit_conversion#Qualification_conversions).
    /// @param arr      The array to create the view into.
    /// @see std::span::span()
    template <
        typename ArrayElementType,
        std::size_t ArrayLen,
        typename std::enable_if<std::is_convertible<const ArrayElementType (*)[], T (*)[]>::value, bool>::type = true>
    constexpr span(const std::array<ArrayElementType, ArrayLen>& arr) noexcept
        : data_(arr.data())
        , size_(ArrayLen)
    {
    }

    // TODO: C++20 range constructor.
    // template< class R >
    // explicit(extent != std::dynamic_extent)
    // constexpr span( R&& range );

    /// Copy constructor to create a span from another span. This overload allows conversion of any span to a dynamic
    /// span.
    /// @tparam DeducedElementType  The element type of the source span.
    /// @tparam DeducedExtent       The extent of the source span.
    /// @tparam type                Enables this override only if the element conversion is a simple [qualification
    ///                 conversion](https://en.cppreference.com/w/cpp/language/implicit_conversion#Qualification_conversions).
    /// @param source   The span to copy from. The resulting span has `size() == source.size()` and
    ///                 `data() == source.data()`
    /// @see std::span()
    template <class DeducedElementType,
              size_type DeducedExtent,
              typename std::enable_if<std::is_convertible<DeducedElementType (*)[], element_type (*)[]>::value,
                                      bool>::type = true>
    constexpr span(const span<DeducedElementType, DeducedExtent>& source) noexcept
        : data_(source.data())
        , size_(source.size())
    {
    }

    /// Default copy constructor
    /// @see std::span::span()
    constexpr span(const span&) noexcept = default;

    ///@}
    // +----------------------------------------------------------------------+
    ///@{ @name Operators
    // +----------------------------------------------------------------------+
    /// @copydoc span::operator=()
    ///
    constexpr span& operator=(const span& rhs) noexcept = default;

    /// @copydoc span::operator[]()
    ///
    constexpr reference operator[](size_type idx) const
    {
        CETL_DEBUG_ASSERT((idx < size_), "CDE_span_013: Indexing outside of the span is undefined.");
        CETL_DEBUG_ASSERT((data_ != nullptr), "CDE_span_014: Indexing with non data (nullptr) is undefined.");
        return data_[idx];
    }

    ///@}
    // +----------------------------------------------------------------------+
    ///@{ @name Iterators
    // +----------------------------------------------------------------------+
    /// @copydoc span::begin()
    ///
    constexpr iterator begin() const noexcept
    {
        return iterator(&data_[0]);
    }
    /// @copydoc span::end()
    ///
    constexpr iterator end() const noexcept
    {
        return iterator(&data_[size_]);
    }
    /// @copydoc span::rbegin()
    ///
    constexpr reverse_iterator rbegin() const noexcept
    {
        return reverse_iterator(end());
    }
    /// @copydoc span::rend()
    ///
    constexpr reverse_iterator rend() const noexcept
    {
        return reverse_iterator(begin());
    }

    // +----------------------------------------------------------------------+
    ///@{ @name Element Access
    // +----------------------------------------------------------------------+
    /// @copydoc span::front()
    ///
    constexpr reference front() const
    {
        CETL_DEBUG_ASSERT((size_ > 0), "CDE_span_015: Calling front on an empty span is undefined.");
        return *begin();
    }

    /// @copydoc span::back()
    ///
    constexpr reference back() const
    {
        CETL_DEBUG_ASSERT((size_ > 0), "CDE_span_016: Calling back on an empty span is undefined.");
        return data_[size_ - 1];
    }

    /// @copydoc span::data()
    ///
    constexpr pointer data() const noexcept
    {
        return data_;
    }

    ///@}
    // +----------------------------------------------------------------------+
    ///@{ @name Observers
    // +----------------------------------------------------------------------+

    ///
    /// Returns if the span has a zero size or not.
    /// @return true if the span size is 0.
#if (__cplusplus >= 201703L)
    [[nodiscard]]
#endif
    constexpr bool
    empty() const noexcept
    {
        return (size_ == 0);
    }

    ///
    /// Returns the size of the span.
    /// @return The number of elements in the span.
    /// @see std::span::size
    constexpr size_type size() const noexcept
    {
        return size_;
    }

    /// @copydoc span::size_bytes()
    ///
    constexpr size_type size_bytes() const noexcept
    {
        return sizeof(element_type) * size_;
    }
    ///@}
    // +----------------------------------------------------------------------+
    ///@{ @name Subviews
    // +----------------------------------------------------------------------+
    /// Create a new span from the start of the current span for `Count` elements.
    /// @tparam Count   Number of elements for the sub-span. The behavior of this method and the returned span is
    ///                 undefined if `Count` is greater than this span's size
    /// @return A new span with the static extent `Count`.
    /// @see std::span::first
    template <size_type Count>
    constexpr span<element_type, Count> first() const
    {
        CETL_DEBUG_ASSERT(Count <= size_, "CDE_span_017: Subviews beyond the size of the span's view are undefined.");
        return span<element_type, Count>(data_, Count);
    }

    /// @copydoc span::first(span::size_type count) const
    ///
    constexpr span<element_type, dynamic_extent> first(size_type count) const
    {
        CETL_DEBUG_ASSERT(count <= size_,
                          "CDE_span_018: Dynamic subviews beyond the size of the span's view are undefined.");
        return span<element_type, dynamic_extent>(data_, count);
    }

    /// Create a new span `Count` elements from the last item of the current span to its end.
    /// @tparam Count   Number of elements for the sub-span. The behavior of this method is undefined where
    ///                 `Count` is greater than the span's current size.
    /// @return A new span with the static extent `Count`.
    /// @see std::span::last
    template <size_type Count>
    constexpr span<element_type, Count> last() const
    {
        CETL_DEBUG_ASSERT(Count <= size_, "CDE_span_019: Subviews beyond the size of the span's view are undefined.");
        return span<element_type, Count>(&data_[size_ - Count], Count);
    }

    /// @copydoc span::last(span::size_type count) const
    ///
    constexpr span<element_type, dynamic_extent> last(size_type count) const
    {
        CETL_DEBUG_ASSERT(count <= size_,
                          "CDE_span_020: Dynamic subviews beyond the size of the span's view are undefined.");
        return span<element_type, dynamic_extent>(&data_[size_ - count], count);
    }

    /// Create a new span Offset elements from the start of the current span and for `Count` elements.
    /// The behavior of this method is undefined where `Count` is greater-than the size of this span or if
    /// `Count` is not dynamic_extent but `Count` is greater-than `size() - Offset`.
    /// @tparam Offset  Number of elements from the start of the current span for the subspan.
    /// @tparam Count   The number of elements from the Offset to include in the subspan. If this value is
    ///                 `dynamic_extent` then the Count is `size() - Offset`.
    /// @return A new span where the new `Extent` is `Count` if that parameter was not `dynamic_extent` otherwise
    ///         the new span's `Extent` is `dynamic_extent`.
    /// @see std::span::subspan()
    template <size_type Offset, size_type Count = dynamic_extent>
    constexpr span<element_type, Count> subspan() const
    {
        CETL_DEBUG_ASSERT(Offset <= size_, "CDE_span_023: subspan Offsets > extent are ill-formed.");
        CETL_DEBUG_ASSERT((Count == dynamic_extent) || (Count <= size_ - Offset),
                          "CDE_span_024: subspan Count argument is ill-formed");
        return span<element_type, Count>{&data_[Offset], Count == dynamic_extent ? size() - Offset : Count};
    }

    /// @copydoc span::subspan(span::size_type Offset, span::size_type Count) const
    ///
    constexpr span<element_type, dynamic_extent> subspan(size_type Offset, size_type Count = dynamic_extent) const
    {
        CETL_DEBUG_ASSERT(Offset <= size_, "CDE_span_021: subspan Offsets > size() are ill-formed.");
        if (Count == dynamic_extent)
        {
            return {&data_[Offset], size_ - Offset};
        }
        else
        {
            CETL_DEBUG_ASSERT(Count <= (size_ - Offset), "CDE_span_022: subspan Count argument is ill-formed");
            return {&data_[Offset], Count};
        }
    }

    ///@}
private:
    pointer   data_;
    size_type size_;
};

// required till C++ 17. Redundant but allowed after that.
template <typename T>
const std::size_t span<T, dynamic_extent>::extent;

}  // namespace pf20
}  // namespace cetl

#endif  // CETL_PF20_SPAN_H_INCLUDED
