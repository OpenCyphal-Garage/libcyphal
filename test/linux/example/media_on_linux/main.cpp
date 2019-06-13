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
 * it is tested with any rigor so bugs may exists even while the libuavcan build is passing.
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
    std::cout << "starting up." << std::endl;
    libuavcan::example::SocketCANInterfaceManager manager;
    const std::size_t                             found_count = manager.reenumerateInterfaces();
    std::cout << "Found " << found_count << " interfaces." << std::endl;

    for (std::size_t i = 0, if_count = manager.getHardwareInterfaceCount(); i < if_count; ++i)
    {
        libuavcan::example::CanInterface* interface_ptr;
        if (0 == manager.openInterface(i, nullptr, 0, interface_ptr))
        {
            // demonstration of how to convert the media layer APIs into RAII patterns.
            auto interface_deleter = [&](libuavcan::example::CanInterface* interface) {
                manager.closeInterface(interface);
            };
            std::unique_ptr<libuavcan::example::CanInterface, decltype(interface_deleter)>
                interface(interface_ptr, interface_deleter);
            std::cout << "Opened interface " << manager.getInterfaceName(i) << std::endl;

            libuavcan::example::CanFrame test_frame{1,
                                                    libuavcan::time::Monotonic::fromMicrosecond(0),
                                                    nullptr,
                                                    libuavcan::transport::media::CAN::FrameDLC::CodeForLength0};

            if (0 <= interface->enqueue(test_frame))
            {
                std::cout << "Successfully enqueued a frame on " << manager.getInterfaceName(*interface_ptr)
                          << std::endl;
            }
            else
            {
                std::cout << "Failed to enqueue a frame on " << manager.getInterfaceName(*interface_ptr) << std::endl;
            }
            while (true)
            {
                interface->exchange();
            }
        }
        else
        {
            std::cout << "Failed to open interface " << manager.getInterfaceName(i) << std::endl;
        }
    }
    return 0;
}
