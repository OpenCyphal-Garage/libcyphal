/*
 * Copyright 2019 Amazon.com, Inc. or its affiliates. All Rights Reserved.
 */
#include "libuavcan/libuavcan.hpp"
#include "libuavcan/time.hpp"

namespace
{
class UnsignedDuration : public libuavcan::duration::Base<UnsignedDuration, uint32_t>
{
};
}  // namespace

int main()
{
    UnsignedDuration a;
    (void) a;
    return 0;
}
