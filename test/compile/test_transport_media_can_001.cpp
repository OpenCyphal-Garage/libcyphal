/*
 * Copyright 2019 Amazon.com, Inc. or its affiliates. All Rights Reserved.
 */
#include "libuavcan/libuavcan.hpp"
#include "libuavcan/transport/media/can.hpp"

int main()
{
    (void)libuavcan::transport::media::CAN::Frame<65>::lengthToDlc(64);
    return 0;
}
