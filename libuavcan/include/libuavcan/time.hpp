/*
 * Copyright (C) 2014 Pavel Kirienko <pavel.kirienko@gmail.com>
 * Copyright 2019 Amazon.com, Inc. or its affiliates. All Rights Reserved.
 *
 * Types and utilities for working with time.
 */
/** @file */

#ifndef LIBUAVCAN_TIME_HPP_INCLUDED
#define LIBUAVCAN_TIME_HPP_INCLUDED

#include "libuavcan/libuavcan.hpp"
#include "libuavcan/util/math.hpp"

namespace libuavcan
{
/**
 * Protected base class for duration values.
 *
 * @tparam DurationType  The datatype returned when retrieving durations from
 *                       realizations of this base class. This type must be
 *                       exactly 8 bytes in size and must support certain concepts
 *                       implied by DurationBase.
 */
template <typename DurationType>
class DurationBase
{
    std::int64_t usec_; /**< Internal storage of the duration value in microseconds. */

protected:
    /**
     * Non-virtual destructor.
     */
    ~DurationBase() = default;

    DurationBase()
        : usec_(0)
    {
        static_assert(sizeof(DurationType) == 8, "DurationType must be 64 bits wide.");
    }

public:
    static DurationType getInfinite()
    {
        return fromMicrosecond(std::numeric_limits<std::int64_t>::max());
    }

    static DurationType fromMicrosecond(std::int64_t us)
    {
        DurationType d;
        d.usec_ = us;
        return d;
    }

    static DurationType fromMillisecond(std::int64_t ms)
    {
        return fromMicrosecond(ms * 1000);
    }

    std::int64_t toMicrosecond() const
    {
        return usec_;
    }

    std::int64_t toMillisecond() const
    {
        return usec_ / 1000;
    }

    DurationType getAbs() const
    {
        return DurationType::fromMicrosecond((usec_ < 0) ? (-usec_) : usec_);
    }

    bool isPositive() const
    {
        return usec_ > 0;
    }

    bool isNegative() const
    {
        return usec_ < 0;
    }

    bool isZero() const
    {
        return usec_ == 0;
    }

    bool operator==(const DurationType& r) const
    {
        return usec_ == r.usec_;
    }

    bool operator!=(const DurationType& r) const
    {
        return !operator==(r);
    }

    bool operator<(const DurationType& r) const
    {
        return usec_ < r.usec_;
    }

    bool operator>(const DurationType& r) const
    {
        return usec_ > r.usec_;
    }

    bool operator<=(const DurationType& r) const
    {
        return usec_ <= r.usec_;
    }

    bool operator>=(const DurationType& r) const
    {
        return usec_ >= r.usec_;
    }

    DurationType operator+(const DurationType& r) const
    {
        return fromMicrosecond(util::saturating_add(usec_, r.usec_));
    }

    DurationType operator-(const DurationType& r) const
    {
        return fromMicrosecond(util::saturating_sub(usec_, r.usec_));
    }

    DurationType operator-() const
    {
        return fromMicrosecond(-usec_);
    }

    DurationType& operator+=(const DurationType& r)
    {
        *this = *this + r;
        return *static_cast<DurationType*>(this);
    }

    DurationType& operator-=(const DurationType& r)
    {
        *this = *this - r;
        return *static_cast<DurationType*>(this);
    }

    template <typename Scale>
    DurationType operator*(Scale scale) const
    {
        return fromMicrosecond(usec_ * scale);
    }

    template <typename Scale>
    DurationType& operator*=(Scale scale)
    {
        *this = *this * scale;
        return *static_cast<DurationType*>(this);
    }
};

class LIBUAVCAN_EXPORT MonotonicDuration : public DurationBase<MonotonicDuration>
{};

}  // namespace libuavcan

#endif  // LIBUAVCAN_TIME_HPP_INCLUDED
