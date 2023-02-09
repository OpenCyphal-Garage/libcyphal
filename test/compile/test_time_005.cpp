/*
 * Copyright 2023 Amazon.com, Inc. or its affiliates. All Rights Reserved.
 */
#include "libcyphal/libcyphal.hpp"
#include "libcyphal/types/time.hpp"

namespace
{
class EightByteDuration : public libcyphal::types::duration::Base<EightByteDuration, int64_t>
{
};

class DifferentSizeTimeAndDuration : public libcyphal::types::time::Base<DifferentSizeTimeAndDuration, EightByteDuration, uint32_t>
{
};
}  // namespace

int main()
{
    DifferentSizeTimeAndDuration a;
    (void) a;
    return 0;
}
