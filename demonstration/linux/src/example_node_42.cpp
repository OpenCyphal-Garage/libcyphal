/// @copyright Copyright 2023 Amazon.com Inc. and its affiliates. All Rights Reserved.
/// @file
/// Example of a UDP Node with Node 42

#include "o1heap.h"
#include "posix/libcyphal/demo/listener.hpp"
#include "posix/libcyphal/demo/utilities.hpp"
#include "posix/libcyphal/application/udp/node.hpp"
#include "posix/libcyphal/utils.hpp"

using UDPNode = libcyphal::posix::application::udp::UDPNode;
using O1Heap  = libcyphal::O1Heap;

constexpr int  kError       = -1;
constexpr int  kSuccess     = 0;
constexpr long kSleepTimeNs = 1000000000L;

constexpr libcyphal::NodeID kNodeID                  = 42;
constexpr std::uint16_t     kDemoRemoteServerNodeID  = 43;
constexpr libcyphal::PortID kDemoSubjectID           = 3;
constexpr libcyphal::PortID kDemoMultiframeSubjectID = 4;
constexpr libcyphal::PortID kDemoServiceID           = 5;
constexpr libcyphal::PortID kDemoSubjectID_10        = 10;
constexpr libcyphal::PortID kDemoServiceID_20        = 20;

constexpr std::size_t kDemoMessageSize           = 11;
constexpr std::size_t kDemoMultiframeMessageSize = 64000;
constexpr std::size_t kDemoRequestSize           = 21;

int main()
{
    signal(SIGINT, demo::sigint_handler);

    // Memory for udpard. Replace with CETL PMR once merged with upstream.
    alignas(O1HEAP_ALIGNMENT) static std::uint8_t g_HeapArea[2000000] = {0};
    // Initialize Heap
    O1Heap heap{O1Heap(&g_HeapArea[0], sizeof(g_HeapArea))};

    // Create Node
    UDPNode udp_node(libcyphal::AddressFromString("172.16.0.1"), kNodeID, heap);

    // Initialize the Node
    libcyphal::Status status = udp_node.initialize();
    if (status.isFailure())
    {
        printf("Failed to initialize UDP Node\n");
        return kError;
    }

    // Make Publisher
    libcyphal::presentation::Publisher udp_publisher = udp_node.makePublisher();

    // Register Subject IDs
    status = udp_publisher.registerSubjectID(kDemoSubjectID);
    if (status.isFailure())
    {
        printf("Failed to register subject ID: %u\n", kDemoSubjectID);
    }
    status = udp_publisher.registerSubjectID(kDemoMultiframeSubjectID);
    if (status.isFailure())
    {
        printf("Failed to register subject ID: %u\n", kDemoMultiframeSubjectID);
    }

    // User defined Listener
    demo::Listener listener{demo::Listener()};

    // Create Subscriber
    libcyphal::presentation::Subscriber udp_subscriber = udp_node.makeSubscriber();

    // Register subject IDs
    status = udp_subscriber.registerSubjectID(kDemoSubjectID_10);
    if (status.isFailure())
    {
        printf("Failed to register subject ID: %u\n", kDemoSubjectID_10);
    }

    // Make Client
    libcyphal::presentation::Client udp_client = udp_node.makeClient();

    // Register Service ID for sending requests and receiving responses
    status = udp_client.registerServiceID(kDemoServiceID);
    if (status.isFailure())
    {
        printf("Failed to register service ID: %u\n", kDemoServiceID);
    }

    // Make Server
    libcyphal::presentation::Server udp_server = udp_node.makeServer();

    // Register Service ID for receiving requests and sending responses
    status = udp_server.registerServiceID(kDemoServiceID_20);
    if (status.isFailure())
    {
        printf("Failed to register service ID: %u, Error: %d\n",
               kDemoServiceID_20,
               demo::to_underlying(status.getResultCode()));
    }

    // Give listener access to the server's send response method
    listener.setServer(&udp_server);

    // Demo messages that are just simple raw buffers for now, representing already serialized messages
    std::uint8_t buffer[kDemoMessageSize] = {10, 9, 8, 7, 6, 5, 4, 3, 2, 1, 0};
    std::uint8_t multiframe_buffer[kDemoMultiframeMessageSize];
    memset(&multiframe_buffer[0], 0, kDemoMultiframeMessageSize);
    for (std::uint32_t i = 0; i < kDemoMultiframeMessageSize; i++)
    {
        multiframe_buffer[i] = static_cast<std::uint8_t>(i % 100);
    }

    // Main Execution Loop
    uint64_t counter = 0;
    while (demo::continue_running)
    {
        // Publish Message
        status = udp_publisher.publish(kDemoSubjectID, buffer, sizeof(buffer));
        if (status.isFailure())
        {
            printf("Failed to send message with Port ID: %u\n", kDemoSubjectID);
        }

        // Publish Multiframe Message
        status = udp_publisher.publish(kDemoMultiframeSubjectID, multiframe_buffer, sizeof(multiframe_buffer));
        if (status.isFailure())
        {
            printf("Failed to send message with Port ID: %u\n", kDemoMultiframeSubjectID);
        }

        // Publish Request
        char demo_request_buffer[kDemoRequestSize];
        snprintf(demo_request_buffer, sizeof(demo_request_buffer), "R E Q U E S T_%u_%lu", kDemoServiceID, counter);

        libcyphal::Status result{};
        result = udp_client.request(kDemoServiceID,
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

    return kSuccess;
}
