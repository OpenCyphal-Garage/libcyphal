/*
 * Copyright 2023 Amazon.com, Inc. or its affiliates. All Rights Reserved.
 */

/**
 * @defgroup examples Examples
 *
 * Examples are provided as documentation for how libcyphal can be implemented
 * for real systems.
 *
 * @{
 * @file
 * Implements just the media layer of libcyphal on top of <a
 * href="https://www.kernel.org/doc/Documentation/networking/can.txt">SocketCAN</a>.
 * To test using virtual can interfaces on linux see the instructions in <a
 * href="https://en.wikipedia.org/wiki/SocketCAN"> the SocketCAN wiki.</a>. These basically amount to
 * @code
 * sudo ip link add dev vcan0 type vcan
 * sudo ip link set up vcan0
 * @endcode
 * Note that this is a naive and simplistic implementation. While it may be suitable as prototype
 * it should not be used as an example of how to implement the media layer optimally nor
 * is it tested with any rigor so bugs may exists even while the libcyphal build is passing.
 * @}  // end of examples group
 */
#include <iostream>
#include <getopt.h>
#include <chrono>
#include <memory>

#include "libcyphal/libcyphal.hpp"
#include "libcyphal/media/interfaces.hpp"
#include "libcyphal/media/can.hpp"
#include "SocketCANInterfaceManager.hpp"

// +--------------------------------------------------------------------------+
// | COMMANDLINE ARGUMENT PARSING
// +--------------------------------------------------------------------------+

namespace
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
        : devices()
        , run_continuously(false)
    {}

    std::vector<std::string> devices;
    bool                     run_continuously;
};

/**
 * Print a help message for how to use the commandline.
 */
inline void print_usage()
{
    std::cout << "Usage:" << std::endl;
    std::cout << "\t--device, -d      : ip device name(s) to use for the test." << std::endl << std::endl;
    std::cout << "\t--continuous, --c : run until the test times out." << std::endl;
    std::cout << std::endl;
    std::cout << "\tTo create a virtual device on linux do:" << std::endl << std::endl;
    std::cout << "\t\tip link add dev vcan0 type vcan" << std::endl;
    std::cout << "\t\tip link set up vcan0" << std::endl << std::endl;
}

/**
 * Quick argument parser object modeled on python's argparse (sorta).
 */
inline libcyphal::Result parse_args(int argc, char* argv[], Namespace& out_namespace)
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
            out_namespace.devices.push_back(optarg);
            long_options[option_index].has_arg = 0;
            break;

        case '?':
            return libcyphal::Result::SuccessPartial;

        default:
            return libcyphal::Result::Failure;
        }
    }

    return (long_options[0].has_arg == required_argument) ? libcyphal::Result::BadArgument : libcyphal::Result::Success;
}

// +--------------------------------------------------------------------------+
// | HELPERS
// +--------------------------------------------------------------------------+
/**
 * Helper to print interface statistics to stdout.
 */
inline void print_stats(const libcyphal::example::SocketCANInterface* interface)
{
    if (nullptr != interface)
    {
        static libcyphal::example::SocketCANInterface::Statistics stats;
        interface->getStatistics(stats);
        std::cout << interface->getInterfaceName() << ": rx=" << stats.rx_total;
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
}

// +--------------------------------------------------------------------------+
// | LoopBackTest
// +--------------------------------------------------------------------------+

/**
 * Test class that manages sending frames and verifing that the frames were
 * looped-back.
 */
class LoopBackTest final
{
    static constexpr std::size_t TxTestFramesLen = 2;
    static constexpr std::size_t TestFiltersLen  = 1;

    const libcyphal::example::SocketCANInterfaceManager::InterfaceGroupType::FrameType
        test_frames_[libcyphal::example::SocketCANInterfaceGroup::TxFramesLen];

    const libcyphal::example::SocketCANInterfaceManager::InterfaceGroupType::FrameType::Filter
                              test_filters_[TestFiltersLen];
    std::vector<unsigned int> test_frames_found_;
    const std::chrono::duration<std::uint64_t> test_timeout_;
    const bool run_continuously_;
    std::chrono::steady_clock::time_point test_started_at_;
    libcyphal::Result test_result_;

public:
    LoopBackTest(const LoopBackTest&)  = delete;
    LoopBackTest(const LoopBackTest&&) = delete;
    LoopBackTest& operator=(const LoopBackTest&) = delete;

    LoopBackTest(bool run_continuously, std::chrono::duration<std::uint64_t> test_timeout = std::chrono::seconds(10))
        : test_frames_{{1,
                        nullptr,
                        libcyphal::media::CAN::FrameDLC::CodeForLength0,
                        libcyphal::time::Monotonic::fromMicrosecond(0)},
                       {2,
                        nullptr,
                        libcyphal::media::CAN::FrameDLC::CodeForLength0,
                        libcyphal::time::Monotonic::fromMicrosecond(1)}}
        , test_filters_{{0, 0}}
        , test_frames_found_()
        , test_timeout_(test_timeout)
        , run_continuously_(run_continuously)
        , test_started_at_()
        , test_result_(libcyphal::Result::Failure)
    {}

    /**
     * Start performing the loopback test.
     * This is a one-shot test so this method can only be called once.
     *
     * @return The interface group obtained from the manager or nullptr if the test failed to start.
     */
    libcyphal::example::SocketCANInterfaceManager::InterfaceGroupPtrType start_test(
        libcyphal::example::SocketCANInterfaceManager& manager)
    {
        libcyphal::example::SocketCANInterfaceManager::InterfaceGroupPtrType interface_group;
        if (test_frames_found_.size() > 0)
        {
            // Test was already started?
            return interface_group;
        }
        if (!manager.doesReceiveOwnMessages())
        {
            std::cout << "You must enable local loopback of frames sent from this process for this test to work."
                      << std::endl;
            return interface_group;
        }
        if (libcyphal::isFailure(manager.startInterfaceGroup(test_filters_, TestFiltersLen, interface_group)))
        {
            return interface_group;
        }
        std::cout << "Opened " << static_cast<unsigned int>(interface_group->getInterfaceCount()) << " interface(s)."
                  << std::endl;
        std::size_t frames_written;
        bool        success = (interface_group->getInterfaceCount() > 0);
        test_frames_found_.clear();
        for (std::uint_fast8_t i = 0; i < interface_group->getInterfaceCount(); ++i)
        {
            if (!!interface_group->write(i, test_frames_, TxTestFramesLen, frames_written))
            {
                std::cout << "Successfully enqueued " << frames_written << " frame(s) on interface "
                          << interface_group->getInterfaceName(i) << std::endl;
                test_frames_found_.push_back(0);
            }
            else
            {
                std::cout << "Failed to enqueue a frame on interface group " << interface_group->getInterfaceName(i)
                          << std::endl;
                success = false;
                break;
            }
        }
        if (!success)
        {
            test_frames_found_.clear();
            interface_group = nullptr;
        }
        test_started_at_ = std::chrono::steady_clock::now();
        return interface_group;
    }

    libcyphal::Result run_test(libcyphal::example::SocketCANInterfaceManager::InterfaceGroupPtrType interface_group)
    {
        auto last_period = std::chrono::steady_clock::now();

        while (true)
        {
            if (std::chrono::steady_clock::now() - test_started_at_ > test_timeout_)
            {
                std::cout << "Test timed out after " << test_timeout_.count() << std::endl;
                break;
            }

            // Wait for a bit unless some data comes in. Either way, we'll want to loop around and check in on the
            // driver statistics so don't wait too long.
            interface_group->select(libcyphal::duration::Monotonic::fromMicrosecond(100000U), true);

            auto now = std::chrono::steady_clock::now();
            if (now - last_period >= std::chrono::seconds(1))
            {
                for (std::uint_fast8_t i = 0; i < interface_group->getInterfaceCount(); ++i)
                {
                    print_stats(interface_group->getInterface(i));
                }
                last_period = now;
            }

            libcyphal::example::SocketCANInterfaceManager::InterfaceGroupType::FrameType
                frames[libcyphal::example::SocketCANInterfaceManager::InterfaceGroupType::RxFramesLen];

            for (std::uint8_t i = 0; i < interface_group->getInterfaceCount(); ++i)
            {
                std::size_t frames_read = 0;
                if (!!interface_group->read(i, frames, frames_read) && frames_read > 0)
                {
                    if (!test_result_)
                    {
                        evaluate(interface_group, i, frames, frames_read);
                    }
                }
            }

            if (isComplete(interface_group))
            {
                test_result_ = libcyphal::Result::Success;
                if (!run_continuously_)
                {
                    break;
                }
            }
        }
        return test_result_;
    }

private:
    /**
     * After the loopback test has started call this method after receiving any messages on a given interfaces.
     * @return true if all the messages for this one interface were now received.
     */
    bool evaluate(libcyphal::example::SocketCANInterfaceManager::InterfaceGroupPtrType interface_group,
                  std::uint_fast8_t                                                    index,
                  libcyphal::example::SocketCANInterfaceManager::InterfaceGroupType::FrameType (
                      &frames)[libcyphal::example::SocketCANInterfaceManager::InterfaceGroupType::RxFramesLen],
                  const std::size_t frames_read)
    {
        std::cout << "Evaluating " << frames_read << " frame(s)..." << std::endl;
        bool          all_found         = false;
        unsigned int& test_frames_found = test_frames_found_[index];
        if (test_frames_found != 0x03)
        {
            for (size_t i = 0; i < frames_read; ++i)
            {
                for (size_t j = 0; j < 2 && j < sizeof(test_frames_found) * 8; ++j)
                {
                    const unsigned int frame_bit = 1U << j;
                    if ((test_frames_found & frame_bit) == 0 && frames[i] == test_frames_[j])
                    {
                        test_frames_found |= 1U << j;
                    }
                }
            }
            if (test_frames_found == 0x03)
            {
                all_found = true;
                std::cout << "...Got all frames for interface " << interface_group->getInterfaceName(index)
                          << std::endl;
            }
        }
        return all_found;
    }

    /**
     * Test to see if all messages were received for all interfaces in a group.
     */
    bool isComplete(libcyphal::example::SocketCANInterfaceManager::InterfaceGroupPtrType interface_group) const
    {
        if (interface_group->getInterfaceCount() == test_frames_found_.size() && test_frames_found_.size() > 0)
        {
            for (std::uint_fast8_t i = 0; i < interface_group->getInterfaceCount(); ++i)
            {
                if ((test_frames_found_[i] & 0x03) != 0x03)
                {
                    return false;
                }
            }
            return true;
        }
        else
        {
            return false;
        }
    }
};
}  // namespace

// +--------------------------------------------------------------------------+
// | main
// +--------------------------------------------------------------------------+

int main(int argc, char* argv[])
{
    Namespace args;

    if (libcyphal::Result::Success != parse_args(argc, argv, args))
    {
        print_usage();
        return -1;
    }

    // Enable "receive own messages" to allow us to test send and receive using just this one process.
    libcyphal::example::SocketCANInterfaceManager manager(std::move(args.devices), true, true);

    // Create our loopback test.
    LoopBackTest test(args.run_continuously);

    // Start the test saving the interface_group so we can use it later if we want to.
    libcyphal::example::SocketCANInterfaceManager::InterfaceGroupPtrType interface_group = test.start_test(manager);

    if (!interface_group)
    {
        return -1;
    }

    {
        auto stop_on_exit = [&manager](libcyphal::example::SocketCANInterfaceManager::InterfaceGroupPtrType* group) {
            if (group && *group)
            {
                manager.stopInterfaceGroup(*group);
                std::cout << "Stopped interface group." << std::endl;
            }
        };

        // Call stopInterfaceGroup when we exit this block.
        std::unique_ptr<libcyphal::example::SocketCANInterfaceManager::InterfaceGroupPtrType, decltype(stop_on_exit)>
            raii_stopper(&interface_group, stop_on_exit);

        if (!test.run_test(interface_group))
        {
            return -1;
        }
        else
        {
            std::cout << "Test passed!" << std::endl;
        }

    }
    return 0;
}
