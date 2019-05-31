/*
 * Copyright (C) 2014 Pavel Kirienko <pavel.kirienko@gmail.com>
 * Copyright 2019 Amazon.com, Inc. or its affiliates. All Rights Reserved.
 *
 * Concepts and requirements for exchanging time values between libuavcan, media
 * layer implementations, and applications.
 */
/** @file */

#ifndef LIBUAVCAN_TIME_HPP_INCLUDED
#define LIBUAVCAN_TIME_HPP_INCLUDED

#include "libuavcan/libuavcan.hpp"
#include "libuavcan/util/math.hpp"

namespace libuavcan
{
namespace duration
{
/**
 * Protected base class for duration values.
 *
 * @tparam Type          The type of the derived class. This must be an empty type.
 *                       All storage will be provided by this base class.
 * @tparam USecT         The datatype returned when retrieving durations from
 *                       realizations of this base class. This type must be signed.
 */
template <typename Type, typename USecT = std::int64_t>
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
                      "The duratiotn abstraction must be the same size as the underlying duration type.");
        static_assert(std::is_signed<USecT>::value, "The microsecond type must be signed for durations.");
    }

public:
    using MicrosecondType = USecT;
    using DurationType    = Type;

    static Type getInfinite()
    {
        return fromMicrosecond(std::numeric_limits<USecT>::max());
    }

    static Type fromMicrosecond(USecT us)
    {
        Type d;
        d.usec_ = us;
        return d;
    }

    USecT toMicrosecond() const
    {
        return usec_;
    }

    USecT toMillisecond() const
    {
        return usec_ / static_cast<USecT>(1000);
    }

    Type getAbs() const
    {
        return Type::fromMicrosecond(std::abs(usec_));
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
        return fromMicrosecond(-usec_);
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

class LIBUAVCAN_EXPORT Monotonic : public Base<Monotonic>
{};

}  // namespace duration

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
template <typename Type, typename DType, typename USecT = uint64_t>
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

public:
    using MicrosecondType = USecT;
    using DurationType    = DType;

    static Type getMax()
    {
        return fromMicrosecond(std::numeric_limits<USecT>::max());
    }

    static Type fromMicrosecond(USecT us)
    {
        Type t;
        t.usec_ = us;
        return t;
    }

    USecT toMicrosecond() const
    {
        return usec_;
    }

    USecT toMillisecond() const
    {
        return usec_ / static_cast<USecT>(1000);
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

    DType operator-(const Type& r) const
    {
        return DType::fromMicrosecond(libuavcan::util::saturating_sub(usec_, r.toMicrosecond()));
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

class LIBUAVCAN_EXPORT Monotonic : public Base<Monotonic, duration::Monotonic>
{};

}  // namespace time

}  // namespace libuavcan

#endif  // LIBUAVCAN_TIME_HPP_INCLUDED
