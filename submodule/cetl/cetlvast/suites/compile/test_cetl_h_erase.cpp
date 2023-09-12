/// @file
/// Compile test that ensures CETL_H_ERASE works as intended.
///
/// @copyright
/// Copyright (C) OpenCyphal Development Team  <opencyphal.org>
/// Copyright Amazon.com Inc. or its affiliates.
/// SPDX-License-Identifier: MIT
///


#ifndef CETLVAST_COMPILETEST_PRECHECK
#define CETL_H_ERASE
#endif

// this should have an error macro that prevents compilation.
#include "cetl/cetl.hpp"

int main()
{
    return 0;
}
