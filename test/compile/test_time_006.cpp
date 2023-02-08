/*
 * Copyright 2023 Amazon.com, Inc. or its affiliates. All Rights Reserved.
 */
#include "libcyphal/libcyphal.hpp"
#include "libcyphal/types/time.hpp"

namespace
{
class IllegalDurationForTime : public libcyphal::types::time::Base<IllegalDurationForTime, int64_t, uint64_t>
{
};
}  // namespace

int main()
{
    IllegalDurationForTime a;
    (void) a;
    return 0;
}
