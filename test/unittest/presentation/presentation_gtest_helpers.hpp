/// @copyright
/// Copyright (C) OpenCyphal Development Team  <opencyphal.org>
/// Copyright Amazon.com Inc. or its affiliates.
/// SPDX-License-Identifier: MIT

#ifndef LIBCYPHAL_PRESENTATION_GTEST_HELPERS_HPP_INCLUDED
#define LIBCYPHAL_PRESENTATION_GTEST_HELPERS_HPP_INCLUDED

#include "gtest_helpers.hpp"
#include "transport/transport_gtest_helpers.hpp"

#include <libcyphal/errors.hpp>
#include <libcyphal/presentation/errors.hpp>
#include <libcyphal/presentation/response_promise.hpp>

#include <cetl/pf17/cetlpf.hpp>
#include <cetl/visit_helpers.hpp>

#include <nunavut/support/serialization.hpp>

#include <gtest/gtest-printers.h>

#include <ostream>

namespace libcyphal
{
namespace presentation
{

// MARK: - GTest Printers:

template <typename Response>
void PrintTo(const typename ResponsePromise<Response>::Success& success, std::ostream* os)
{
    *os << "ResponsePromiseSuccess{meta=" << testing::PrintToString(success.metadata) << "}";
}

template <typename Response>
void PrintTo(const ResponsePromiseFailure& failure, std::ostream* os)
{
    *os << "ResponsePromiseFailure{";
    cetl::visit(cetl::make_overloaded(  //
                    [os](const ResponsePromiseExpired& expired) {
                        *os << "deadline=" << testing::PrintToString(expired.deadline);
                    },
                    [os](const nunavut::support::Error& error) {
                        *os << "NunavutError{code=" << testing::PrintToString(error) << "}";
                    },
                    [os](const MemoryError&) { *os << "MemoryError{}"; }),
                failure);
    *os << "}";
}

inline void PrintTo(const ResponsePromise<void>::Success& success, std::ostream* os)
{
    *os << "RawResponsePromiseSuccess{meta=" << testing::PrintToString(success.metadata)
        << ", response=" << testing::PrintToString(success.response) << "}";
}

inline void PrintTo(const RawResponsePromiseFailure& failure, std::ostream* os)
{
    *os << "RawResponsePromiseFailure{";
    cetl::visit(cetl::make_overloaded(  //
                    [os](const ResponsePromiseExpired& expired) {
                        *os << "deadline=" << testing::PrintToString(expired.deadline);
                    }),
                failure);
    *os << "}";
}

}  // namespace presentation
}  // namespace libcyphal

#endif  // LIBCYPHAL_PRESENTATION_GTEST_HELPERS_HPP_INCLUDED
