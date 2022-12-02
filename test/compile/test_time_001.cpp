/*
 * Copyright 2019 Amazon.com, Inc. or its affiliates. All Rights Reserved.
 */
#include "libcyphal/libcyphal.hpp"
#include "libcyphal/time.hpp"

namespace
{
class TooBig : public libcyphal::duration::Base<TooBig>
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
