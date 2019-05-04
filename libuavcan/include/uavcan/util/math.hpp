/*
 * Helpers for doing mathy things that aren't provided by the standard library.
 *
 * Copyright (C) 2014 Pavel Kirienko <pavel.kirienko@gmail.com>
 * Copyright 2019 Amazon.com, Inc. or its affiliates. All Rights Reserved.
 */
/** @file */

#ifndef UAVCAN_UTIL_MATH_HPP_INCLUDED
#define UAVCAN_UTIL_MATH_HPP_INCLUDED

#include "uavcan/uavcan.hpp"
#include <cstdlib>
#include <cmath>
#include "uavcan/introspection.hpp"

namespace libuavcan
{
/**
 * @namespace util
 * Contains helper types and functions to make integrating libuavcan easier.
 */
namespace util
{
// +--------------------------------------------------------------------------+
// | GENERIC SATURING MATH
// +--------------------------------------------------------------------------+

/*
 * Saturated addition where the RHS is assumed to be a positive value.
 *
 * This is part of the generic, promotionless, saturation math provided by this header. Ideally this would
 * be omitted for most platforms where saturation operations were available.
 */
template <
    class SIGNED_TYPE,
    typename std::enable_if<std::is_integral<SIGNED_TYPE>::value && std::is_signed<SIGNED_TYPE>::value, int>::type = 0>
constexpr SIGNED_TYPE _saturating_add_d(SIGNED_TYPE left, SIGNED_TYPE right)
{
    return (left >= 0 && right > (std::numeric_limits<SIGNED_TYPE>::max() - left))
               ? std::numeric_limits<SIGNED_TYPE>::max()
               : static_cast<SIGNED_TYPE>(left + right);
}

/*
 * Saturated subtraction where the RHS is assumed to be a positive value.
 *
 * This is part of the generic, promotionless, saturation math provided by this header. Ideally this would
 * be omitted for most platforms where saturation operations were available.
 */
template <
    class SIGNED_TYPE,
    typename std::enable_if<std::is_integral<SIGNED_TYPE>::value && std::is_signed<SIGNED_TYPE>::value, int>::type = 0>
constexpr SIGNED_TYPE _saturating_sub_d(SIGNED_TYPE left, SIGNED_TYPE right)
{
    return (left <= 0 && right >= std::abs(std::numeric_limits<SIGNED_TYPE>::min() - left))
               ? std::numeric_limits<SIGNED_TYPE>::min()
               : static_cast<SIGNED_TYPE>(left - right);
}

/**
 * Generic implementation of a saturating add operation that does not require integer promotion (i.e. will work with
 * int64_t without needing a 128-bit type). This is very branchy code so we should provide implementations that use
 * hardware support for saturation arithmetic.
 *
 * @tparam SIGNED_TYPE The signed integer type of both operands.
 * @tparam 0 For SFINAE
 * @param left The left operand.
 * @param right The right operand.
 * @return SIGNED_TYPE The result which will saturate to {@code std::numeric_limits<SIGNED_TYPE>::min()} or {@code
 * std::numeric_limits<SIGNED_TYPE>::max()}
 */
template <
    class SIGNED_TYPE,
    typename std::enable_if<std::is_integral<SIGNED_TYPE>::value && std::is_signed<SIGNED_TYPE>::value, int>::type = 0>
SIGNED_TYPE saturating_add(SIGNED_TYPE left, SIGNED_TYPE right)
{
    SIGNED_TYPE result;
    if (right >= 0)  // make sure this is actually substraction
    {
        result = _saturating_add_d<SIGNED_TYPE>(left, right);
    }
    else if (right == std::numeric_limits<SIGNED_TYPE>::min())
    {
        static_assert(std::numeric_limits<SIGNED_TYPE>::max() + std::numeric_limits<SIGNED_TYPE>::min() == -1,
                      "This header assumes two's complement integer representations.");
        // For two's compliment the abs(max) !- abs(min). This is the only
        // place we assume two's compliment. If we need to port this to another integer representation
        // this should be the only thing we need to fix.
        result = _saturating_sub_d<SIGNED_TYPE>(left, std::numeric_limits<SIGNED_TYPE>::max());
        if (result > std::numeric_limits<SIGNED_TYPE>::min())
        {
            result = static_cast<SIGNED_TYPE>(result - 1);
        }
    }
    else
    {
        // Flip the sign-bit and delegate to the subtraction implementation.
        // We assume that the integer representation can flip the bit without
        // promotion where right > std::numeric_limits<SIGNED_TYPE>::min().
        result = _saturating_sub_d<SIGNED_TYPE>(left, static_cast<SIGNED_TYPE>(-right));
    }
    return result;
}

/**
 * Generic implementation of a saturating subtract operation that does not require integer promotion (i.e. will work
 * with int64_t without needing a 128-bit type). This is very branchy code so we should provide implementations that use
 * hardware support for saturation arithmetic.
 *
 * @tparam SIGNED_TYPE The signed integer type of both operands.
 * @tparam 0 For SFINAE
 * @param left The left operand.
 * @param right The right operand.
 * @return SIGNED_TYPE The result which will saturate to {@code std::numeric_limits<SIGNED_TYPE>::min()} or {@code
 * std::numeric_limits<SIGNED_TYPE>::max()}
 */
template <
    class SIGNED_TYPE,
    typename std::enable_if<std::is_integral<SIGNED_TYPE>::value && std::is_signed<SIGNED_TYPE>::value, int>::type = 0>
SIGNED_TYPE saturating_sub(SIGNED_TYPE left, SIGNED_TYPE right)
{
    SIGNED_TYPE result;
    if (right >= 0)  // make sure this is actually substraction
    {
        result = _saturating_sub_d<SIGNED_TYPE>(left, right);
    }
    else if (right == std::numeric_limits<SIGNED_TYPE>::min())
    {
        // For two's compliment the abs(max) !- abs(min). This is the only
        // place we assume two's compliment. If we need to port this to another integer representation
        // this should be the only thing we need to fix.
        result = _saturating_add_d<SIGNED_TYPE>(left, std::numeric_limits<SIGNED_TYPE>::max());
        if (result < std::numeric_limits<SIGNED_TYPE>::max())
        {
            result = static_cast<SIGNED_TYPE>(result + 1);
        }
    }
    else
    {
        // Flip the sign-bit and delegate to the addition implementation.
        // We assume that the integer representation can flip the bit
        // without promotion where right > std::numeric_limits<SIGNED_TYPE>::min().
        result = _saturating_add_d<SIGNED_TYPE>(left, static_cast<SIGNED_TYPE>(-right));
    }
    return result;
}

/**
 * Generic implementation of a saturating add operation that does not require integer promotion (i.e. will work with
 * int64_t without needing a 128-bit type). This is very branchy code so we should provide implementations that use
 * hardware support for saturation arithmetic.
 *
 * @tparam UNSIGNED_TYPE The unsigned integer type of both operands.
 * @tparam 0 For SFINAE
 * @param left The left operand.
 * @param right The right operand.
 * @return UNSIGNED_TYPE The result which will saturate to {@code std::numeric_limits<UNSIGNED_TYPE>::min()} or {@code
 * std::numeric_limits<UNSIGNED_TYPE>::max()}
 */
template <class UNSIGNED_TYPE,
          typename std::enable_if<std::is_integral<UNSIGNED_TYPE>::value && !std::is_signed<UNSIGNED_TYPE>::value,
                                  int>::type = 0>
constexpr UNSIGNED_TYPE saturating_add(UNSIGNED_TYPE left, UNSIGNED_TYPE right)
{
    // Be careful here. Some ways of writing this logic will run afoul of
    // optimizers where the compiler may assume that the result must be <
    // than the rhs and will elide any post processing logic. To make this
    // more stable we've written it as pre-processing logic.
    return (right > static_cast<UNSIGNED_TYPE>(std::numeric_limits<UNSIGNED_TYPE>::max() - left))
               ? std::numeric_limits<UNSIGNED_TYPE>::max()
               : static_cast<UNSIGNED_TYPE>(left + right);
}

/**
 * Generic implementation of a saturating subtraction operation that does not require integer promotion (i.e. will work
 * with int64_t without needing a 128-bit type). This is very branchy code so we should provide implementations that use
 * hardware support for saturation arithmetic.
 *
 * @tparam UNSIGNED_TYPE The unsigned integer type of both operands.
 * @tparam 0 For SFINAE
 * @param left The left operand.
 * @param right The right operand.
 * @return UNSIGNED_TYPE The result which will saturate to {@code std::numeric_limits<UNSIGNED_TYPE>::min()} or {@code
 * std::numeric_limits<UNSIGNED_TYPE>::max()}
 */
template <class UNSIGNED_TYPE,
          typename std::enable_if<std::is_integral<UNSIGNED_TYPE>::value && !std::is_signed<UNSIGNED_TYPE>::value,
                                  int>::type = 0>
constexpr UNSIGNED_TYPE saturating_sub(UNSIGNED_TYPE left, UNSIGNED_TYPE right)
{
    return (right > left) ? std::numeric_limits<UNSIGNED_TYPE>::min() : static_cast<UNSIGNED_TYPE>(left - right);
}

}  // namespace util
}  // namespace libuavcan

#endif  // UAVCAN_UTIL_MATH_HPP_INCLUDED
