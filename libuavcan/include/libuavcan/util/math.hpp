/*
 * Helpers for doing mathy things that aren't provided by the standard library.
 *
 * Copyright (C) 2014 Pavel Kirienko <pavel.kirienko@gmail.com>
 * Copyright 2019 Amazon.com, Inc. or its affiliates. All Rights Reserved.
 */
/** @file */

#ifndef LIBUAVCAN_UTIL_MATH_HPP_INCLUDED
#define LIBUAVCAN_UTIL_MATH_HPP_INCLUDED

#include "libuavcan/libuavcan.hpp"
#include <cstdlib>
#include <cmath>
#include "libuavcan/introspection.hpp"

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
    class SignedType,
    typename std::enable_if<std::is_integral<SignedType>::value && std::is_signed<SignedType>::value, int>::type = 0>
constexpr SignedType _saturating_add_d(SignedType left, SignedType right)
{
    return (left >= 0 && right > (std::numeric_limits<SignedType>::max() - left))
               ? std::numeric_limits<SignedType>::max()
               : static_cast<SignedType>(left + right);
}

/*
 * Saturated subtraction where the RHS is assumed to be a positive value.
 *
 * This is part of the generic, promotionless, saturation math provided by this header. Ideally this would
 * be omitted for most platforms where saturation operations were available.
 */
template <
    class SignedType,
    typename std::enable_if<std::is_integral<SignedType>::value && std::is_signed<SignedType>::value, int>::type = 0>
constexpr SignedType _saturating_sub_d(SignedType left, SignedType right)
{
    return (left <= 0 && right >= std::abs(std::numeric_limits<SignedType>::min() - left))
               ? std::numeric_limits<SignedType>::min()
               : static_cast<SignedType>(left - right);
}

/**
 * Generic implementation of a saturating add operation that does not require integer promotion (i.e. will work with
 * int64_t without needing a 128-bit type). This is very branchy code so we should provide implementations that use
 * hardware support for saturation arithmetic.
 *
 * @tparam SignedType The signed integer type of both operands.
 * @tparam 0 For SFINAE
 * @param left The left operand.
 * @param right The right operand.
 * @return SignedType The result which will saturate to {@code std::numeric_limits<SignedType>::min()} or {@code
 * std::numeric_limits<SignedType>::max()}
 */
template <
    class SignedType,
    typename std::enable_if<std::is_integral<SignedType>::value && std::is_signed<SignedType>::value, int>::type = 0>
SignedType saturating_add(SignedType left, SignedType right)
{
    SignedType result;
    if (right >= 0)  // make sure this is actually substraction
    {
        result = _saturating_add_d<SignedType>(left, right);
    }
    else if (right == std::numeric_limits<SignedType>::min())
    {
        static_assert(std::numeric_limits<SignedType>::max() + std::numeric_limits<SignedType>::min() == -1,
                      "This header assumes two's complement integer representations.");
        // For two's compliment the abs(max) !- abs(min). This is the only
        // place we assume two's compliment. If we need to port this to another integer representation
        // this should be the only thing we need to fix.
        result = _saturating_sub_d<SignedType>(left, std::numeric_limits<SignedType>::max());
        if (result > std::numeric_limits<SignedType>::min())
        {
            result = static_cast<SignedType>(result - 1);
        }
    }
    else
    {
        // Flip the sign-bit and delegate to the subtraction implementation.
        // We assume that the integer representation can flip the bit without
        // promotion where right > std::numeric_limits<SignedType>::min().
        result = _saturating_sub_d<SignedType>(left, static_cast<SignedType>(-right));
    }
    return result;
}

/**
 * Generic implementation of a saturating subtract operation that does not require integer promotion (i.e. will work
 * with int64_t without needing a 128-bit type). This is very branchy code so we should provide implementations that use
 * hardware support for saturation arithmetic.
 *
 * @tparam SignedType The signed integer type of both operands.
 * @tparam 0 For SFINAE
 * @param left The left operand.
 * @param right The right operand.
 * @return SignedType The result which will saturate to {@code std::numeric_limits<SignedType>::min()} or {@code
 * std::numeric_limits<SignedType>::max()}
 */
template <
    class SignedType,
    typename std::enable_if<std::is_integral<SignedType>::value && std::is_signed<SignedType>::value, int>::type = 0>
SignedType saturating_sub(SignedType left, SignedType right)
{
    SignedType result;
    if (right >= 0)  // make sure this is actually substraction
    {
        result = _saturating_sub_d<SignedType>(left, right);
    }
    else if (right == std::numeric_limits<SignedType>::min())
    {
        // For two's compliment the abs(max) !- abs(min). This is the only
        // place we assume two's compliment. If we need to port this to another integer representation
        // this should be the only thing we need to fix.
        result = _saturating_add_d<SignedType>(left, std::numeric_limits<SignedType>::max());
        if (result < std::numeric_limits<SignedType>::max())
        {
            result = static_cast<SignedType>(result + 1);
        }
    }
    else
    {
        // Flip the sign-bit and delegate to the addition implementation.
        // We assume that the integer representation can flip the bit
        // without promotion where right > std::numeric_limits<SignedType>::min().
        result = _saturating_add_d<SignedType>(left, static_cast<SignedType>(-right));
    }
    return result;
}

/**
 * Generic implementation of a saturating add operation that does not require integer promotion (i.e. will work with
 * int64_t without needing a 128-bit type). This is very branchy code so we should provide implementations that use
 * hardware support for saturation arithmetic.
 *
 * @tparam UnsignedType The unsigned integer type of both operands.
 * @tparam 0 For SFINAE
 * @param left The left operand.
 * @param right The right operand.
 * @return UnsignedType The result which will saturate to {@code std::numeric_limits<UnsignedType>::min()} or {@code
 * std::numeric_limits<UnsignedType>::max()}
 */
template <class UnsignedType,
          typename std::enable_if<std::is_integral<UnsignedType>::value && !std::is_signed<UnsignedType>::value,
                                  int>::type = 0>
constexpr UnsignedType saturating_add(UnsignedType left, UnsignedType right)
{
    // Be careful here. Some ways of writing this logic will run afoul of
    // optimizers where the compiler may assume that the result must be <
    // than the rhs and will elide any post processing logic. To make this
    // more stable we've written it as pre-processing logic.
    return (right > static_cast<UnsignedType>(std::numeric_limits<UnsignedType>::max() - left))
               ? std::numeric_limits<UnsignedType>::max()
               : static_cast<UnsignedType>(left + right);
}

/**
 * Generic implementation of a saturating subtraction operation that does not require integer promotion (i.e. will work
 * with int64_t without needing a 128-bit type). This is very branchy code so we should provide implementations that use
 * hardware support for saturation arithmetic.
 *
 * @tparam UnsignedType The unsigned integer type of both operands.
 * @tparam 0 For SFINAE
 * @param left The left operand.
 * @param right The right operand.
 * @return UnsignedType The result which will saturate to {@code std::numeric_limits<UnsignedType>::min()} or {@code
 * std::numeric_limits<UnsignedType>::max()}
 */
template <class UnsignedType,
          typename std::enable_if<std::is_integral<UnsignedType>::value && !std::is_signed<UnsignedType>::value,
                                  int>::type = 0>
constexpr UnsignedType saturating_sub(UnsignedType left, UnsignedType right)
{
    return (right > left) ? std::numeric_limits<UnsignedType>::min() : static_cast<UnsignedType>(left - right);
}

/**
 * Generic implementation of a saturating subtraction operation that does not require integer promotion (i.e. will work
 * with int64_t without needing a 128-bit type). This is very branchy code so we should provide implementations that use
 * hardware support for saturation arithmetic.
 *
 * This version allows a saturated subtraction of a signed integer from an unsigned.
 *
 * @tparam LHS_TYPE The unsigned integer type of the left-hand-side operand.
 * @tparam RHS_TYPE The signed integer type of the right-hand-side operand.
 * @tparam 0 For SFINAE
 * @param left The signed left operand.
 * @param right The unsigned right operand.
 * @return LHS_TYPE The result which will saturate to {@code std::numeric_limits<LHS_TYPE>::min()} or {@code
 * std::numeric_limits<LHS_TYPE>::max()}
 */
template <class LhsType,
          class RhsType,
          typename std::enable_if<std::is_integral<LhsType>::value && !std::is_signed<LhsType>::value &&
                                      std::is_integral<RhsType>::value && std::is_signed<RhsType>::value,
                                  int>::type = 0>
constexpr LhsType saturating_sub(LhsType left, RhsType right)
{
    return (right >= 0) ? saturating_sub<LhsType>(left, static_cast<LhsType>(right))
                        : saturating_add<LhsType>(left, static_cast<LhsType>(right));
}

/**
 * Generic implementation of a saturating subtraction operation that does not require integer promotion (i.e. will work
 * with int64_t without needing a 128-bit type). This is very branchy code so we should provide implementations that use
 * hardware support for saturation arithmetic.
 *
 * This version allows a saturated addition of a signed integer with an unsigned.
 *
 * @tparam LHS_TYPE The unsigned integer type of the left-hand-side operand.
 * @tparam RHS_TYPE The signed integer type of the right-hand-side operand.
 * @tparam 0 For SFINAE
 * @param left The signed left operand.
 * @param right The unsigned right operand.
 * @return LHS_TYPE The result which will saturate to {@code std::numeric_limits<LHS_TYPE>::min()} or {@code
 * std::numeric_limits<LHS_TYPE>::max()}
 */
template <class LhsType,
          class RhsType,
          typename std::enable_if<std::is_integral<LhsType>::value && !std::is_signed<LhsType>::value &&
                                      std::is_integral<RhsType>::value && std::is_signed<RhsType>::value,
                                  int>::type = 0>
constexpr LhsType saturating_add(LhsType left, RhsType right)
{
    return (right >= 0) ? saturating_add<LhsType>(left, static_cast<LhsType>(right))
                        : saturating_sub<LhsType>(left, static_cast<LhsType>(right));
}

}  // namespace util
}  // namespace libuavcan

#endif  // LIBUAVCAN_UTIL_MATH_HPP_INCLUDED
