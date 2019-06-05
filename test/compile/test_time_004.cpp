/*
 * Copyright 2019 Amazon.com, Inc. or its affiliates. All Rights Reserved.
 */
#include "libuavcan/libuavcan.hpp"
#include "libuavcan/time.hpp"

namespace
{
class SignedTime : public libuavcan::time::Base<SignedTime, libuavcan::duration::Monotonic, int64_t>
{
};
}  // namespace

int main()
{
    SignedTime a;
    (void) a;
    return 0;
}
