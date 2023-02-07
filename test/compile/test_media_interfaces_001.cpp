/*
 * Copyright 2023 Amazon.com, Inc. or its affiliates. All Rights Reserved.
 */
#include "libcyphal/libcyphal.hpp"
#include "libcyphal/media/interfaces.hpp"
#include "libcyphal/media/can.hpp"

class Dummy : public libcyphal::media::
                  InterfaceGroup<libcyphal::media::CAN::Frame<libcyphal::media::CAN::TypeFD::MaxFrameSizeBytes>, 4, 0>
{
public:
    virtual std::uint_fast8_t getInterfaceCount() const override
    {
        return 0;
    }

    virtual libcyphal::Result reconfigureFilters(const typename FrameType::Filter* filter_config,
                                                 std::size_t                       filter_config_length) override
    {
        (void)filter_config;
        (void)filter_config_length;
        return libcyphal::Result::NotImplemented;
    }

    virtual libcyphal::Result select(libcyphal::duration::Monotonic timeout, bool ignore_write_available) override
    {
        (void)timeout;
        (void)ignore_write_available;
        return libcyphal::Result::NotImplemented;
    }

    virtual libcyphal::Result write(std::uint_fast8_t interface_index,
                                    const FrameType (&frame)[TxFramesLen],
                                    std::size_t  frames_len,
                                    std::size_t& out_frames_written) override
    {
        (void) interface_index;
        (void) frame;
        (void) frames_len;
        (void) out_frames_written;
        return libcyphal::Result::NotImplemented;
    }

    virtual libcyphal::Result read(std::uint_fast8_t interface_index,
                                   FrameType (&out_frames)[RxFramesLen],
                                   std::size_t& out_frames_read) override
    {
        (void) interface_index;
        (void) out_frames;
        (void) out_frames_read;
        return libcyphal::Result::NotImplemented;
    }
};

int main()
{
    Dummy a;
    (void) a;
    return 0;
}
