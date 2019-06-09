/*
 * Copyright (C) 2014 Pavel Kirienko <pavel.kirienko@gmail.com>
 * Copyright 2019 Amazon.com, Inc. or its affiliates. All Rights Reserved.
 *
 * Concepts and requirements for exchanging time values between libuavcan, media
 * layer implementations, and applications.
 */
/** @file
 * This header specifies the concepts used by libuavcan when handling time scalars and vectors.
 * Applications may optionally choose to extend these concepts for their own use but shall always
 * use them, as documented, when exchanging data with libuavcan.
 *
 * <h3>Signed Integer Assumptions</h3>
 * Please note that libuavcan makes some assumptions that signed integers are represented as
 * twos compliment by a machine. You may experience undefined behaviour if your architecture
 * does not use twos compliment integers.
 */

#ifndef LIBUAVCAN_TIME_HPP_INCLUDED
#define LIBUAVCAN_TIME_HPP_INCLUDED

#include "libuavcan/libuavcan.hpp"
#include "libuavcan/util/math.hpp"

namespace libuavcan
{
/**
 * The default signed integer type used in libuavcan for signed microseconds (e.g. all duration types).
 */
using DefaultMicrosecondSignedType = std::int64_t;

/**
 * The default unsigned integer type used in libuavcan for unsigned microseconds (e.g. all time types).
 */
using DefaultMicrosecondUnsignedType = std::uint64_t;

/**
 * @namespace duration
 * Contains concepts and types that implement these concepts for time vector values.
 */
namespace duration
{
/**
 * Protected base class for duration values. This provides a common implementation for
 * various duration datatypes and enforces two concepts:
 *
 * -# duration math is saturing – MAX_DURATION + 1 == MAX_DURATION
 * -# durations are signed integers – By default 8 byte integers but USecT can be redefined by
 *    a specialization.
 *
 * @tparam Type          The type of the derived class. This must be an empty type.
 *                       All storage will be provided by this base class.
 * @tparam USecT         The datatype returned when retrieving durations from
 *                       realizations of this base class. This type must be signed.
 */
template <typename Type, typename USecT = libuavcan::DefaultMicrosecondSignedType>
class Base
{
    USecT usec_; /**< Internal storage of the duration value in microseconds. */

protected:
    /**
     * Non-virtual destructor.
     */
    ~Base() = default;

    Base()
        : usec_(0)
    {
        static_assert(sizeof(Type) == sizeof(USecT),
                      "The duration abstraction must be the same size as the underlying duration type.");
        static_assert(std::is_signed<USecT>::value, "The microsecond type must be signed for durations.");
    }

    Base(const Base& rhs)
        : usec_(rhs.usec_)
    {}

    /**
     * Move constructor takes value from rhs and
     * resets rhs to 0.
     */
    Base(Base&& rhs)
        : usec_(rhs.usec_)
    {
        rhs.usec_ = 0;
    }

public:
    /**
     * The underlying datatype for microsecond values. This must be signed for duration types.
     */
    using MicrosecondType = USecT;

    /**
     * The specialized type of this base duration type.
     */
    using DurationType = Type;

    /**
     * Get the largest possible number of microseconds this type can store.
     */
    static Type getMaximum()
    {
        return fromMicrosecond(std::numeric_limits<USecT>::max());
    }

    /**
     * Construct an instance of Type from a microsecond value.
     */
    static Type fromMicrosecond(USecT us)
    {
        Type d;
        d.usec_ = us;
        return d;
    }

    /**
     * Obtain the underlying microsecond value without conversion.
     */
    USecT toMicrosecond() const
    {
        return usec_;
    }

    /**
     * Get the absolute value of the duration as a duration type.
     */
    Type getAbs() const
    {
        return Type::fromMicrosecond(std::abs(usec_));
    }

    Base& operator=(Base&& rhs)
    {
        usec_     = rhs.usec_;
        rhs.usec_ = 0;
        return *this;
    }

    Base& operator=(const Base& rhs)
    {
        usec_ = rhs.usec_;
        return *this;
    }

    bool operator==(const Type& r) const
    {
        return usec_ == r.usec_;
    }

    bool operator!=(const Type& r) const
    {
        return !operator==(r);
    }

    bool operator<(const Type& r) const
    {
        return usec_ < r.usec_;
    }

    bool operator>(const Type& r) const
    {
        return usec_ > r.usec_;
    }

    bool operator<=(const Type& r) const
    {
        return usec_ <= r.usec_;
    }

    bool operator>=(const Type& r) const
    {
        return usec_ >= r.usec_;
    }

    Type operator+(const Type& r) const
    {
        return fromMicrosecond(util::saturating_add(usec_, r.usec_));
    }

    Type operator-(const Type& r) const
    {
        return fromMicrosecond(util::saturating_sub(usec_, r.usec_));
    }

    Type operator-() const
    {
        if (usec_ == std::numeric_limits<USecT>::min())
        {
            return fromMicrosecond(std::numeric_limits<USecT>::max());
        }
        else
        {
            return fromMicrosecond(-usec_);
        }
    }

    Type& operator+=(const Type& r)
    {
        *this = *this + r;
        return *static_cast<Type*>(this);
    }

    Type& operator-=(const Type& r)
    {
        *this = *this - r;
        return *static_cast<Type*>(this);
    }
};

/**
 * A monotonic duration used by libuavcan.
 */
class LIBUAVCAN_EXPORT Monotonic : public Base<Monotonic>
{};

}  // namespace duration

/**
 * @namespace time
 * Contains concepts and types that implement these concepts for time scalar values.
 */
namespace time
{
/**
 * Protected base class for time values.
 *
 * @tparam Type          The type of the derived class. This must be an empty type.
 *                       All storage will be provided by this base class.
 * @tparam DType         The type of duration used for this time type. Time is concrete and duration
 *                       is relative.
 * @tparam USecT         The datatype returned when retrieving time from
 *                       realizations of this base class. This type must be unsigned.
 */
template <typename Type, typename DType, typename USecT = DefaultMicrosecondUnsignedType>
class Base
{
    USecT usec_;

protected:
    ~Base() {}

    Base()
        : usec_(0)
    {
        static_assert(sizeof(Type) == sizeof(USecT),
                      "The time abstraction must be the same size as the underlying time type.");
        static_assert(!std::is_signed<USecT>::value, "Microsecond type must be unsigned for time.");
        // Note that this also, somewhat, enforces that the duration type supports the duration "concept".
        // It won't be until C++20 that this type can truly enforce this requirement. If you must re-implement
        // the concept then remember that Duration math is saturating. It's much safer to just derive
        // your duration from libuavcan::time::Base.
        static_assert(sizeof(USecT) == sizeof(typename DType::MicrosecondType),
                      "Microsecond Type must be the same size as the duration type.");
    }

    Base(const Base& rhs)
        : usec_(rhs.usec_)
    {}

    /**
     * Move constructor takes value from rhs and
     * resets rhs to 0.
     */
    Base(Base&& rhs)
        : usec_(rhs.usec_)
    {
        rhs.usec_ = 0;
    }

public:
    /**
     * The underlying datatype for microsecond values. This must be unsigned for time types.
     */
    using MicrosecondType = USecT;

    /**
     * The specialized type of this base time type.
     */
    using DurationType = DType;

    /**
     * Get the largest possible number of microseconds this type can store.
     */
    static Type getMaximum()
    {
        return fromMicrosecond(std::numeric_limits<USecT>::max());
    }

    /**
     * Construct an instance of Type from a microsecond value.
     */
    static Type fromMicrosecond(USecT us)
    {
        Type t;
        t.usec_ = us;
        return t;
    }

    /**
     * Obtain the underlying microsecond value without conversion.
     */
    USecT toMicrosecond() const
    {
        return usec_;
    }

    Base& operator=(Base&& rhs)
    {
        usec_     = rhs.usec_;
        rhs.usec_ = 0;
        return *this;
    }

    Base& operator=(const Base& rhs)
    {
        usec_ = rhs.usec_;
        return *this;
    }

    bool operator==(const Type& r) const
    {
        return usec_ == r.usec_;
    }

    bool operator!=(const Type& r) const
    {
        return !operator==(r);
    }

    bool operator<(const Type& r) const
    {
        return usec_ < r.usec_;
    }

    bool operator>(const Type& r) const
    {
        return usec_ > r.usec_;
    }

    bool operator<=(const Type& r) const
    {
        return usec_ <= r.usec_;
    }

    bool operator>=(const Type& r) const
    {
        return usec_ >= r.usec_;
    }

    Type operator+(const DType& r) const
    {
        return fromMicrosecond(libuavcan::util::saturating_add(usec_, r.toMicrosecond()));
    }

    Type operator-(const DType& r) const
    {
        return fromMicrosecond(libuavcan::util::saturating_sub(usec_, r.toMicrosecond()));
    }

    Type& operator+=(const DType& r)
    {
        *this = *this + r;
        return *static_cast<Type*>(this);
    }

    Type& operator-=(const DType& r)
    {
        *this = *this - r;
        return *static_cast<Type*>(this);
    }
};

/**
 * A monotonic time value used by libuavcan.
 */
class LIBUAVCAN_EXPORT Monotonic : public Base<Monotonic, duration::Monotonic>
{};

}  // namespace time

}  // namespace libuavcan

#endif  // LIBUAVCAN_TIME_HPP_INCLUDED
