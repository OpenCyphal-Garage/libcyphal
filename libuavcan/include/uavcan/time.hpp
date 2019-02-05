/*
 * Copyright (C) 2014 Pavel Kirienko <pavel.kirienko@gmail.com>
 * Copyright 2018 Amazon.com, Inc. or its affiliates. All Rights Reserved.
 */

#ifndef UAVCAN_TIME_HPP_INCLUDED
#define UAVCAN_TIME_HPP_INCLUDED

#include "uavcan/uavcan.hpp"

// DSDL defined types.
#include "uavcan/Timestamp.hpp"

namespace uavcan
{

template <typename D> class DurationBase
{
    int64_t usec_;

  protected:
    ~DurationBase() {}

    DurationBase()
        : usec_(0)
    {
        static_assert(sizeof(D) == 8, "DurationBase's template paramter must be 8 bytes.");
    }

  public:
    static D getInfinite() { return fromUSec(UINT64_MAX); }

    static D fromUSec(int64_t us)
    {
        D d;
        d.usec_ = us;
        return d;
    }
    static D fromMSec(int64_t ms) { return fromUSec(ms * 1000); }

    int64_t toUSec() const { return usec_; }
    int64_t toMSec() const { return usec_ / 1000; }

    D getAbs() const { return D::fromUSec((usec_ < 0) ? (-usec_) : usec_); }

    bool isPositive() const { return usec_ > 0; }
    bool isNegative() const { return usec_ < 0; }
    bool isZero() const { return usec_ == 0; }

    bool operator==(const D& r) const { return usec_ == r.usec_; }
    bool operator!=(const D& r) const { return !operator==(r); }

    bool operator<(const D& r) const { return usec_ < r.usec_; }
    bool operator>(const D& r) const { return usec_ > r.usec_; }
    bool operator<=(const D& r) const { return usec_ <= r.usec_; }
    bool operator>=(const D& r) const { return usec_ >= r.usec_; }

    D operator+(const D& r) const { return fromUSec(usec_ + r.usec_); } // TODO: overflow check
    D operator-(const D& r) const { return fromUSec(usec_ - r.usec_); } // ditto

    D operator-() const { return fromUSec(-usec_); }

    D& operator+=(const D& r)
    {
        *this = *this + r;
        return *static_cast<D*>(this);
    }
    D& operator-=(const D& r)
    {
        *this = *this - r;
        return *static_cast<D*>(this);
    }

    template <typename Scale> D operator*(Scale scale) const { return fromUSec(usec_ * scale); }

    template <typename Scale> D& operator*=(Scale scale)
    {
        *this = *this * scale;
        return *static_cast<D*>(this);
    }

    static const unsigned StringBufSize = 32;
    void toString(char buf[StringBufSize]) const; ///< Prints time in seconds with microsecond resolution
};

// ----------------------------------------------------------------------------

template <typename D> const unsigned DurationBase<D>::StringBufSize;

template <typename D> void DurationBase<D>::toString(char buf[StringBufSize]) const
{
    char* ptr = buf;
    if (isNegative())
    {
        *ptr++ = '-';
    }
    (void)snprintf(ptr, StringBufSize - 1, "%llu.%06lu", static_cast<unsigned long long>(getAbs().toUSec() / 1000000L),
                   static_cast<unsigned long>(getAbs().toUSec() % 1000000L));
}

} // end namespace uavcan

#endif // UAVCAN_TIME_HPP_INCLUDED
