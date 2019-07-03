/*
 * Copyright 2019 Amazon.com, Inc. or its affiliates. All Rights Reserved.
 */
#include "libuavcan/libuavcan.hpp"
#include "libuavcan/media/can.hpp"

int main()
{
    (void)libuavcan::media::CAN::Frame<65>::lengthToDlc(64);
    return 0;
}
