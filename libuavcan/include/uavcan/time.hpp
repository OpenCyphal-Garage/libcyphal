/*
 * Copyright (C) 2014 Pavel Kirienko <pavel.kirienko@gmail.com>
 * Copyright 2019 Amazon.com, Inc. or its affiliates. All Rights Reserved.
 *
 * Types and utilities for working with time.
 */
/** @file */

#ifndef UAVCAN_TIME_HPP_INCLUDED
#define UAVCAN_TIME_HPP_INCLUDED

#include "uavcan/uavcan.hpp"
#include "uavcan/util/math.hpp"

namespace uavcan
{
/**
 * Protected base class for duration values.
 *
 * @tparam DURATIONTYPE  The datatype returned when retrieving durations from
 *                       realizations of this base class. This type must be
 *                       exactly 8 bytes in size and must support certain concepts
 *                       implied by DurationBase.
 */
template <typename DURATIONTYPE>
class DurationBase
{
    int64_t usec_; /**< Internal storage of the duration value in microseconds. */

protected:
    /**
     * Non-virtual destructor.
     */
    ~DurationBase() = default;

    DurationBase()
        : usec_(0)
    {
        static_assert(sizeof(DURATIONTYPE) == 8, "DURATIONTYPE must be 64 bits wide.");
    }

public:
    static DURATIONTYPE getInfinite()
    {
        return fromUSec(std::numeric_limits<int64_t>::max());
    }

    static DURATIONTYPE fromUSec(int64_t us)
    {
        DURATIONTYPE d;
        d.usec_ = us;
        return d;
    }

    static DURATIONTYPE fromMSec(int64_t ms)
    {
        return fromUSec(ms * 1000);
    }

    int64_t toUSec() const
    {
        return usec_;
    }

    int64_t toMSec() const
    {
        return usec_ / 1000;
    }

    DURATIONTYPE getAbs() const
    {
        return DURATIONTYPE::fromUSec((usec_ < 0) ? (-usec_) : usec_);
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

    bool operator==(const DURATIONTYPE& r) const
    {
        return usec_ == r.usec_;
    }

    bool operator!=(const DURATIONTYPE& r) const
    {
        return !operator==(r);
    }

    bool operator<(const DURATIONTYPE& r) const
    {
        return usec_ < r.usec_;
    }

    bool operator>(const DURATIONTYPE& r) const
    {
        return usec_ > r.usec_;
    }

    bool operator<=(const DURATIONTYPE& r) const
    {
        return usec_ <= r.usec_;
    }

    bool operator>=(const DURATIONTYPE& r) const
    {
        return usec_ >= r.usec_;
    }

    DURATIONTYPE operator+(const DURATIONTYPE& r) const
    {
        return fromUSec(util::saturating_add(usec_, r.usec_));
    }

    DURATIONTYPE operator-(const DURATIONTYPE& r) const
    {
        return fromUSec(util::saturating_sub(usec_, r.usec_));
    }

    DURATIONTYPE operator-() const
    {
        return fromUSec(-usec_);
    }

    DURATIONTYPE& operator+=(const DURATIONTYPE& r)
    {
        *this = *this + r;
        return *static_cast<DURATIONTYPE*>(this);
    }

    DURATIONTYPE& operator-=(const DURATIONTYPE& r)
    {
        *this = *this - r;
        return *static_cast<DURATIONTYPE*>(this);
    }

    template <typename Scale>
    DURATIONTYPE operator*(Scale scale) const
    {
        return fromUSec(usec_ * scale);
    }

    template <typename Scale>
    DURATIONTYPE& operator*=(Scale scale)
    {
        *this = *this * scale;
        return *static_cast<DURATIONTYPE*>(this);
    }
};

class UAVCAN_EXPORT MonotonicDuration : public DurationBase<MonotonicDuration>
{};

}  // namespace uavcan

#endif  // UAVCAN_TIME_HPP_INCLUDED
