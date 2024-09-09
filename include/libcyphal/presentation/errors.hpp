/// @copyright
/// Copyright (C) OpenCyphal Development Team  <opencyphal.org>
/// Copyright Amazon.com Inc. or its affiliates.
/// SPDX-License-Identifier: MIT

#ifndef LIBCYPHAL_PRESENTATION_ERRORS_HPP_INCLUDED
#define LIBCYPHAL_PRESENTATION_ERRORS_HPP_INCLUDED

#include "libcyphal/errors.hpp"
#include "libcyphal/types.hpp"

#include <cetl/pf17/cetlpf.hpp>

#include <nunavut/support/serialization.hpp>

namespace libcyphal
{
namespace presentation
{

/// @brief Defines terminal 'expired' error state of the response promise.
///
/// See `response_deadline` parameter of the `Client::request` method,
/// or `setDeadline()` method of the promise itself.
///
struct ResponsePromiseExpired final
{
    /// Holds deadline of the expired (aka timed out) response waiting.
    TimePoint deadline;
};

/// @brief Defines terminal failure state of the raw (aka un-typed) response promise.
///
/// Raw response promise failure state could be only expired. In contrast see `ResponsePromiseFailure`,
/// where the set of possible failure states is extended with additional points of failures.
///
using RawResponsePromiseFailure = cetl::variant<  //
    ResponsePromiseExpired>;

/// @brief Defines terminal failure state of the strong-typed response promise.
///
/// In addition to the raw failure states, this type also includes possible memory allocation errors,
/// aa well as errors from the `nunavut` library in case of response deserialization issues.
///
using ResponsePromiseFailure = libcyphal::detail::AppendType<  //
    RawResponsePromiseFailure,
    MemoryError,
    nunavut::support::Error>::Result;

}  // namespace presentation
}  // namespace libcyphal

#endif  // LIBCYPHAL_PRESENTATION_ERRORS_HPP_INCLUDED
