/*
 * Copyright 2019 Amazon.com, Inc. or its affiliates. All Rights Reserved.
 */
#include "libcyphal/libcyphal.hpp"
#include "libcyphal/time.hpp"

namespace
{
class IllegalDurationForTime : public libcyphal::time::Base<IllegalDurationForTime, int64_t, uint64_t>
{
};
}  // namespace

int main()
{
    IllegalDurationForTime a;
    (void) a;
    return 0;
}
