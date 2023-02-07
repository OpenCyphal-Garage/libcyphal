/*
 * Copyright 2023 Amazon.com, Inc. or its affiliates. All Rights Reserved.
 */
#include "libcyphal/libcyphal.hpp"
#include "libcyphal/time.hpp"

namespace
{
class UnsignedDuration : public libcyphal::duration::Base<UnsignedDuration, uint32_t>
{
};
}  // namespace

int main()
{
    UnsignedDuration a;
    (void) a;
    return 0;
}
