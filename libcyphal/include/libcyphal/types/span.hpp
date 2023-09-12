/// @copyright Copyright Amazon.com Inc. and its affiliates. All Rights Reserved.
/// @file
/// Defines a generic span type

#ifndef LIBCYPHAL_TYPES_SPAN_HPP_INCLUDED
#define LIBCYPHAL_TYPES_SPAN_HPP_INCLUDED

#include <array>
#include <cstddef>
#include <cstdint>
#include <cstring>
#include <type_traits>
#include <utility>

namespace libcyphal
{

template <typename T>
class Span
{
public:
    static_assert(not std::is_pointer<T>::value, "Can not be a pointer type");

    using ElementType        = T;         //!< The element type over which the span operates
    using PointerType        = T*;        //!< The element pointer type
    using ConstPointerType   = const T*;  //!< A constant pointer to the element type
    using ReferenceType      = T&;        //!< A reference type to the element
    using ConstReferenceType = const T&;  //!< A constant reference type to the element
    using CountType          = size_t;    //!< Used to track the count of elements
    using SizeType           = size_t;    //!< Used to track the size in bytes

    static_assert(sizeof(ElementType) != 0u, "Must have non zero element size");

    /// Default construction chains to parameter construction
    constexpr Span()
        : Span(nullptr, 0u)
    {
    }
    /// Parameter construction must be explicitly called.
    constexpr explicit Span(PointerType ptr, CountType c)
        : data_{ptr}
        , count_{c}
    {
    }
    /// Reversed Parameter Constructor must be also explicitly called (chains to other constructor)
    constexpr explicit Span(CountType c, PointerType ptr)
        : Span(ptr, c)
    {
    }
    /// Secondary Templated Constructor passes to parameter constructor
    template <CountType Count>
    constexpr explicit Span(ElementType (&array)[Count])
        : Span(&array[0], Count)
    {
    }
    /// Copy Constructor (chain to parameter)
    constexpr Span(const Span& other)
        : Span(other.data_, other.count_)
    {
    }
    /// Move Constructor empties the moved Span
    constexpr Span(Span&& other)
        : Span(other.data_, other.count_)
    {
        other.empty();
    }
    /// Copy Assign
    Span& operator=(const Span& other)
    {
        data_  = other.data_;
        count_ = other.count_;
        return (*this);
    }
    /// Move Assign empties the moved Span
    Span& operator=(Span&& other)
    {
        data_  = other.data_;
        count_ = other.count_;
        other.empty();
        return (*this);
    }
    // Destructor empties the Span ( can't have this if we want to use it in constexpr functions! )
    // ~Span() {
    //     empty();
    // }

    /// Conditional usage of the class
    explicit operator bool() const
    {
        return not is_empty();
    }
    /// Equality operator
    bool operator==(const Span& other) const
    {
        return other.data() == data() and other.size() == size();
    }
    /// Inequality operator
    bool operator!=(const Span& other) const
    {
        return not operator==(other);
    }
    /// Destructively erases the pointer and size.
    void empty()
    {
        reset(nullptr, 0u);
    }
    /// Checks to see if the pointer is null or the size is zero
    bool is_empty() const
    {
        return data() == nullptr or size() == 0u;
    }
    /// Returns a reference to an element.
    constexpr ReferenceType operator[](SizeType index)
    {
        return data()[(index < count() or index == 0u) ? index : index % count()];
    }
    /// Returns a reference to an element.
    constexpr ConstReferenceType operator[](SizeType index) const
    {
        return data()[(index < count() or index == 0u) ? index : index % count()];
    }
    /// Returns the data pointer
    constexpr PointerType data()
    {
        return data_;
    }
    /// Returns a constant pointer
    constexpr ConstPointerType data() const
    {
        return data_;
    }
    /// Returns the size of the Span in bytes
    constexpr SizeType size() const
    {
        return sizeof(ElementType) * count();
    }
    /// Returns the count of the elements
    constexpr CountType count() const
    {
        return count_;
    }
    /// Destructively resets the values of the pointer and count
    void reset(PointerType p, CountType c)
    {
        data_  = p;
        count_ = c;
    }
    /// Destructively resets the values of the pointer and count
    void reset(CountType c, PointerType p)
    {
        data_  = p;
        count_ = c;
    }

    /// Secondary Templatized Destructively resets values of pointer and count
    template <CountType Count>
    void reset(ElementType (&array)[Count])
    {
        reset(&array[0], Count);
    }
    /// Adjusts the count to a value more than zero and less than or equal to the current size. To reduce to zero use
    /// @ref empty
    bool recount(CountType c)
    {
        if (0u < c and c < count())
        {
            count_ = c;
            return true;
        }
        return false;
    }
    /// Adjusts the count to a value more than zero and less than or equal to the current size. To reduce to zero use
    /// @ref empty
    bool resize(SizeType s)
    {
        if (0u < s and s < size())
        {
            // depending on integer truncation in division
            CountType c = s / sizeof(T);
            if ((c * sizeof(T)) != s)
            {
                // produced a rounding issue
                return false;
            }
            count_ = c;
            return true;
        }
        return false;
    }

    /// Returns a subspan of the original span.
    /// @returns A Span within this span. If the offset and size extend over this span, an empty Span is returned.
    auto subspan(size_t offset, size_t size)
    {
        if ((offset + size) <= count_)
        {
            return Span{&data_[offset], size};
        }
        return Span{nullptr, 0u};
    }

    /// @{ Reuse the types  from std::array
    using const_iterator = typename std::array<ElementType, 0>::const_iterator;
    using iterator       = typename std::array<ElementType, 0>::iterator;
    /// @}

    /// Reuse the array begin
    const_iterator begin() const
    {
        return data_;
    }
    /// Reuse the array begin
    iterator begin()
    {
        return data_;
    }
    /// Implement an end which is just past the last used element.
    const_iterator end() const
    {
        return &data_[count_];
    }
    /// Implement an end which is just past the last used element.
    iterator end()
    {
        return &data_[count_];
    }

protected:
    PointerType data_;
    CountType   count_;
};

/// Copies from source to destination only if the source will fit within the destination entirely.
/// @retval False if the destination was too small to hold all of source.
template <typename Type>
bool FullCopy(Span<Type> dst, Span<const Type> src)
{
    if (src.count() <= dst.count())
    {
        std::memcpy(dst.data(), src.data(), src.size());
        return true;
    }
    return false;
}

/// Copies from source to destination only if the source will fit within the destination entirely.
/// @retval False if the destination was too small to hold all of source.
template <typename DestinationType, typename SourceType>
bool FullCopy(Span<DestinationType> dst, Span<const SourceType> src)
{
    if (src.size() <= dst.size())
    {
        std::memcpy(dst.data(), src.data(), src.size());
        return true;
    }
    return false;
}

/// Copies from source to destination. If destination is smaller, some data will not be copied.
template <typename Type>
void PartialCopy(Span<Type> dst, Span<const Type> src)
{
    size_t len = std::min(src.size(), dst.size());
    std::memcpy(dst.data(), src.data(), len);
}

}  // namespace libcyphal

#endif  // LIBCYPHAL_TYPES_SPAN_HPP_INCLUDED
