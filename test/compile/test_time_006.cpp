/*
 * Copyright 2019 Amazon.com, Inc. or its affiliates. All Rights Reserved.
 */
#include "libuavcan/libuavcan.hpp"
#include "libuavcan/time.hpp"

namespace
{
class IllegalDurationForTime : public libuavcan::time::Base<IllegalDurationForTime, int64_t, uint64_t>
{
};
}  // namespace

int main()
{
    IllegalDurationForTime a;
    (void) a;
    return 0;
}
