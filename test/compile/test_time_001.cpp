/*
 * Copyright 2023 Amazon.com, Inc. or its affiliates. All Rights Reserved.
 */
#include "libcyphal/libcyphal.hpp"
#include "libcyphal/types/time.hpp"

namespace
{
class TooBig : public libcyphal::types::duration::Base<TooBig>
{
    uint8_t more_;
};
}  // namespace

int main()
{
    TooBig a;
    (void) a;
    return 0;
}
