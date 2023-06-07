/// @copyright Copyright Amazon.com Inc. and its affiliates. All Rights Reserved.
/// @file
/// Generic List implementation

#ifndef LIBCYPHAL_TYPES_LIST_HPP_INCLUDED
#define LIBCYPHAL_TYPES_LIST_HPP_INCLUDED

#include <array>
#include <cstddef>
#include <initializer_list>

namespace libcyphal
{

/// @brief An array with a count of the number of used elements
/// @note Objects which live in this List will be constructed once by the default Constructor and then a second time
/// when emplaced.
/// @warning Objects will also be destructed at least once by the List destruction but possibly twice when dismissed.
template <typename Type, std::size_t Size>
class List
{
public:
    // type must be either fundamental or if a class, default constructible.
    static_assert(std::is_fundamental<Type>::value or
                      (std::is_class<Type>::value and std::is_default_constructible<Type>::value),
                  "Type must either be a fundamental type or default constructable");

    /// @brief Default constructor
    List()
        : array_{}
        , used_{0u}
    {
    }

    /// @brief Allows the list to be initialized by an initializer list
    List(std::initializer_list<Type> list)
        : array_{}
        , used_{0}
    {
        // cycle over the initializer list
        for (auto it = list.begin(); it != list.end() && used_ < Size; it++)
        {
            array_[used_] = *it;
            used_++;
        }
    }

    /// @brief Allows in-place construction of an element in the List.
    template <typename... Args>  // TODO: Templated - Replace with something else?
    bool emplace_back(Args&&... args)
    {
        if (used_ < Size)
        {
            // placement new in this spot
            // use the ::new to restrict to top level namespace new, not class versions
            void* location = &array_[used_];
            ::new (location) Type{args...};
            used_++;
            return true;
        }
        return false;
    }

    /// @brief Destructs an object from the end position.
    /// @return Returns true if there was an element to destruct, false otherwise
    /// @warning This can only be called on objects, not primitive types.
    bool dismiss_back()
    {
        if (used_ > 0)
        {
            std::size_t index = used_ - 1;
            array_[index].~Type();  // call destructor
            used_--;
            return true;
        }
        return false;
    }

    /// @brief Get a "mutable" reference to an element.
    /// @warning When size is zero, will return the zeroth element.
    Type& operator[](std::size_t index)
    {
        return array_[(used_ > 0u) ? index % used_ : 0u];
    }

    /// @brief Get a read only reference to an element.
    /// @warning When size is zero, will return the zeroth element.
    const Type& operator[](std::size_t index) const
    {
        return array_[(used_ > 0u) ? index % used_ : 0u];
    }

    /// @brief Returns a mutable reference last element.
    /// @warning When size is zero, will return the zeroth element.
    Type& last()
    {
        return array_[(used_ == 0u) ? 0u : (used_ - 1u)];
    }

    /// @brief Returns a const reference to the last element.
    /// @warning When size is zero, will return the zeroth element.
    const Type& last() const
    {
        return array_[(used_ == 0u) ? 0u : (used_ - 1u)];
    }

    /// @brief Returns if the list is empty.
    bool is_empty() const
    {
        return (size() == 0u);
    }

    /// @brief Returns if the list is full.
    bool is_full() const
    {
        return (size() == capacity());
    }

    /// @brief Returns the active count of the list which is guaranteed to be less than or equal to the capacity.
    std::size_t size() const
    {
        return used_;
    }

    /// @brief Returns the maximum size of the list.
    std::size_t capacity() const
    {
        return Size;
    }

    /// @{ Reuse the types from std::array
    using ConstIterator = typename std::array<Type, Size>::const_iterator;
    using Iterator      = typename std::array<Type, Size>::iterator;
    /// @}

    /// @brief Reuse the array begin
    ConstIterator begin() const
    {
        return array_.begin();
    }

    /// @brief Reuse the array begin
    Iterator begin()
    {
        return array_.begin();
    }

    /// @brief Implement an end which is just past the last used element.
    ConstIterator end() const
    {
        return Iterator{&array_[used_]};
    }

    /// @brief Implement an end which is just past the last used element.
    Iterator end()
    {
        return Iterator{&array_[used_]};
    }

protected:
    std::array<Type, Size> array_;  //!< The internal memory
    std::size_t            used_;   //!< The number of used elements in the array. The number grows from zero up.
};

}  // namespace libcyphal

#endif  // LIBCYPHAL_TYPES_LIST_HPP_INCLUDED
