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
#include <getopt.h>

#include "libuavcan/libuavcan.hpp"
#include "libuavcan/transport/media/interfaces.hpp"
#include "libuavcan/transport/media/can.hpp"
#include "SocketCANInterfaceManager.hpp"

namespace argparse
{
static struct ::option long_options[] = {{"device", required_argument, nullptr, 'd'}, {nullptr, 0, nullptr, 0}};

/**
 * Quick argument parser result structure object modeled on python's argparse (sorta).
 */
struct Namespace
{
    std::string device;
};

/**
 * Print a help message for how to use the commandline.
 */
inline void print_usage()
{
    std::cout << "Usage:" << std::endl;
    std::cout << "\t--device, -d : ip device name to use for the test." << std::endl << std::endl;
    std::cout << "\tTo create a virtual device on linux do:" << std::endl << std::endl;
    std::cout << "\t\tip link add dev vcan0 type vcan" << std::endl;
    std::cout << "\t\tip link set up vcan0" << std::endl << std::endl;
}

/**
 * Quick argument parser object modeled on python's argparse (sorta).
 */
inline libuavcan::Result parse_args(int argc, char* argv[], Namespace& out_namespace)
{
    bool keep_going = true;
    while (keep_going)
    {
        int option_index = 0;

        switch (getopt_long(argc, argv, "d:", long_options, &option_index))
        {
        case -1:
            // no more options.
            keep_going = false;
            break;

        case 'd':
            out_namespace.device               = optarg;
            long_options[option_index].has_arg = 0;
            break;

        case '?':
            return libuavcan::results::success_partial;

        default:
            return libuavcan::results::failure;
        }
    }

    return (long_options[0].has_arg == required_argument) ? libuavcan::results::bad_argument
                                                          : libuavcan::results::success;
}

}  // namespace argparse

int main(int argc, char* argv[])
{
    argparse::Namespace args;

    if (libuavcan::results::success != argparse::parse_args(argc, argv, args))
    {
        argparse::print_usage();
        return -1;
    }

    // Enable "receive own messages" to allow us to test send and receive using just this one process.
    libuavcan::example::SocketCANInterfaceManager manager(true, true);
    std::uint_fast8_t                             test_device_index;
    if (!manager.reenumerateInterfaces())
    {
        std::cout << "Failed to enumerate available CAN interfaces." << std::endl;
    }

    if (0 < manager.getInterfaceIndex(args.device, test_device_index))
    {
        std::cout << "Found " << args.device << " at index " << test_device_index << "." << std::endl;
    }
    else
    {
        std::cout << "Device " << args.device << " was not found." << std::endl;
        return -1;
    }

    libuavcan::example::CanFilterConfig c;
    c.id   = 0xFFFFFFFFU;
    c.mask = 0x00U;
    libuavcan::example::CanInterface* interface_ptr;
    if (libuavcan::results::success == manager.openInterface(test_device_index, &c, 1, interface_ptr))
    {
        // demonstration of how to convert the media layer APIs into RAII patterns.
        auto interface_deleter = [&](libuavcan::example::CanInterface* interface) {
            manager.closeInterface(interface);
        };
        std::unique_ptr<libuavcan::example::CanInterface, decltype(interface_deleter)> interface(interface_ptr,
                                                                                                 interface_deleter);
        std::cout << "Opened interface " << manager.getInterfaceName(test_device_index) << std::endl;

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
        std::cout << "Failed to open interface " << manager.getInterfaceName(test_device_index) << std::endl;
    }
    return 0;
}