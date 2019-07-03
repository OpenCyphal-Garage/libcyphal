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
#include <chrono>

#include "libuavcan/libuavcan.hpp"
#include "libuavcan/media/interfaces.hpp"
#include "libuavcan/media/can.hpp"
#include "SocketCANInterfaceManager.hpp"

namespace argparse
{
static struct ::option long_options[] = {{"device", required_argument, nullptr, 'd'},
                                         {"continuous", 0, nullptr, 'c'},
                                         {"c", 0, nullptr, 'e'},
                                         {nullptr, 0, nullptr, 0}};

/**
 * Quick argument parser result structure object modeled on python's argparse (sorta).
 */
struct Namespace
{
    Namespace()
        : device()
        , run_continuously(false)
    {}

    std::string device;
    bool        run_continuously;
};

/**
 * Print a help message for how to use the commandline.
 */
inline void print_usage()
{
    std::cout << "Usage:" << std::endl;
    std::cout << "\t--device, -d      : ip device name to use for the test." << std::endl << std::endl;
    std::cout << "\t--continuous, --c : run continuously. By default this is a pass (0)" << std::endl;
    std::cout << "\t                    fail(non-zero) test. This flag requires ctrl+c" << std::endl;
    std::cout << "\t                    to end the test." << std::endl;
    std::cout << std::endl;
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
        case 'e':
        case 'c':
            out_namespace.run_continuously = true;
            break;

        case 'd':
            out_namespace.device               = optarg;
            long_options[option_index].has_arg = 0;
            break;

        case '?':
            return libuavcan::Result::success_partial;

        default:
            return libuavcan::Result::failure;
        }
    }

    return (long_options[0].has_arg == required_argument) ? libuavcan::Result::bad_argument
                                                          : libuavcan::Result::success;
}

}  // namespace argparse

namespace
{
inline void print_stats(const libuavcan::example::SocketCANInterfaceManager&                manager,
                        const libuavcan::example::SocketCANInterfaceManager::InterfaceType& interface)
{
    static libuavcan::example::SocketCANInterfaceManager::InterfaceType::Statistics stats;
    interface.getStatistics(stats);
    std::cout << manager.getInterfaceName(interface) << ": rx=" << stats.rx_total;
    std::cout << ", rx_dropped=" << stats.rx_dropped;
    std::cout << ", err_ack=" << stats.err_ack;
    std::cout << ", err_bussoff=" << stats.err_bussoff;
    std::cout << ", err_buserror=" << stats.err_buserror;
    std::cout << ", err_crtl=" << stats.err_crtl;
    std::cout << ", err_tx_timeout=" << stats.err_tx_timeout;
    std::cout << ", err_lostarb=" << stats.err_lostarb;
    std::cout << ", err_prot=" << stats.err_prot;
    std::cout << ", err_trx=" << stats.err_trx;
    std::cout << ", err_restarted=" << stats.err_restarted;
    std::cout << std::endl;
}

/**
 * Class to encapsulate our loopback test states.
 */
class LoopBackTest final
{
public:
    LoopBackTest(const LoopBackTest&)  = delete;
    LoopBackTest(const LoopBackTest&&) = delete;
    LoopBackTest& operator=(const LoopBackTest&) = delete;

    static constexpr std::size_t TxTestFramesLen = 2;

    LoopBackTest(libuavcan::example::SocketCANInterfaceManager& manager)
        : manager_(manager)
        , test_frames_{{1,
                        nullptr,
                        libuavcan::media::CAN::FrameDLC::CodeForLength0,
                        libuavcan::time::Monotonic::fromMicrosecond(0)},
                       {2,
                        nullptr,
                        libuavcan::media::CAN::FrameDLC::CodeForLength0,
                        libuavcan::time::Monotonic::fromMicrosecond(1)}}
        , test_frames_found_(0)
    {}

    bool start_test(libuavcan::example::SocketCANInterfaceManager::InterfaceType& interface)
    {
        if (!manager_.doesReceiveOwnMessages())
        {
            std::cout << "You must enable local loopback of frames sent from this process for this test to work."
                      << std::endl;
            return false;
        }
        std::size_t frames_written;
        if (!!interface.write(test_frames_, TxTestFramesLen, frames_written))
        {
            std::cout << "Successfully enqueued " << frames_written << " frame(s) on "
                      << manager_.getInterfaceName(interface) << std::endl;
            return true;
        }
        else
        {
            std::cout << "Failed to enqueue a frame on " << manager_.getInterfaceName(interface) << std::endl;
            return false;
        }
    }

    bool evalute(const libuavcan::example::SocketCANInterfaceManager::InterfaceType::FrameType (
                     &frames)[libuavcan::example::SocketCANInterfaceManager::InterfaceType::RxFramesLen],
                 const std::size_t frames_read)
    {
        std::cout << "Evaluating " << frames_read << " frame(s)..." << std::endl;
        if (test_frames_found_ != 0x03)
        {
            for (size_t i = 0; i < frames_read; ++i)
            {
                for (size_t j = 0; j < 2 && j < sizeof(test_frames_found_) * 8; ++j)
                {
                    const unsigned int frame_bit = 1U << j;
                    if ((test_frames_found_ & frame_bit) == 0 && frames[i] == test_frames_[j])
                    {
                        test_frames_found_ |= 1U << j;
                    }
                }
            }
            if (test_frames_found_ == 0x03)
            {
                std::cout << "...Yep! Got our frames. Test passed." << std::endl;
            }
        }
        return static_cast<bool>(*this);
    }

    explicit operator bool() const
    {
        return (test_frames_found_ & 0x03) == 0x03;
    }

private:
    libuavcan::example::SocketCANInterfaceManager& manager_;

    libuavcan::example::SocketCANInterfaceManager::InterfaceType::FrameType
        test_frames_[libuavcan::example::SocketCANInterface::TxFramesLen];

    unsigned int test_frames_found_;
};
}  // namespace

int main(int argc, char* argv[])
{
    argparse::Namespace args;

    if (libuavcan::Result::success != argparse::parse_args(argc, argv, args))
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

    if (!!manager.getInterfaceIndex(args.device, test_device_index))
    {
        std::cout << "Found " << args.device << " at index " << test_device_index << "." << std::endl;
    }
    else
    {
        std::cout << "Device " << args.device << " was not found." << std::endl;
        return -1;
    }

    // Get everything.
    libuavcan::example::SocketCANInterfaceManager::InterfaceType::FrameType::Filter c;
    c.id   = 0;
    c.mask = 0;
    libuavcan::example::SocketCANInterfaceManager::InterfaceType* interface_ptr;
    if (!manager.openInterface(test_device_index, &c, 1, interface_ptr))
    {
        std::cout << "Failed to open interface " << manager.getInterfaceName(test_device_index) << std::endl;
        return -1;
    }

    // demonstration of how to convert the media layer APIs into RAII patterns.
    auto interface_deleter = [&](libuavcan::example::SocketCANInterfaceManager::InterfaceType* interface) {
        manager.closeInterface(interface);
    };
    std::unique_ptr<libuavcan::example::SocketCANInterfaceManager::InterfaceType, decltype(interface_deleter)>
        interface(interface_ptr, interface_deleter);
    std::cout << "Opened interface " << manager.getInterfaceName(test_device_index) << std::endl;

    {
        // Don't let the test have the manager reference longer than the manager is valid.
        LoopBackTest test(manager);

        auto last_period = std::chrono::steady_clock::now();

        if (!test.start_test(*interface_ptr))
        {
            return -1;
        }

        while (true)
        {
            const libuavcan::example::SocketCANInterfaceManager::InterfaceType* const
                interfaces[libuavcan::example::SocketCANInterfaceManager::MaxSelectInterfaces] = {interface.get(),
                                                                                                  nullptr};
            // Wait for a bit unless some data comes in. Either way, we'll want to loop around and check in on the
            // driver statistics so don't wait too long.
            manager.select(interfaces, 1, libuavcan::duration::Monotonic::fromMicrosecond(100000U), true);

            auto now = std::chrono::steady_clock::now();
            if (now - last_period >= std::chrono::seconds(1))
            {
                print_stats(manager, *interface);
                last_period = now;
            }

            libuavcan::example::SocketCANInterfaceManager::InterfaceType::FrameType
                frames[libuavcan::example::SocketCANInterfaceManager::InterfaceType::RxFramesLen];

            std::size_t frames_read = 0;
            if (!!interface->read(frames, frames_read) && frames_read > 0)
            {
                if (!test && test.evalute(frames, frames_read) && !args.run_continuously)
                {
                    break;
                }
            }
        }
    }
    return 0;
}
