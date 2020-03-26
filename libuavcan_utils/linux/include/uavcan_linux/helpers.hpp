/*
 * Copyright (C) 2014 Pavel Kirienko <pavel.kirienko@gmail.com>
 * Copyright 2018 Amazon.com, Inc. or its affiliates. All Rights Reserved.
 */

#pragma once

#include <memory>
#include <string>
#include <vector>
#include <chrono>
#include <iostream>
#include <sstream>
#include <uavcan/uavcan.hpp>
#include <uavcan_posix/socketcan.hpp>

namespace uavcan_posix
{
// +--------------------------------------------------------------------------+
// | PLATFORM API HELPERS
// +--------------------------------------------------------------------------+

inline unsigned short getTerminalRows()
{
    auto w = ::winsize();
    UAVCAN_ASSERT(0 >= ioctl(STDOUT_FILENO, TIOCGWINSZ, &w));
    UAVCAN_ASSERT(w.ws_col > 0 && w.ws_row > 0);
    return w.ws_row;
}

inline char* strndup(const char *str, size_t size) 
{ 
    return ::strndup(str, size); 
}

} // namespace uavcan_posix
