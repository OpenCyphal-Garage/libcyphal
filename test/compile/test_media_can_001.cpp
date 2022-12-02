/*
 * Copyright 2019 Amazon.com, Inc. or its affiliates. All Rights Reserved.
 */
#include "libcyphal/libcyphal.hpp"
#include "libcyphal/media/can.hpp"

int main()
{
    (void)libcyphal::media::CAN::Frame<65>::lengthToDlc(64);
    return 0;
}
