/// @copyright
/// Copyright (C) OpenCyphal Development Team  <opencyphal.org>
/// Copyright Amazon.com Inc. or its affiliates.
/// SPDX-License-Identifier: MIT

#ifndef LIBCYPHAL_PRESENTATION_GTEST_HELPERS_HPP_INCLUDED
#define LIBCYPHAL_PRESENTATION_GTEST_HELPERS_HPP_INCLUDED

#include <libcyphal/presentation/response_promise.hpp>

#include <gmock/gmock.h>
#include <gtest/gtest-matchers.h>
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
    *os << "Promise<Response>::Success{meta=" << testing::PrintToString(success.metadata) << "}";
}

template <typename Response>
void PrintTo(const typename ResponsePromise<Response>::Expired&, std::ostream* os)
{
    *os << "Promise<Response>::Expired{}";
}

inline void PrintTo(const ResponsePromise<void>::Success& success, std::ostream* os)
{
    *os << "Promise<void>::Success{meta=" << testing::PrintToString(success.metadata)
        << ", response=" << testing::PrintToString(success.response) << "}";
}

inline void PrintTo(const ResponsePromise<void>::Expired&, std::ostream* os)
{
    *os << "Promise<void>::Expired{}";
}

}  // namespace presentation
}  // namespace libcyphal

#endif  // LIBCYPHAL_PRESENTATION_GTEST_HELPERS_HPP_INCLUDED
