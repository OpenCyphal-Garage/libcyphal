/*
 * Copyright 2019 Amazon.com, Inc. or its affiliates. All Rights Reserved.
 */
/**
 * @defgroup examples Examples
 *
 * Examples are provided as documentation for how libuavcan can be implemented
 * for real systems.
 *
 * @{
 * @file
 * Implements just the media layer of libuavcan on top of <a
 * href="https://www.kernel.org/doc/Documentation/networking/can.txt">SocketCAN</a>.
 * To test using virtual can interfaces on linux see the instructions in <a
 * href="https://en.wikipedia.org/wiki/SocketCAN"> the SocketCAN wiki.</a>. These basically amount to
 * @code
 * sudo ip link add dev vcan0 type vcan
 * sudo ip link set up vcan0
 * @endcode
 * Note that this is a naive and simplistic implementation. While it may be suitable as prototype
 * it should not be used as an example of how to implement the media layer optimally nor
 * is it tested with any rigor so bugs may exists even while the libuavcan build is passing.
 * @}  // end of examples group
 */
#include <iostream>

#include "libuavcan/libuavcan.hpp"
#include "libuavcan/transport/media/interfaces.hpp"
#include "libuavcan/transport/media/can.hpp"
#include "SocketCANInterfaceManager.hpp"

int main(int argc, char* argv[])
{
    (void) argc;
    (void) argv;
    // TODO: get adapter from arguments.
    std::cout << "This test requires a SocketCAN interface." << std::endl;
    // Enable "receive own messages" to allow us to test send and receive using just this one process.
    libuavcan::example::SocketCANInterfaceManager manager(true, true);
    const std::size_t                             found_count = manager.reenumerateInterfaces();
    std::cout << "Found " << found_count << " interfaces." << std::endl;

    libuavcan::example::CanFilterConfig c;
    c.id   = 0xFFFFFFFFU;
    c.mask = 0x00U;
    for (std::uint_fast8_t i = 0, if_count = manager.getHardwareInterfaceCount(); i < if_count; ++i)
    {
        libuavcan::example::CanInterface* interface_ptr;
        if (libuavcan::results::success == manager.openInterface(i, &c, 1, interface_ptr))
        {
            // demonstration of how to convert the media layer APIs into RAII patterns.
            auto interface_deleter = [&](libuavcan::example::CanInterface* interface) {
                manager.closeInterface(interface);
            };
            std::unique_ptr<libuavcan::example::CanInterface, decltype(interface_deleter)> interface(interface_ptr,
                                                                                                     interface_deleter);
            std::cout << "Opened interface " << manager.getInterfaceName(i) << std::endl;

            libuavcan::example::CanFrame test_frames[libuavcan::example::CanInterface::TxFramesLen] =
                {{1,
                  nullptr,
                  libuavcan::transport::media::CAN::FrameDLC::CodeForLength0,
                  libuavcan::time::Monotonic::fromMicrosecond(0)},
                 {2,
                  nullptr,
                  libuavcan::transport::media::CAN::FrameDLC::CodeForLength0,
                  libuavcan::time::Monotonic::fromMicrosecond(1)}};

            std::size_t frames_written;
            if (interface->write(test_frames, 2, frames_written))
            {
                std::cout << "Successfully enqueued " << frames_written << " frame(s) on "
                          << manager.getInterfaceName(*interface_ptr) << std::endl;
            }
            else
            {
                std::cout << "Failed to enqueue a frame on " << manager.getInterfaceName(*interface_ptr) << std::endl;
            }
            while (true)
            {
                libuavcan::example::CanFrame frames[libuavcan::example::CanInterface::RxFramesLen];
                std::size_t                  frames_read = 0;
                if (interface->read(frames, frames_read) && frames_read > 0)
                {
                    std::cout << "Got " << frames_read << " frame(s)..." << std::endl;
                    bool all_matched = false;
                    for (size_t i = 0; i < frames_read; ++i)
                    {
                        if (frames[i] == test_frames[i])
                        {
                            all_matched = true;
                        }
                        else
                        {
                            all_matched = false;
                            break;
                        }
                    }
                    if (!all_matched)
                    {
                        std::cout << "...hmm.  Don't know what some of these are. I'll keep listening." << std::endl;
                    }
                    else
                    {
                        std::cout << "...Yep! Got our frames. Test passed." << std::endl;
                        break;
                    }
                }
            }
        }
        else
        {
            std::cout << "Failed to open interface " << manager.getInterfaceName(i) << std::endl;
        }
    }
    return 0;
}
