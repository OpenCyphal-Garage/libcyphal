/// @copyright Copyright 2023 Amazon.com Inc. and its affiliates. All Rights Reserved.
/// @file
/// Example of a UDP Node with Node ID 43

#include "o1heap.h"
#include "posix/libcyphal/application/udp/node.hpp"
#include "posix/libcyphal/demo/listener.hpp"
#include "posix/libcyphal/demo/utilities.hpp"
#include "posix/libcyphal/utils.hpp"

#include <atomic>
#include <signal.h>
#include <time.h>

using UDPNode = libcyphal::posix::application::udp::UDPNode;
using O1Heap  = libcyphal::O1Heap;

constexpr int  kError       = -1;
constexpr int  kSuccess     = 0;
constexpr long kSleepTimeNs = 1000000000L;

constexpr libcyphal::NodeID kNodeID                  = 43;
constexpr std::uint16_t     kDemoRemoteServerNodeID  = 42;
constexpr libcyphal::PortID kDemoSubjectID           = 3;
constexpr libcyphal::PortID kDemoMultiframeSubjectID = 4;
constexpr libcyphal::PortID kDemoServiceRequestID    = 5;
constexpr libcyphal::PortID kDemoSubjectID_10        = 10;
constexpr libcyphal::PortID kDemoServiceID_20        = 20;
constexpr std::size_t       kDemoMessageSize         = 11;
constexpr std::size_t       kDemoRequestSize         = 21;

int main()
{
    signal(SIGINT, demo::sigint_handler);

    // Memory for udpard. Replace with CETL PMR once merged with upstream.
    alignas(O1HEAP_ALIGNMENT) static std::uint8_t g_HeapArea[2000000] = {0};
    // Initialize Heap
    O1Heap heap{O1Heap(&g_HeapArea[0], sizeof(g_HeapArea))};

    UDPNode udp_node(libcyphal::AddressFromString("172.16.0.2"), kNodeID, heap);

    libcyphal::Status status = udp_node.initialize();
    if (status.isFailure())
    {
        printf("Failed to initialize UDP Node\n");
        return kError;
    }

    // Make Publisher
    libcyphal::presentation::Publisher udp_publisher = udp_node.makePublisher();

    // Register Subject IDs
    status = udp_publisher.registerSubjectID(kDemoSubjectID_10);
    if (status.isFailure())
    {
        printf("Failed to register subject ID: %u\n", kDemoSubjectID_10);
    }

    // User defined Listener
    demo::Listener listener{demo::Listener()};

    // Create Subscriber
    libcyphal::presentation::Subscriber udp_subscriber = udp_node.makeSubscriber();

    // Register subject IDs
    status = udp_subscriber.registerSubjectID(kDemoSubjectID);
    if (status.isFailure())
    {
        printf("Failed to register subject ID: %u\n", kDemoSubjectID);
    }
    status = udp_subscriber.registerSubjectID(kDemoMultiframeSubjectID);
    if (status.isFailure())
    {
        printf("Failed to register subject ID: %u\n", kDemoMultiframeSubjectID);
    }

    // Make Client
    libcyphal::presentation::Client udp_client = udp_node.makeClient();

    // Register Service ID for sending requests and receiving responses
    status = udp_client.registerServiceID(kDemoServiceID_20);
    if (status.isFailure())
    {
        printf("Failed to register service ID: %u\n", kDemoServiceID_20);
    }

    // Create Server
    libcyphal::presentation::Server udp_server = udp_node.makeServer();

    // Register Service ID for receiving requests and sending responses
    status = udp_server.registerServiceID(kDemoServiceRequestID);
    if (status.isFailure())
    {
        printf("Failed to register service ID: %u\n", kDemoServiceRequestID);
    }

    // Give listener access to the server's send response method
    listener.setServer(&udp_server);

    // Demo messages that are just simple raw buffers for now, representing already serialized messages
    std::uint8_t buffer[kDemoMessageSize] = {0, 1, 2, 3, 4, 5, 6, 7, 8, 9, 10};

    // Main Execution Loop
    uint64_t counter = 0;
    while (demo::continue_running)
    {
        // Publish Message
        status = udp_publisher.publish(kDemoSubjectID_10, buffer, sizeof(buffer));
        if (status.isFailure())
        {
            printf("Failed to send message with Port ID: %u\n", kDemoSubjectID_10);
        }

        // Publish Request
        char demo_request_buffer[kDemoRequestSize];
        snprintf(demo_request_buffer, sizeof(demo_request_buffer), "R E Q U E S T_%u_%lu", kDemoServiceID_20, counter);

        libcyphal::Status result{};
        result = udp_client.request(kDemoServiceID_20,
                                    kDemoRemoteServerNodeID,
                                    reinterpret_cast<unsigned char*>(demo_request_buffer),
                                    sizeof(demo_request_buffer));
        if (result.isFailure())
        {
            printf("Failed to send request: %d\n", demo::to_underlying(result.getResultCode()));
        }

         // Receive All Messages, Requests, and Responses
        while (udp_node.receiveAllTransfers(listener).isSuccess()) {}

        demo::high_resolution_sleep(kSleepTimeNs);
        counter++;
    }

    return 0;
}
