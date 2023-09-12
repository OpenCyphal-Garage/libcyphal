/// @file
/// Compile test that ensures nodiscard attribute is provided when using
/// c++17 or newer.
///
/// @copyright
/// Copyright (C) OpenCyphal Development Team  <opencyphal.org>
/// Copyright Amazon.com Inc. or its affiliates.
/// SPDX-License-Identifier: MIT
///
#include "cetl/cetl.hpp"
#include "cetl/pf20/span.hpp"

#ifndef CETLVAST_COMPILETEST_PRECHECK
static_assert(__cplusplus >= CETL_CPP_STANDARD_17, "We simply pass this test (i.e. fail to compile) for C++14");
#endif

int main()
{
    cetl::pf20::span<int,0> subject;
#ifndef CETLVAST_COMPILETEST_PRECHECK
    subject.empty(); // this should fail because nodiscard is available
#else
    (void)subject;
#endif
    return 0;
}
