/// @copyright Copyright 2023 Amazon.com Inc. and its affiliates. All Rights Reserved.
/// @file
/// Implementation of Listener interface from libcyphal that processes broadcast messages,
/// requests, and responses

#ifndef DEMO_BROADCAST_LISTENER_HPP
#define DEMO_BROADCAST_LISTENER_HPP

#include <stdio.h>

#include "libcyphal/presentation/server.hpp"
#include "libcyphal/transport/listener.hpp"
#include "libcyphal/transport/message.hpp"
#include "libcyphal/transport/metadata.hpp"

#include "posix/libcyphal/demo/utilities.hpp"

namespace demo
{

constexpr libcyphal::PortID kDemoMultiframePortID = 4;
constexpr std::size_t       kDemoResponseSize     = 43;

/// @brief Implementation of Listener interface from libcyphal that processes Broadcast messages
class Listener : public libcyphal::transport::Listener
{
public:
    /// @brief Gives the listener access to the server's send response method
    /// @param udp_server The UDP server instance
    void setServer(libcyphal::presentation::Server* udp_server)
    {
        if (udp_server != nullptr)
        {
            udp_server_ = udp_server;
        }
    }

    /// @brief Custom action to do when a payload is received
    /// @param[in] rx_metadata The Cyphal metadata for the payload
    /// @param[in] payload The reference to the buffer to populate
    void onReceive(const libcyphal::transport::RxMetadata& rx_metadata, const libcyphal::Message& payload) noexcept
    {
        if (rx_metadata.kind == libcyphal::transport::TransferKind::TransferKindMessage)
        {
            printf("Listener - Received Broadcast Message with Subject ID: %d\n", rx_metadata.port_id);
            if (rx_metadata.port_id == kDemoMultiframePortID)
            {
                return validate_multiframe_message(payload);
            }

            return print_payload_as_byte_array(payload);
        }
        else if (rx_metadata.kind == libcyphal::transport::TransferKind::TransferKindRequest)
        {
            printf("Listener - Received Request for Service ID: %d\n", rx_metadata.port_id);
            printf("Source Node ID: %u\n", rx_metadata.remote_node_id);
            printf("Transfer ID: %lu\n", rx_metadata.transfer_id);
            printf("Size: %ld\n", payload.size());
            printf("Data: {%s}\n", payload.data());

            return respond(rx_metadata);
        }
        else if (rx_metadata.kind == libcyphal::transport::TransferKind::TransferKindResponse)
        {
            printf("Listener - Received Response for Service ID: %d\n", rx_metadata.port_id);
            printf("Source Node ID: %u\n", rx_metadata.remote_node_id);
            printf("Transfer ID: %lu\n", rx_metadata.transfer_id);
            printf("Size: %ld\n", payload.size());
            printf("Data: {%s}\n", payload.data());
        }
    }

private:
    /// Handle to the UDP Server
    libcyphal::presentation::Server* udp_server_{nullptr};

    /// @brief Helper function to validate a demo multiframe message
    /// @param payload The received data
    void validate_multiframe_message(const libcyphal::Message& payload)
    {
        for (std::uint32_t i = 0; i < payload.size(); i++)
        {
            if (payload.data()[i] != static_cast<std::uint8_t>(i % 100))
            {
                printf("Invalid data\n");
                return;
            }
        }
        printf("Successfully validated %zu byte message\n", payload.size());
    }

    /// @brief Helper function to print payload as  byte array
    /// @param payload The received data
    void print_payload_as_byte_array(const libcyphal::Message& payload)
    {
        printf("Size: %ld, Data: \n{", payload.size());
        for (unsigned int i = 0; i < payload.size(); i++)
        {
            printf("%d ", payload.data()[i]);
            if ((i != 0) && ((i % 50) == 0))
            {
                printf("\n");
            }
        }
        printf("}\n");
    }

    void respond(const libcyphal::transport::RxMetadata& rx_metadata)
    {
        printf("Trying to respond...\n");
        if (udp_server_ == nullptr)
        {
            printf("Server is not initialized, cannot respond.");
            return;
        }

        char response_buffer[kDemoResponseSize];
        snprintf(response_buffer, sizeof(response_buffer), "R E S P O N S E_%u_%lu", rx_metadata.port_id, rx_metadata.transfer_id);

        libcyphal::Status result{};
        result = udp_server_->respond(rx_metadata.port_id,
                                      rx_metadata.remote_node_id,
                                      reinterpret_cast<unsigned char*>(response_buffer),
                                      sizeof(response_buffer));
        if (result.isFailure())
        {
            printf("Failed to send response: %d\n", demo::to_underlying(result.getResultCode()));
        }
    }
};

}  // namespace demo

#endif  // DEMO_BROADCAST_LISTENER_HPP
