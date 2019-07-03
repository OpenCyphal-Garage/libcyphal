/*
 * Copyright 2019 Amazon.com, Inc. or its affiliates. All Rights Reserved.
 */
#include "libuavcan/libuavcan.hpp"
#include "libuavcan/media/interfaces.hpp"
#include "libuavcan/media/can.hpp"

class Dummy : public libuavcan::media::
                  Interface<libuavcan::media::CAN::Frame<libuavcan::media::CAN::TypeFD::MaxFrameSizeBytes>, 0, 4>
{
public:
    virtual std::uint_fast8_t getInterfaceIndex() const override
    {
        return 0;
    }

    virtual libuavcan::Result write(const FrameType (&frame)[TxFramesLen],
                                    std::size_t  frames_len,
                                    std::size_t& out_frames_written) override
    {
        (void)frame;
        (void)frames_len;
        (void)out_frames_written;
        return libuavcan::Result::not_implemented;
    }

    virtual libuavcan::Result read(FrameType (&out_frames)[RxFramesLen], std::size_t& out_frames_read) override
    {
        (void)out_frames;
        (void)out_frames_read;
        return libuavcan::Result::not_implemented;
    }
};

int main()
{
    Dummy a;
    (void)a;
    return 0;
}
