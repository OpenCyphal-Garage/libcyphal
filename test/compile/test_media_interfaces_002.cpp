/*
 * Copyright 2019 Amazon.com, Inc. or its affiliates. All Rights Reserved.
 */
#include "libuavcan/libuavcan.hpp"
#include "libuavcan/media/interfaces.hpp"
#include "libuavcan/media/can.hpp"

class Dummy : public libuavcan::media::
                  InterfaceGroup<libuavcan::media::CAN::Frame<libuavcan::media::CAN::TypeFD::MaxFrameSizeBytes>, 0, 4>
{
public:
    virtual std::uint_fast8_t getInterfaceCount() const override
    {
        return 0;
    }

    virtual libuavcan::Result reconfigureFilters(const typename FrameType::Filter* filter_config,
                                                 std::size_t                       filter_config_length) override
    {
        (void) filter_config;
        (void) filter_config_length;
        return libuavcan::Result::NotImplemented;
    }

    virtual libuavcan::Result select(libuavcan::duration::Monotonic timeout, bool ignore_write_available) override
    {
        (void) timeout;
        (void) ignore_write_available;
        return libuavcan::Result::NotImplemented;
    }

    virtual libuavcan::Result write(std::uint_fast8_t interface_index,
                                    const FrameType (&frame)[TxFramesLen],
                                    std::size_t  frames_len,
                                    std::size_t& out_frames_written) override
    {
        (void) interface_index;
        (void) frame;
        (void) frames_len;
        (void) out_frames_written;
        return libuavcan::Result::NotImplemented;
    }

    virtual libuavcan::Result read(std::uint_fast8_t interface_index,
                                   FrameType (&out_frames)[RxFramesLen],
                                   std::size_t& out_frames_read) override
    {
        (void) interface_index;
        (void) out_frames;
        (void) out_frames_read;
        return libuavcan::Result::NotImplemented;
    }
};

int main()
{
    Dummy a;
    (void) a;
    return 0;
}
