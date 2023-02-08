/*
 * Copyright 2023 Amazon.com, Inc. or its affiliates. All Rights Reserved.
 */
#include "libcyphal/libcyphal.hpp"
#include "libcyphal/types/time.hpp"

namespace
{
class SignedTime : public libcyphal::types::time::Base<SignedTime, libcyphal::types::duration::Monotonic, int64_t>
{
};
}  // namespace

int main()
{
    SignedTime a;
    (void) a;
    return 0;
}
