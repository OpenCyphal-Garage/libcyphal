/*
 * Copyright 2019 Amazon.com, Inc. or its affiliates. All Rights Reserved.
 */
#include "libcyphal/libcyphal.hpp"
#include "libcyphal/time.hpp"

namespace
{
class SignedTime : public libcyphal::time::Base<SignedTime, libcyphal::duration::Monotonic, int64_t>
{
};
}  // namespace

int main()
{
    SignedTime a;
    (void) a;
    return 0;
}
