/// @file
/// Temporary (yeah, right!) header for types that need to be promoted to CETL.
///
/// @copyright
/// Copyright (C) OpenCyphal Development Team  <opencyphal.org>
/// Copyright Amazon.com Inc. or its affiliates.
/// SPDX-License-Identifier: MIT
///

#ifndef LIBCYPHAL_JANKY_HPP_INCLUDED
#define LIBCYPHAL_JANKY_HPP_INCLUDED

#include <array>
#include <new>

#include "libcyphal/libcyphal.hpp"
#include "cetl/cetl.hpp"
#include "cetl/variable_length_array.hpp"

namespace libcyphal
{

///
/// @namespace janky
/// Things that are half-baked placeholders for future CETL types.
///
namespace janky
{

// +---------------------------------------------------------------------------+
// | EXPECTED
// +---------------------------------------------------------------------------+
template <typename Err>
struct unexpected final
{
    Err value;
    explicit unexpected(Err e)
        : value(e)
    {
    }
};

/// This is a dumbed down version of C++23 std::expected hoisted from Nunavut support.
/// It never throws, but uses CETL_DEBUG_ASSERT to signal exceptional cases.
/// All versions of Ret are expected to be non-throwing.
template <typename Ret>
class expected final
{
    // We can use a maximum of all types.
    using storage_t = typename std::aligned_storage<(std::max(sizeof(Ret), sizeof(ResultCode))),
                                                    (std::max(alignof(Ret), alignof(ResultCode)))>::type;
    storage_t storage;
    bool      is_expected_;

private:
    Ret* ret_ptr()
    {
        return reinterpret_cast<Ret*>(&storage);
    }
    const Ret* ret_ptr() const
    {
        return reinterpret_cast<const Ret*>(&storage);
    }
    ResultCode* error_ptr()
    {
        return reinterpret_cast<ResultCode*>(&storage);
    }
    const ResultCode* error_ptr() const
    {
        return reinterpret_cast<const ResultCode*>(&storage);
    }

public:
    expected()
        : is_expected_(true)
    {
        new (ret_ptr()) Ret();
    }
    expected(Ret r)
        : is_expected_(true)
    {
        new (ret_ptr()) Ret(std::move(r));
    }
    expected& operator=(Ret other)
    {
        this->~expected();
        return *new (this) expected(std::move(other));
    }
    expected(unexpected<ResultCode> err)
        : is_expected_(false)
    {
        new (error_ptr()) ResultCode(std::move(err.value));
    }
    expected(const expected& other)
        : is_expected_(other.is_expected_)
    {
        if (is_expected_)
        {
            new (ret_ptr()) Ret(*other.ret_ptr());
        }
        else
        {
            new (error_ptr()) ResultCode(*other.error_ptr());
        }
    }
    expected& operator=(const expected& other)
    {
        this->~expected();
        return *new (this) expected(other);
    }
    expected(expected&& other)
        : is_expected_(other.is_expected_)
    {
        if (is_expected_)
        {
            new (ret_ptr()) Ret(std::move(*other.ret_ptr()));
        }
        else
        {
            new (error_ptr()) ResultCode(std::move(*other.error_ptr()));
        }
    }
    expected& operator=(expected&& other)
    {
        this->~expected();
        return *new (this) expected(std::move(other));
    }
    ~expected()
    {
        if (is_expected_)
        {
            ret_ptr()->~Ret();
        }
        else
        {
            error_ptr()->~ResultCode();
        }
    }

    Ret& value()
    {
        CETL_DEBUG_ASSERT(is_expected_, "expected::value() called on unexpected value");
        return *ret_ptr();
    }
    const Ret& value() const
    {
        CETL_DEBUG_ASSERT(is_expected_, "expected::value() called on unexpected value");
        return *ret_ptr();
    }
    Ret& operator*()
    {
        return value();
    }
    const Ret& operator*() const
    {
        return value();
    }
    Ret* operator->()
    {
        CETL_DEBUG_ASSERT(is_expected_, "expected::operator->() called on unexpected value");
        return ret_ptr();
    }
    const Ret* operator->() const
    {
        CETL_DEBUG_ASSERT(is_expected_, "expected::operator->() called on unexpected value");
        return ret_ptr();
    }
    ResultCode& error()
    {
        CETL_DEBUG_ASSERT(not is_expected_, "expected::error() called on expected value");
        return *error_ptr();
    }
    const ResultCode& error() const
    {
        CETL_DEBUG_ASSERT(not is_expected_, "expected::error() called on expected value");
        return *error_ptr();
    }

    bool has_value() const
    {
        return is_expected_;
    }
    operator bool() const
    {
        return has_value();
    }
};

template <>
class expected<void> final
{
    using underlying_type = typename std::underlying_type<ResultCode>::type;
    underlying_type e;

public:
    expected()
        : e(0)
    {
    }
    expected(unexpected<ResultCode> err)
        : e(static_cast<underlying_type>(err.value))
    {
    }
    ResultCode error() const
    {
        CETL_DEBUG_ASSERT(not has_value(), "expected::error() called on expected value");
        return static_cast<ResultCode>(e);
    }

    bool has_value() const
    {
        return e == 0;
    }
    operator bool() const
    {
        return has_value();
    }
};

// +---------------------------------------------------------------------------+
// | DETECTION IDIOM
// +---------------------------------------------------------------------------+
template <typename...>
using _void_t = void;

template <class AlwaysVoid, template <class...> class Op, class... Args>
struct detector
{
    using value_t = std::false_type;
};

template <template <class...> class Op, class... Args>
struct detector<_void_t<Op<Args...>>, Op, Args...>
{
    using value_t = std::true_type;
};

template <template <class...> class Op, class... Args>
using is_detected = typename detector<void, Op, Args...>::value_t;

// +---------------------------------------------------------------------------+
// | EXPLICIT TYPE SYSTEM (EXPERIMENTAL)
// +---------------------------------------------------------------------------+

/// 128-bit type ID for polymorphic types. On most platforms, this will be represented as a 16-byte array but some
/// platforms may end up optimizing this to use 128-bit SIMD instructions. As such, USE THE TYPE ALIAS!
using PolymorphicTypeId = std::array<std::uint8_t, 16>;

/// Base class for polymorphic types that support a runtime type system. This is used instead of RTTI as it is fully
/// visible in the source code and applies only to a specific set of types.
class IPolymorphicType
{
public:
    /// Query the type to see if it supports a given type. Mutable override.
    /// @param id   The UID of the type to query. This ID is unique to a single libcyphal polymorphic type.
    /// @param out  A pointer that will be set to an interface supporting IPolymorphicType but which can be safely cast
    ///             to a known subtype. This is set to nullptr for all failures.
    ///             @note Libcyphal does not use object composition. As such, the pointer returned will always be to a
    ///             subtype of the object being queried.
    /// @return ResultCode::Success if the type is supported and the out pointer was set. ResultCode::LookupError
    /// otherwise.
    virtual Status queryType(const PolymorphicTypeId& id, void*& out) noexcept = 0;

    /// Query the type to see if it supports a given type. Const override.
    /// @param id   The UID of the type to query. This ID is unique to a single libcyphal polymorphic type.
    /// @param out  A pointer that will be set to an interface supporting IPolymorphicType but which can be safely cast
    ///             to a known subtype. This is set to nullptr for all failures.
    ///             @note Libcyphal does not use object composition. As such, the pointer returned will always be to a
    ///             subtype of the object being queried.
    /// @return ResultCode::Success if the type is supported and the out pointer was set. ResultCode::LookupError
    /// otherwise.
    virtual Status queryType(const PolymorphicTypeId& id, const void*& out) const noexcept = 0;

    /// Test if two instances of the type are the same instance. Polymorphic types are free to define instance equality
    /// as they see fit.
    /// @param other The other instance to compare to.
    /// @return true if this instance is equal to other else false.
    virtual bool isEqual(const IPolymorphicType& other) const noexcept = 0;

protected:
    virtual ~IPolymorphicType() = default;
};

/// Modelled on std::allocator_traits, this class uses Template Meta-Programming to perform generic operations on
/// types that may or may not be IPolymorphicType types.
struct polymorphic_type_traits
{
    ~polymorphic_type_traits()                                         = delete;
    polymorphic_type_traits(const polymorphic_type_traits&)            = delete;
    polymorphic_type_traits& operator=(const polymorphic_type_traits&) = delete;

    /// Reserved to identify "not a polymorphic type" types.
    static constexpr PolymorphicTypeId NoType =
        {0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00};

    /// Used to detect if a given type is a PolymorphicTypeId
    /// @tparam U The suspected type.
    template <typename U>
    using TypeIdSize = decltype(std::declval<U>().TypeId.size());

    /// Gets a type ID for a given type or NoType of the type did not define TypeId.
    /// @tparam ConcreteType The suspected concrete types.
    /// @return Either the type ID of the concrete type or NoType.
    template <typename ConcreteType>
    static constexpr
        typename std::enable_if<is_detected<TypeIdSize, ConcreteType>::value, const PolymorphicTypeId&>::type
        id()
    {
        return ConcreteType::TypeId;
    }

    /// Gets a type ID for a given type or NoType of the type did not define TypeId.
    /// @tparam ConcreteType The suspected concrete types.
    /// @return Either the type ID of the concrete type or NoType.
    template <typename ConcreteType>
    static constexpr
        typename std::enable_if<!is_detected<TypeIdSize, ConcreteType>::value, const PolymorphicTypeId&>::type
        id()
    {
        return NoType;
    }

    /// Cast a give object to a concrete type if it is a polymorphic type and the queryType successfully returns a
    /// pointer to cast.
    /// @tparam ToType      The type to cast to.
    /// @tparam FromType    The type to cast from.
    /// @param obj          The object to cast to ToType by way of IPolymorphicType::queryType.
    /// @return A pointer to the object cast to ToType or nullptr if the object is not a polymorphic type or is a
    /// polymorphic type but is not
    ///         ToType.
    template <typename ToType, typename FromType>
    static constexpr typename std::enable_if<std::is_base_of<IPolymorphicType, FromType>::value,
                                             typename std::add_pointer<ToType>::type>::type
    safe_downcast(FromType& obj)
    {
        typename std::conditional<std::is_const<ToType>::value, const void*, void*>::type result{nullptr};
        if (obj.queryType(id<ToType>(), result))
        {
            return reinterpret_cast<typename std::add_pointer<ToType>::type>(result);
        }
        else
        {
            return nullptr;
        }
    }

    /// Cast a give object to a concrete type if it is a polymorphic type and the queryType successfully returns a
    /// pointer to cast.
    /// @tparam ToType      The type to cast to.
    /// @tparam FromType    The type to cast from.
    /// @param obj          The object to cast to ToType by way of IPolymorphicType::queryType.
    /// @return A pointer to the object cast to ToType or nullptr if the object is not a polymorphic type or is a
    /// polymorphic type but is not
    ///         ToType.
    template <typename ToType, typename FromType>
    static constexpr typename std::enable_if<!std::is_base_of<IPolymorphicType, FromType>::value,
                                             typename std::add_pointer<ToType>::type>::type
    safe_downcast(FromType& obj)
    {
        (void) obj;
        return nullptr;
    }

    /// Compare two IPolymorphicType instances for equality.
    /// @tparam LeftType     The type to cast to IPolymorphicType which will be used to call IPolymorphicType::isEqual
    ///                      on.
    /// @tparam RightType    The type to cast to IPolymorphicType which will be used as the argument to LeftType's
    ///                      IPolymorphicType::isEqual.
    /// @param  left         The object to cast to IPolymorphicType and call IPolymorphicType::isEqual.
    /// @param  right        The object to cast to IPolymorphicType and provide to left.isEqual().
    /// @return the result of left.isEqual(right) if both left and right are IPolymorphicType instances otherwise false.
    template <typename LeftType, typename RightType>
    static constexpr typename std::enable_if<
        std::is_pointer<LeftType>::value && std::is_pointer<RightType>::value &&
            std::is_base_of<IPolymorphicType, typename std::remove_pointer<LeftType>::type>::value &&
            std::is_base_of<IPolymorphicType, typename std::remove_pointer<RightType>::type>::value,
        bool>::type
    isEqual(const LeftType left, const RightType right) noexcept
    {
        if (nullptr == left || nullptr == right)
        {
            return false;
        }
        else
        {
            return isEqual<
                typename std::add_lvalue_reference<typename std::remove_pointer<LeftType>::type>::type,
                typename std::add_lvalue_reference<typename std::remove_pointer<RightType>::type>::type>(*left, *right);
        }
    }

    /// Compare two IPolymorphicType instances for equality.
    /// @tparam LeftType     The type to cast to IPolymorphicType which will be used to call IPolymorphicType::isEqual
    ///                      on.
    /// @tparam RightType    The type to cast to IPolymorphicType which will be used as the argument to LeftType's
    ///                      IPolymorphicType::isEqual.
    /// @param  left         The object to cast to IPolymorphicType and call IPolymorphicType::isEqual.
    /// @param  right        The object to cast to IPolymorphicType and provide to left.isEqual().
    /// @return the result of left.isEqual(right) if both left and right are IPolymorphicType instances otherwise false.
    template <typename LeftType, typename RightType>
    static constexpr typename std::enable_if<
        std::is_pointer<LeftType>::value && std::is_pointer<RightType>::value &&
            (!std::is_base_of<IPolymorphicType, typename std::remove_pointer<LeftType>::type>::value ||
             !std::is_base_of<IPolymorphicType, typename std::remove_pointer<RightType>::type>::value),
        bool>::type
    isEqual(const LeftType left, const RightType right) noexcept
    {
        (void) left;
        (void) right;
        return false;
    }

    /// Compare two IPolymorphicType instances for equality.
    /// @tparam LeftType     The type to cast to IPolymorphicType which will be used to call IPolymorphicType::isEqual
    ///                      on.
    /// @tparam RightType    The type to cast to IPolymorphicType which will be used as the argument to LeftType's
    ///                      IPolymorphicType::isEqual.
    /// @param  left         The object to cast to IPolymorphicType and call IPolymorphicType::isEqual.
    /// @param  right        The object to cast to IPolymorphicType and provide to left.isEqual().
    /// @return the result of left.isEqual(right) if both left and right are IPolymorphicType instances otherwise false.
    template <typename LeftType, typename RightType>
    static constexpr typename std::enable_if<!std::is_pointer<LeftType>::value && !std::is_pointer<RightType>::value &&
                                                 std::is_base_of<IPolymorphicType, LeftType>::value &&
                                                 std::is_base_of<IPolymorphicType, RightType>::value,
                                             bool>::type
    isEqual(const LeftType& left, const RightType& right) noexcept
    {
        return static_cast<const IPolymorphicType&>(left).isEqual(static_cast<const IPolymorphicType&>(right));
    }

    /// Compare two IPolymorphicType instances for equality.
    /// @tparam LeftType     The type to cast to IPolymorphicType which will be used to call IPolymorphicType::isEqual
    ///                      on.
    /// @tparam RightType    The type to cast to IPolymorphicType which will be used as the argument to LeftType's
    ///                      IPolymorphicType::isEqual.
    /// @param  left         The object to cast to IPolymorphicType and call IPolymorphicType::isEqual.
    /// @param  right        The object to cast to IPolymorphicType and provide to left.isEqual().
    /// @return the result of left.isEqual(right) if both left and right are IPolymorphicType instances otherwise false.
    template <typename LeftType, typename RightType>
    static constexpr typename std::enable_if<!std::is_pointer<LeftType>::value && !std::is_pointer<RightType>::value &&
                                                 (!std::is_base_of<IPolymorphicType, LeftType>::value ||
                                                  !std::is_base_of<IPolymorphicType, RightType>::value),
                                             bool>::type
    isEqual(const LeftType& left, const RightType& right) noexcept
    {
        (void) left;
        (void) right;
        return false;
    }
};

// +---------------------------------------------------------------------------+
// | LAUNDER
// +---------------------------------------------------------------------------+
/// Per the cppreference documentation, launder should be used when:
/// > Obtaining a pointer to an object created by placement new from a pointer to an object providing storage for
/// > that object.
template <typename ToType, typename FromType>
constexpr ToType launder_cast(FromType p) noexcept
{
#if __cplusplus >= CETL_CPP_STANDARD_17
    return std::launder(reinterpret_cast<ToType>(p));
#else
    return reinterpret_cast<ToType>(p);
#endif
}

/// You should be ashamed of yourself!
template <typename ToType, typename FromType>
constexpr ToType nuclear_cast(FromType p) noexcept
{
    using ToConstType = std::add_pointer_t<std::add_const_t<std::remove_pointer_t<ToType>>>;
    return const_cast<ToType>(launder_cast<ToConstType>(p));
}

// +---------------------------------------------------------------------------+
// | OPTIONAL
// +---------------------------------------------------------------------------+
struct nullopt_t
{
    struct _none_type
    {
        explicit _none_type() = default;
    };
    explicit constexpr nullopt_t(_none_type) {}
};

constexpr nullopt_t nullopt{nullopt_t::_none_type{}};

struct make_type_t
{
    struct _none_type
    {
        explicit _none_type() = default;
    };
    explicit constexpr make_type_t(_none_type) {}
};

constexpr make_type_t make_type{make_type_t::_none_type{}};

template <typename T>
class optional
{
public:
    optional()
        : has_value_(false)
    {
    }

    optional(nullopt_t)
        : has_value_(false)
    {
    }

    optional(const T& value)
        : has_value_(true)
    {
        new (&storage_) T(value);
    }

    optional(T&& value)
        : has_value_(true)
    {
        new (&storage_) T(std::move(value));
    }

    optional(const optional& rhs)
        : has_value_(rhs.has_value_)
    {
        if (has_value_)
        {
            new (&storage_) T(*rhs);
        }
    }

    template <typename... Args>
    optional(make_type_t tag, Args&&... args)
    {
        (void) tag;
        new (&storage_) T(std::forward<Args>(args)...);
    }

    optional(optional&& rhs)
        : has_value_(rhs.has_value_)
    {
        if (has_value_)
        {
            new (&storage_) T(std::move(*rhs));
        }
    }

    ~optional()
    {
        if (has_value_)
        {
            launder_cast<T*>(&storage_)->~T();
        }
    }

    optional& operator=(const optional& other)
    {
        if (this == &other)
        {
            return *this;
        }

        if (has_value_)
        {
            if (other.has_value)
            {
                launder_cast<T*>(&storage_)->operator=(*other);
            }
            else
            {
                launder_cast<T*>(&storage_)->~T();
                has_value_ = false;
            }
        }
        else
        {
            if (other.has_value_)
            {
                new (&storage_) T(*other);
                has_value_ = true;
            }
        }
        return *this;
    }

    optional& operator=(optional&& other)
    {
        if (this == &other)
        {
            return *this;
        }
        if (has_value_)
        {
            if (other.has_value_)
            {
                launder_cast<T*>(&storage_)->operator=(std::move(*other));
            }
            else
            {
                reinterpret_cast<T*>(&storage_)->~T();
                has_value_ = false;
            }
        }
        else
        {
            if (other.has_value_)
            {
                new (&storage_) T(std::move(*other));
                has_value_ = true;
            }
        }
        return *this;
    }

    operator bool() const
    {
        return has_value_;
    }

    bool has_value() const
    {
        return has_value_;
    }

    T* operator->()
    {
        return launder_cast<T*>(&storage_);
    }

    const T* operator->() const
    {
        return launder_cast<const T*>(&storage_);
    }

    const T& operator*() const& noexcept
    {
        return *launder_cast<const T*>(&storage_);
    }

    T& operator*() & noexcept
    {
        return *launder_cast<T*>(&storage_);
    }

    const T&& operator*() const&& noexcept
    {
        return std::move(*launder_cast<const T*>(&storage_));
    }

    T&& operator*() && noexcept
    {
        return std::move(*launder_cast<T*>(&storage_));
    }

    template <class U>
    constexpr T value_or(U&& default_value) const&
    {
        return bool(*this) ? **this : static_cast<T>(std::forward<U>(default_value));
    }

    template <class U>
    constexpr T value_or(U&& default_value) &&
    {
        return bool(*this) ? std::move(**this) : static_cast<T>(std::forward<U>(default_value));
    }

private:
    typename std::aligned_storage<sizeof(T), alignof(T)>::type storage_;
    bool                                                       has_value_;
};

template <class T, class... Args>
constexpr janky::optional<T> make_optional(Args&&... args)
{
    return janky::optional<T>(make_type, std::forward<Args>(args)...);
}

}  // namespace janky
}  // namespace libcyphal

// +---------------------------------------------------------------------------+
// | MEMORY.hpp FIXME!!!!!!!
// +---------------------------------------------------------------------------+
#include "cetl/pf17/memory_resource.hpp"

namespace cetl
{
namespace pmr
{
using memory_resource = cetl::pf17::pmr::memory_resource;

template <typename T>
using polymorphic_allocator = cetl::pf17::pmr::polymorphic_allocator<T>;
}  // namespace pmr
}  // namespace cetl

#include "cetl/pmr/memory.hpp"

// +---------------------------------------------------------------------------+
// | array_memory_resource.hpp FIXME!!!!!!!
// +---------------------------------------------------------------------------+

#include "cetl/pmr/array_memory_resource.hpp"

namespace cetl
{
namespace pmr
{
template <typename UpstreamMemoryResourceType>
class UnsynchronizedArrayMemoryResource : public cetl::pf17::pmr::memory_resource
{
private:
    /// Saturating add of two max size values clamped to the maximum value for the pointer difference type
    /// for the current architecture.
    static constexpr std::size_t calculate_max_size_bytes(std::size_t max_size_left, std::size_t max_size_right)
    {
        static_assert(std::numeric_limits<std::ptrdiff_t>::max() >= 0,
                      "We don't know what it means to have a negative maximum pointer diff? Serious, what gives?");

        constexpr const std::size_t max_diff_as_size =
            static_cast<std::size_t>(std::numeric_limits<std::ptrdiff_t>::max());
        const std::size_t left_clamped  = std::min(max_size_left, max_diff_as_size);
        const std::size_t right_clamped = std::min(max_size_right, max_diff_as_size);
        if (right_clamped > (max_diff_as_size - left_clamped))
        {
            return max_diff_as_size;
        }
        else
        {
            return left_clamped + right_clamped;
        }
    }

public:
    /// Designated constructor that initializes the object with a fixed buffer and an optional upstream memory resource.
    /// @param buffer                   The buffer that is used to satisfy allocation requests.
    /// @param buffer_size_bytes        The size, in bytes, of the buffer.
    /// @param upstream                 An optional upstream memory resource to use if the buffer is already in use.
    /// @param upstream_max_size_bytes  The maximum size of the upstream buffer.
    UnsynchronizedArrayMemoryResource(void*                       buffer,
                                      std::size_t                 buffer_size_bytes,
                                      UpstreamMemoryResourceType* upstream) noexcept
        : upstream_{upstream}
        , buffer_{buffer}
        , buffer_size_bytes_{buffer_size_bytes}
        , upstream_max_size_bytes_{cetl::pf17::pmr::deviant::memory_resource_traits<
              UpstreamMemoryResourceType>::max_size(*upstream)}
        , max_size_bytes_{calculate_max_size_bytes(buffer_size_bytes, upstream_max_size_bytes_)}
        , in_use_{nullptr}
    {
        CETL_DEBUG_ASSERT(nullptr != upstream,
                          "Upstream memory resource cannot be null. Use std::pmr::null_memory_resource or "
                          "cetl::pmr::null_memory_resource if you don't want an upstream memory resource.");
    }

    ~UnsynchronizedArrayMemoryResource()                                                   = default;
    UnsynchronizedArrayMemoryResource(const UnsynchronizedArrayMemoryResource&)            = delete;
    UnsynchronizedArrayMemoryResource& operator=(const UnsynchronizedArrayMemoryResource&) = delete;
    UnsynchronizedArrayMemoryResource& operator=(UnsynchronizedArrayMemoryResource&&)      = delete;
    UnsynchronizedArrayMemoryResource(UnsynchronizedArrayMemoryResource&&)                 = delete;

    //  +--[public methods]---------------------------------------------------+
    constexpr UpstreamMemoryResourceType* upstream_resource() const
    {
        return upstream_;
    }

    //  +--[memory_resource]--------------------------------------------------+
    void* do_allocate(std::size_t size_bytes, std::size_t alignment = alignof(std::max_align_t)) override
    {
        void* result = nullptr;
        if (!in_use_)
        {
            result = allocate_internal_buffer(size_bytes, alignment);
        }
        if (nullptr != result)
        {
            in_use_ = result;
        }
        else if (upstream_ && size_bytes <= upstream_max_size_bytes_)
        {
            result = upstream_->allocate(size_bytes, alignment);
        }

#if __cpp_exceptions
        if (nullptr == result)
        {
            throw std::bad_alloc();
        }
#endif
        return result;
    }

    void* do_reallocate(void* p, std::size_t old_size_bytes, std::size_t new_size_bytes, std::size_t new_align) override
    {
        (void) old_size_bytes;
        CETL_DEBUG_ASSERT(nullptr == p || in_use_ == p || nullptr != upstream_,
                          "Unknown pointer passed into reallocate.");
        if (p == in_use_)
        {
            return allocate_internal_buffer(new_size_bytes, new_align);
        }
        else
        {
            return nullptr;
        }
        return nullptr;
    }

    void do_deallocate(void* p, std::size_t size_bytes, std::size_t alignment = alignof(std::max_align_t)) override
    {
        CETL_DEBUG_ASSERT(nullptr == p || in_use_ == p || nullptr != upstream_,
                          "Unknown pointer passed into deallocate.");
        if (p == in_use_)
        {
            in_use_ = nullptr;
        }
        else if (nullptr != upstream_)
        {
            upstream_->deallocate(p, size_bytes, alignment);
        }
    }

    bool do_is_equal(const memory_resource& rhs) const noexcept override
    {
        return (this == &rhs);
    }

    std::size_t do_max_size() const noexcept override
    {
        return max_size_bytes_;
    }

private:
    constexpr void* allocate_internal_buffer(std::size_t size_bytes, std::size_t alignment = alignof(std::max_align_t))
    {
        void* result = nullptr;
        if (nullptr != buffer_ && size_bytes <= buffer_size_bytes_)
        {
            void*       storage_ptr  = buffer_;
            std::size_t storage_size = buffer_size_bytes_;
            result                   = std::align(alignment, size_bytes, storage_ptr, storage_size);
        }
        return result;
    }

    UpstreamMemoryResourceType* upstream_;
    void*                       buffer_;
    const std::size_t           buffer_size_bytes_;
    const std::size_t           upstream_max_size_bytes_;
    const std::size_t           max_size_bytes_;
    void*                       in_use_;
};

}  // namespace pmr
}  // namespace cetl

namespace libcyphal
{
namespace janky
{

/// Convience type that packages a vector-like container with a fixed-size memory resource.
/// @tparam T                   The type of the elements in the vector's storage.
/// @tparam StaticStorageSize   The number of T elements size the static storage for.
template <typename T, std::size_t StaticStorageSize>
class UnsynchronizedStaticVector final
{
    using StorageType                             = typename std::aligned_storage<sizeof(T), alignof(T)>::type;
    constexpr static std::size_t StorageSizeBytes = StaticStorageSize * sizeof(StorageType);

public:
    using AllocatorType  = cetl::pf17::pmr::polymorphic_allocator<T>;
    using VectorType     = cetl::VariableLengthArray<T, AllocatorType>;
    using AtType         = optional<std::reference_wrapper<T>>;
    using pointer        = T*;
    using iterator       = pointer;
    using const_iterator = const T*;
    using size_type      = std::size_t;

    UnsynchronizedStaticVector()
        : storage_{}
        , upstream_resource_{cetl::pf17::pmr::null_memory_resource()}
        , memory_resource_{storage_, StorageSizeBytes, upstream_resource_}
        , vector_{AllocatorType{&memory_resource_}}
    {
    }

    explicit UnsynchronizedStaticVector(std::size_t max_size_override) noexcept
        : storage_{}
        , upstream_resource_{cetl::pf17::pmr::null_memory_resource()}
        , memory_resource_{storage_, StorageSizeBytes, upstream_resource_}
        , vector_{AllocatorType{&memory_resource_}, max_size_override}
    {
        vector_.reserve(StaticStorageSize);
    }

    explicit UnsynchronizedStaticVector(std::initializer_list<T> values)
        : storage_{}
        , upstream_resource_{cetl::pf17::pmr::null_memory_resource()}
        , memory_resource_{storage_, StorageSizeBytes, upstream_resource_}
        , vector_{values, AllocatorType{&memory_resource_}}
    {
    }

    ~UnsynchronizedStaticVector() noexcept                                   = default;
    UnsynchronizedStaticVector(const UnsynchronizedStaticVector&)            = delete;
    UnsynchronizedStaticVector& operator=(const UnsynchronizedStaticVector&) = delete;

    UnsynchronizedStaticVector(UnsynchronizedStaticVector&& rhs) noexcept
        : storage_{}
        , upstream_resource_{rhs.upstream_resource_}
        , memory_resource_{storage_, StorageSizeBytes, upstream_resource_}
        , vector_{std::move(rhs.vector_)}
    {
    }
    UnsynchronizedStaticVector& operator=(UnsynchronizedStaticVector&& rhs) noexcept = delete;
    VectorType&                 v()
    {
        return vector_;
    }

    const VectorType& v() const
    {
        return vector_;
    }

    iterator begin() noexcept
    {
        return vector_.data();
    }

    iterator end() noexcept
    {
        return &begin()[vector_.size()];
    }

    const_iterator begin() const noexcept
    {
        return vector_.data();
    }

    const_iterator end() const noexcept
    {
        return &begin()[vector_.size()];
    }

    size_type size() const noexcept
    {
        return vector_.size();
    }

    AtType at(std::size_t index) noexcept
    {
        if (index < vector_.size())
        {
            return make_optional<std::reference_wrapper<T>>(vector_[index]);
        }
        return AtType{janky::nullopt};
    }

    size_type max_size() const noexcept
    {
        return vector_.max_size();
    }

    template <typename... Args>
    void emplace_back(Args&&... args)
    {
        vector_.emplace_back(std::forward<Args>(args)...);
    }

private:
    StorageType                                                                    storage_[StaticStorageSize];
    cetl::pf17::pmr::memory_resource*                                              upstream_resource_;
    cetl::pmr::UnsynchronizedArrayMemoryResource<cetl::pf17::pmr::memory_resource> memory_resource_;
    VectorType                                                                     vector_;
};

/// TODO integrate this functionality into cetl::pmr::Factory
/// Also add null-pointer support while you are at it.
struct DarkPointer final
{
    template <typename T>
    using allocator_t = cetl::pmr::polymorphic_allocator<T>;

    template <typename T>
    using unique_ptr_t = std::unique_ptr<T, cetl::pmr::PolymorphicDeleter<allocator_t<T>>>;

    /// Construct a new concrete type but return a unique_ptr to an interface type for the concrete object.
    /// Because the concrete type is no longer visible, except by using RTTI or janky IPolymorphicType, after the
    /// pointer is constructed it is referred to as a "Dark" pointer.
    template <typename InterfaceType, typename ConcreteType, typename... Args>
    static unique_ptr_t<InterfaceType> make_unique(allocator_t<ConcreteType> concrete_allocator, Args&&... args)
    {
        ConcreteType* s = concrete_allocator.allocate(1);
        if (s)
        {
#if __cpp_exceptions
            try
            {
                concrete_allocator.construct(s, std::forward<Args>(args)...);
            } catch (...)
            {
                concrete_allocator.deallocate(s, 1);
                throw;
            }
#else
            concrete_allocator.construct(s, std::forward<Args>(args)...);
#endif
        }

        typename unique_ptr_t<InterfaceType>::deleter_type td{allocator_t<InterfaceType>{concrete_allocator.resource()},
                                                              1};

        return unique_ptr_t<InterfaceType>{s, td};
    }
    DarkPointer() = delete;
};

}  // namespace janky
}  // namespace libcyphal

#endif  // LIBCYPHAL_JANKY_HPP_INCLUDED
