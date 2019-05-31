/*
 * Copyright 2019 Amazon.com, Inc. or its affiliates. All Rights Reserved.
 */
#include "libuavcan/libuavcan.hpp"
#include "libuavcan/time.hpp"

namespace
{
class EightByteDuration : public libuavcan::duration::Base<EightByteDuration, int64_t>
{
};

class DifferentSizeTimeAndDuration : public libuavcan::time::Base<DifferentSizeTimeAndDuration, EightByteDuration, uint32_t>
{
};
}  // namespace

int main()
{
    DifferentSizeTimeAndDuration a;
    (void) a;
    return 0;
}
