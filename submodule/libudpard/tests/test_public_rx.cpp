// This software is distributed under the terms of the MIT License.
// Copyright (c) 2016-2020 OpenCyphal Development Team.
/// Copyright 2022 Amazon.com, Inc. or its affiliates. All Rights Reserved.

#include "exposed.hpp"
#include "helpers.hpp"
#include "catch/catch.hpp"
#include <cstring>

constexpr uint16_t UDPARD_SUBJECT_ID_PORT = 16383U;
constexpr uint16_t UDPARD_UDP_PORT = 9382U;

// clang-tidy mistakenly suggests to avoid C arrays here, which is clearly an error
template <typename P, std::size_t N>
auto ensureAllNullptr(P* const (&arr)[N]) -> bool  // NOLINT
{
    return std::all_of(std::begin(arr), std::end(arr), [](const auto* const x) { return x == nullptr; });
}

TEST_CASE("RxBasic0")
{
    using helpers::Instance;
    using exposed::RxSession;

    Instance               ins;
    Instance               ins_new;
    UdpardRxTransfer       transfer{};
    UdpardSessionSpecifier specifier{};
    UdpardFrameHeader      header{};
    UdpardRxSubscription*  subscription = nullptr;

    const auto accept = [&](const std::uint8_t               redundant_transport_index,
                            const UdpardMicrosecond          timestamp_usec,
                            UdpardFrameHeader                frame_header,
                            UdpardSessionSpecifier           session_specifier,
                            const std::vector<std::uint8_t>& payload) {
        auto *header_ptr      = reinterpret_cast<std::uint8_t*>(&frame_header);
        auto payload_storage = std::vector<std::uint8_t>(header_ptr, header_ptr + sizeof(frame_header));
        static std::vector<std::uint8_t> payload_buffer;
        payload_buffer = payload;
        payload_storage.insert(payload_storage.end(), payload_buffer.begin(), payload_buffer.end());
        UdpardFrame frame{};
        frame.payload_size = std::size(payload) + sizeof(frame_header);
        frame.payload      = payload_storage.data();
        return ins
            .rxAccept(timestamp_usec, frame, redundant_transport_index, session_specifier, transfer, &subscription);
    };

    ins.getAllocator().setAllocationCeiling(sizeof(RxSession) + sizeof(UdpardFrameHeader) +
                                            16);  // A session and a 16-byte payload buffer.

    // No subscriptions by default.
    REQUIRE(ins.getMessageSubs().empty());
    REQUIRE(ins.getResponseSubs().empty());
    REQUIRE(ins.getRequestSubs().empty());

    // Some initial header setup
    header.version     = 1;

    // A valid single-frame transfer for which there is no subscription.
    subscription                          = nullptr;
    header.priority                       = 0b001;
    header.source_node_id                 = 0b0000000000100111;
    header.destination_node_id            = 0b1111111111111111;
    header.data_specifier                 = 0b0000110011001100;
    header.transfer_id                    = 1;
    header.frame_index_eot                = (1U << 31U) + 1U;
    specifier.data_specifier              = UDPARD_UDP_PORT;
    specifier.destination_route_specifier = 0b11101111'00'01000'0'0'000110011001100;
    specifier.source_route_specifier      = 0b11000000'10101000'00000000'00100111;
    // This is an empty payload, the last four bytes are CRC.
    REQUIRE(0 == accept(0, 100'000'000, header, specifier, {0, 0, 0, 0}));
    REQUIRE(subscription == nullptr);

    // Create a message subscription.
    UdpardRxSubscription sub_msg{};
    REQUIRE(1 == ins.rxSubscribe(UdpardTransferKindMessage, 0b0110011001100, 32, 2'000'000, sub_msg));  // New.
    REQUIRE(0 == ins.rxSubscribe(UdpardTransferKindMessage, 0b0110011001100, 16, 1'000'000, sub_msg));  // Replaced.
    REQUIRE(ins.getMessageSubs().at(0) == &sub_msg);
    REQUIRE(ins.getMessageSubs().at(0)->port_id == 0b0110011001100);
    REQUIRE(ins.getMessageSubs().at(0)->extent == 16);
    REQUIRE(ins.getMessageSubs().at(0)->transfer_id_timeout_usec == 1'000'000);
    REQUIRE(ensureAllNullptr(ins.getMessageSubs().at(0)->sessions));
    REQUIRE(ins.getResponseSubs().empty());
    REQUIRE(ins.getRequestSubs().empty());

    // Create a request subscription.
    UdpardRxSubscription sub_req{};
    REQUIRE(1 == ins.rxSubscribe(UdpardTransferKindRequest, 0b0000110011, 20, 3'000'000, sub_req));
    REQUIRE(ins.getMessageSubs().at(0) == &sub_msg);
    REQUIRE(ins.getResponseSubs().empty());
    REQUIRE(ins.getRequestSubs().at(0) == &sub_req);
    REQUIRE(ins.getRequestSubs().at(0)->port_id == 0b0000110011);
    REQUIRE(ins.getRequestSubs().at(0)->extent == 20);
    REQUIRE(ins.getRequestSubs().at(0)->transfer_id_timeout_usec == 3'000'000);
    REQUIRE(ensureAllNullptr(ins.getRequestSubs().at(0)->sessions));

    // Create a response subscription.
    UdpardRxSubscription sub_res{};
    REQUIRE(1 == ins.rxSubscribe(UdpardTransferKindResponse, 0b0000111100, 10, 100'000, sub_res));
    REQUIRE(ins.getMessageSubs().at(0) == &sub_msg);
    REQUIRE(ins.getResponseSubs().at(0) == &sub_res);
    REQUIRE(ins.getResponseSubs().at(0)->port_id == 0b0000111100);
    REQUIRE(ins.getResponseSubs().at(0)->extent == 10);
    REQUIRE(ins.getResponseSubs().at(0)->transfer_id_timeout_usec == 100'000);
    REQUIRE(ensureAllNullptr(ins.getResponseSubs().at(0)->sessions));
    REQUIRE(ins.getRequestSubs().at(0) == &sub_req);

    // Create a second response subscription. It will come before the one we added above due to lower port-ID.
    UdpardRxSubscription sub_res2{};
    REQUIRE(1 == ins.rxSubscribe(UdpardTransferKindResponse, 0b0000000000, 10, 1'000, sub_res2));
    REQUIRE(ins.getMessageSubs().at(0) == &sub_msg);
    REQUIRE(ins.getResponseSubs().at(0) == &sub_res2);
    REQUIRE(ins.getResponseSubs().at(0)->port_id == 0b0000000000);
    REQUIRE(ins.getResponseSubs().at(0)->extent == 10);
    REQUIRE(ins.getResponseSubs().at(0)->transfer_id_timeout_usec == 1'000);
    REQUIRE(ins.getResponseSubs().at(1) == &sub_res);  // The earlier one.
    REQUIRE(ins.getRequestSubs().at(0) == &sub_req);

    // Accepted message.
    subscription                          = nullptr;
    header.frame_index_eot                = (1U << 31U) + 1U;
    header.priority                       = 0b001;
    header.transfer_id                    = 0;
    header.data_specifier                 = 0b0000110011001100; // Subject ID = 3276
    specifier.data_specifier              = UDPARD_UDP_PORT;
    specifier.destination_route_specifier = 0b11101111'00'01000'0'0'000110011001100;
    specifier.source_route_specifier      = 0b11000000'10101000'00000000'00100111;
    // This is an empty payload, the last four bytes are CRC.
    REQUIRE(1 == accept(0, 100'000'001, header, specifier, {0, 0, 0, 0}));
    REQUIRE(subscription != nullptr);
    REQUIRE(subscription->port_id == 0b0110011001100);
    REQUIRE(transfer.timestamp_usec == 100'000'001);
    REQUIRE(transfer.metadata.priority == UdpardPriorityImmediate);
    REQUIRE(transfer.metadata.transfer_kind == UdpardTransferKindMessage);
    REQUIRE(transfer.metadata.port_id == 0b0110011001100);
    REQUIRE(transfer.metadata.remote_node_id == 0b0100111);
    REQUIRE(transfer.metadata.transfer_id == 0);
    REQUIRE(transfer.payload_size == 0); // Payload size should not include the CRC (0 byte payload + 4 byte CRC)
    REQUIRE(0 == std::memcmp(transfer.payload, "", 0));
    REQUIRE(ins.getAllocator().getNumAllocatedFragments() == 2);  // The SESSION and the PAYLOAD BUFFER.
    REQUIRE(ins.getAllocator().getTotalAllocatedAmount() == (sizeof(RxSession) + 16));
    REQUIRE(ins.getMessageSubs().at(0)->sessions[0b0100111] != nullptr);
    auto* msg_payload = transfer.payload;  // Will need it later.

    // Provide the space for an extra session and its payload.
    ins.getAllocator().setAllocationCeiling(sizeof(RxSession) * 2 + 16 + 20);

    // Dropped request because the local node does not have a node-ID.
    subscription                          = nullptr;
    header.frame_index_eot                = (1U << 31U) + 1U;
    header.priority                       = 0b011;
    header.transfer_id                    = 1;
    header.source_node_id                 = 0b00000000'00100111;
    header.destination_node_id            = 0b00000000'00011010;
    header.data_specifier                 = 0b1100000000110011;  // Service ID = 51
    specifier.data_specifier              = UDPARD_UDP_PORT;
    specifier.destination_route_specifier = 0b11101111'00'01000'1'00000000'00011010;
    specifier.source_route_specifier      = 0b11000000'10101000'00000000'00100111;
    REQUIRE(0 == accept(0, 100'000'002, header, specifier, {0, 0, 0, 0}));
    REQUIRE(subscription == nullptr);

    // Dropped request because the local node has a different node-ID.
    ins.setNodeID(0b0011010);
    subscription                          = nullptr;
    header.frame_index_eot                = (1U << 31U) + 1U;
    header.priority                       = 0b011;
    header.transfer_id                    = 1;
    header.source_node_id                 = 0b00000000'00100111;
    header.destination_node_id            = 0b00000000'00011011;
    specifier.data_specifier              = UDPARD_UDP_PORT;
    specifier.destination_route_specifier = 0b11101111'00'01000'1'00000000'00011011;
    specifier.source_route_specifier      = 0b11000000'10101000'00000000'00100111;
    REQUIRE(0 == accept(0, 100'000'002, header, specifier, {0, 0, 0, 0}));
    REQUIRE(subscription == nullptr);

    // Same request accepted now.
    subscription                          = nullptr;
    header.frame_index_eot                = (1U << 31U) + 1U;
    header.priority                       = 0b011;
    header.transfer_id                    = 4;
    header.destination_node_id            = 0b00000000'00011010;
    header.source_node_id                 = 0b00000000'00100101;
    specifier.data_specifier              = UDPARD_UDP_PORT;
    specifier.destination_route_specifier = 0b11101111'00'01000'1'00000000'00011010;
    specifier.source_route_specifier      = 0b11000000'10101000'00000000'00100101;
    REQUIRE(1 == accept(0, 100'000'002, header, specifier, {1, 2, 3, 30, 242, 48, 241}));
    REQUIRE(subscription != nullptr);
    REQUIRE(subscription->port_id == 0b0000110011);
    REQUIRE(transfer.timestamp_usec == 100'000'002);
    REQUIRE(transfer.metadata.priority == UdpardPriorityHigh);
    REQUIRE(transfer.metadata.transfer_kind == UdpardTransferKindRequest);
    REQUIRE(transfer.metadata.port_id == 0b0000110011);
    REQUIRE(transfer.metadata.remote_node_id == 0b0100101);
    REQUIRE(transfer.metadata.transfer_id == 4);
    REQUIRE(transfer.payload_size == 3); // Payload size should not include the CRC (3 byte payload + 4 byte CRC)
    REQUIRE(0 == std::memcmp(transfer.payload, "\x01\x02\x03\x1E\xF2\x30\xF1", 7));
    REQUIRE(ins.getAllocator().getNumAllocatedFragments() == 4);  // Two SESSIONS and two PAYLOAD BUFFERS.
    REQUIRE(ins.getAllocator().getTotalAllocatedAmount() == (2 * sizeof(RxSession) + 16 + 20));
    REQUIRE(ins.getRequestSubs().at(0)->sessions[0b0100101] != nullptr);

    // Response transfer not accepted because the local node has a different node-ID.
    // There is no dynamic memory available, but it doesn't matter because a rejection does not require allocation.
    subscription                          = nullptr;
    header.frame_index_eot                = (1U << 31U) + 1U;
    header.priority                       = 0b100;
    header.transfer_id                    = 1;
    header.source_node_id                 = 0b00000000'00011011;
    header.destination_node_id            = 0b00000000'00100111;
    header.data_specifier                 = 0b1000000000111100;  // Service ID = 60
    specifier.data_specifier              = UDPARD_UDP_PORT;
    specifier.source_route_specifier      = 0b11000000'10101000'00000000'00011011;
    specifier.destination_route_specifier = 0b11101111'00'01000'1'00000000'00100111;
    REQUIRE(0 == accept(0, 100'000'002, header, specifier, {10, 20, 30, 167, 39, 51, 218}));
    REQUIRE(subscription == nullptr);

    // Response transfer not accepted due to OOM -- can't allocate RX session.
    subscription                          = nullptr;
    header.frame_index_eot                = (1U << 31U) + 1U;
    header.priority                       = 0b100;
    header.transfer_id                    = 1;
    header.source_node_id                 = 0b00000000'00011011;
    header.destination_node_id            = 0b00000000'00011010;
    header.data_specifier                 = 0b1000000000111100;
    specifier.data_specifier              = UDPARD_UDP_PORT;
    specifier.source_route_specifier      = 0b11000000'10101000'00000000'00011011;
    specifier.destination_route_specifier = 0b11101111'00'01000'1'00000000'00011010;
    REQUIRE(-UDPARD_ERROR_OUT_OF_MEMORY == accept(0, 100'000'003, header, specifier, {5, 77, 71, 140, 103}));
    REQUIRE(subscription != nullptr);  // Subscription get assigned before error code
    REQUIRE(ins.getAllocator().getNumAllocatedFragments() == 4);
    REQUIRE(ins.getAllocator().getTotalAllocatedAmount() == (2 * sizeof(RxSession) + 16 + 20));

    // Response transfer not accepted due to OOM -- can't allocate the buffer (RX session is allocated OK).
    ins.getAllocator().setAllocationCeiling(3 * sizeof(RxSession) + 16 + 20);
    subscription                          = nullptr;
    header.frame_index_eot                = (1U << 31U) + 1U;
    header.priority                       = 0b100;
    header.transfer_id                    = 1;
    header.source_node_id                 = 0b00000000'00011011;
    header.destination_node_id            = 0b00000000'00011010;
    header.data_specifier                 = 0b1000000000111100;
    specifier.data_specifier              = UDPARD_UDP_PORT;
    specifier.source_route_specifier      = 0b11000000'10101000'00000000'00011011;
    specifier.destination_route_specifier = 0b11101111'00'01000'1'00000000'00011010;
    REQUIRE(-UDPARD_ERROR_OUT_OF_MEMORY == accept(0, 100'000'003, header, specifier, {5, 77, 71, 140, 103}));
    REQUIRE(subscription != nullptr);  // Subscription get assigned before error code
    REQUIRE(ins.getAllocator().getNumAllocatedFragments() == 5);
    REQUIRE(ins.getAllocator().getTotalAllocatedAmount() == (3 * sizeof(RxSession) + 16 + 20));

    // Destroy the message subscription and the buffer to free up memory.
    REQUIRE(1 == ins.rxUnsubscribe(UdpardTransferKindMessage, 0b0110011001100));
    REQUIRE(0 == ins.rxUnsubscribe(UdpardTransferKindMessage, 0b0110011001100));  // Repeat, nothing to do.
    REQUIRE(ins.getAllocator().getNumAllocatedFragments() == 4);
    REQUIRE(ins.getAllocator().getTotalAllocatedAmount() == (2 * sizeof(RxSession) + 16 + 20));
    ins.getAllocator().deallocate(msg_payload);
    REQUIRE(ins.getAllocator().getNumAllocatedFragments() == 3);
    REQUIRE(ins.getAllocator().getTotalAllocatedAmount() == (2 * sizeof(RxSession) + 20));

    // Same response accepted now. We have to keep incrementing the transfer-ID though because it's tracked.
    subscription                          = nullptr;
    header.frame_index_eot                = (1U << 31U) + 1U;
    header.priority                       = 0b100;
    header.transfer_id                    = 5;
    header.source_node_id                 = 0b00000000'00011011;
    header.destination_node_id            = 0b00000000'00011010;
    header.data_specifier                 = 0b1000000000111100;
    specifier.data_specifier              = UDPARD_UDP_PORT;
    specifier.source_route_specifier      = 0b11000000'10101000'00000000'00011011;
    specifier.destination_route_specifier = 0b11101111'00'01000'1'00000000'00011010;
    REQUIRE(1 == accept(0, 100'000'003, header, specifier, {5, 77, 71, 140, 103}));
    REQUIRE(subscription != nullptr);
    REQUIRE(subscription->port_id == 0b0000111100);
    REQUIRE(transfer.timestamp_usec == 100'000'003);
    REQUIRE(transfer.metadata.priority == UdpardPriorityNominal);
    REQUIRE(transfer.metadata.transfer_kind == UdpardTransferKindResponse);
    REQUIRE(transfer.metadata.port_id == 0b0000111100);
    REQUIRE(transfer.metadata.remote_node_id == 0b0011011);
    REQUIRE(transfer.metadata.transfer_id == 5);
    REQUIRE(transfer.payload_size == 1); // Payload size should not include the CRC (1 byte payload + 4 byte CRC)
    REQUIRE(0 == std::memcmp(transfer.payload, "\x05\x4D\x47\x8C\x67", 5));
    REQUIRE(ins.getAllocator().getNumAllocatedFragments() == 4);
    REQUIRE(ins.getAllocator().getTotalAllocatedAmount() == (2 * sizeof(RxSession) + 10 + 20));

    // Bad frames shall be rejected silently.
    /* May need a specicial setup to remove header from frame see private_rx test
    subscription                          = nullptr;
    header.frame_index_eot                = 0;
    header.priority                       = 0;
    header.transfer_id                    = 0;
    specifier.data_specifier              = 0;
    specifier.source_route_specifier      = 0;
    specifier.destination_route_specifier = 0;
    REQUIRE(0 == accept(0, 900'000'000, header, specifier, {}));
    REQUIRE(subscription == nullptr);
    */

    // Unsubscribe.
    REQUIRE(1 == ins.rxUnsubscribe(UdpardTransferKindRequest, 0b0000110011));
    REQUIRE(0 == ins.rxUnsubscribe(UdpardTransferKindRequest, 0b0000110011));
    REQUIRE(1 == ins.rxUnsubscribe(UdpardTransferKindResponse, 0b0000111100));
    REQUIRE(0 == ins.rxUnsubscribe(UdpardTransferKindResponse, 0b0000111100));
    REQUIRE(1 == ins.rxUnsubscribe(UdpardTransferKindResponse, 0b0000000000));
    REQUIRE(0 == ins.rxUnsubscribe(UdpardTransferKindResponse, 0b0000000000));
}

TEST_CASE("RxAnonymous")
{
    using helpers::Instance;
    using exposed::RxSession;
    Instance               ins;
    UdpardRxTransfer       transfer{};
    UdpardSessionSpecifier specifier{};
    UdpardFrameHeader      header{};
    UdpardRxSubscription*  subscription = nullptr;

    const auto accept = [&](const std::uint8_t               redundant_transport_index,
                            const UdpardMicrosecond          timestamp_usec,
                            UdpardFrameHeader                frame_header,
                            UdpardSessionSpecifier           session_specifier,
                            const std::vector<std::uint8_t>& payload) {
        auto *header_ptr      = reinterpret_cast<std::uint8_t*>(&frame_header);
        auto payload_storage = std::vector<std::uint8_t>(header_ptr, header_ptr + sizeof(frame_header));
        static std::vector<std::uint8_t> payload_buffer;
        payload_buffer = payload;
        payload_storage.insert(payload_storage.end(), payload_buffer.begin(), payload_buffer.end());
        UdpardFrame frame{};
        frame.payload_size = std::size(payload) + sizeof(frame_header);
        frame.payload      = payload_storage.data();
        return ins
            .rxAccept(timestamp_usec, frame, redundant_transport_index, session_specifier, transfer, &subscription);
    };

    ins.getAllocator().setAllocationCeiling(16);

    // Some initial header setup
    header.version     = 1;

    // A valid anonymous transfer for which there is no subscription.
    subscription                          = nullptr;
    header.priority                       = 0b001;
    header.source_node_id                 = 0b1111111111111111;
    header.destination_node_id            = 0b1111111111111111;
    header.data_specifier                 = 0b0000110011001100;
    header.transfer_id                    = 1;
    header.frame_index_eot                = (1U << 31U) + 1U;
    specifier.data_specifier              = UDPARD_UDP_PORT;
    specifier.destination_route_specifier = 0b11101111'00'01000'0'0'000110011001100;
    specifier.source_route_specifier      = 0b11000000'10101000'00000000'00000000;
    // REQUIRE(0 == exposed::txMakeMessageSessionSpecifier(0b0110011001100, 0b0, 0xc0a80000, &specifier));
    REQUIRE(0 == accept(0, 100'000'000, header, specifier, {0, 0, 0, 0}));
    REQUIRE(subscription == nullptr);

    // Create a message subscription.
    void* const          my_user_reference = &ins;
    UdpardRxSubscription sub_msg{};
    sub_msg.user_reference = my_user_reference;
    REQUIRE(1 == ins.rxSubscribe(UdpardTransferKindMessage, 0b0110011001100, 16, 2'000'000, sub_msg));  // New.

    // Accepted anonymous message.
    subscription                          = nullptr;
    header.frame_index_eot                = (1U << 31U) + 1U;
    header.priority                       = 0b001;
    header.source_node_id                 = 0b1111111111111111;
    header.destination_node_id            = 0b1111111111111111;
    header.data_specifier                 = 0b0000110011001100;
    header.transfer_id                    = 0;
    specifier.data_specifier              = UDPARD_UDP_PORT;
    specifier.destination_route_specifier = 0b11101111'00'01000'0'0'000110011001100;
    specifier.source_route_specifier      = 0b11000000'10101000'00000000'00000000;
    REQUIRE(1 == accept(0,
                        100'000'001,
                        header,
                        specifier,  //
                        {1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12, 13, 14, 15, 16, 17, 18, 19, 133, 210, 47, 197}));
    REQUIRE(subscription != nullptr);
    REQUIRE(subscription->port_id == 0b0110011001100);
    REQUIRE(subscription->user_reference == my_user_reference);
    REQUIRE(transfer.timestamp_usec == 100'000'001);
    REQUIRE(transfer.metadata.priority == UdpardPriorityImmediate);
    REQUIRE(transfer.metadata.transfer_kind == UdpardTransferKindMessage);
    REQUIRE(transfer.metadata.port_id == 0b0110011001100);
    REQUIRE(transfer.metadata.remote_node_id == UDPARD_NODE_ID_UNSET);
    REQUIRE(transfer.metadata.transfer_id == 0);
    REQUIRE(transfer.payload_size == 16);  // Truncated.
    REQUIRE(0 == std::memcmp(transfer.payload, "\x01\x02\x03\x04\x05\x06\x07\x08\x09\x0A\x0B\x0C\x0D\x0E\x0F\x10", 0));
    REQUIRE(ins.getAllocator().getNumAllocatedFragments() == 1);  // The PAYLOAD BUFFER only! No session for anons.
    REQUIRE(ins.getAllocator().getTotalAllocatedAmount() == 16);
    REQUIRE(ensureAllNullptr(ins.getMessageSubs().at(0)->sessions));  // No RX states!

    // Anonymous message not accepted because OOM. The transfer shall remain unmodified by the call, so we re-check it.
    header.frame_index_eot                = (1U << 31U) + 1U;
    header.priority                       = 0b001;
    header.transfer_id                    = 1;
    specifier.data_specifier              = UDPARD_UDP_PORT;
    specifier.destination_route_specifier = 0b11101111'00'01000'0'0'000110011001100;
    specifier.source_route_specifier      = 0b11000000'10101000'00000000'00000000;
    // REQUIRE(0 == exposed::txMakeMessageSessionSpecifier(0b0110011001100, 0b0, 0xc0a80000, &specifier));
    REQUIRE(-UDPARD_ERROR_OUT_OF_MEMORY == accept(0, 100'000'001, header, specifier, {3, 2, 1, 228, 208, 100, 95}));
    REQUIRE(subscription != nullptr);
    REQUIRE(subscription->port_id == 0b0110011001100);
    REQUIRE(transfer.timestamp_usec == 100'000'001);
    REQUIRE(transfer.metadata.priority == UdpardPriorityImmediate);
    REQUIRE(transfer.metadata.transfer_kind == UdpardTransferKindMessage);
    REQUIRE(transfer.metadata.port_id == 0b0110011001100);
    REQUIRE(transfer.metadata.remote_node_id == UDPARD_NODE_ID_UNSET);
    REQUIRE(transfer.metadata.transfer_id == 0);
    REQUIRE(transfer.payload_size == 16);  // Truncated.
    REQUIRE(0 == std::memcmp(transfer.payload, "\x01\x02\x03\x04\x05\x06\x07\x08\x09\x0A\x0B\x0C\x0D\x0E\x0F\x10", 0));
    REQUIRE(ins.getAllocator().getNumAllocatedFragments() == 1);  // The PAYLOAD BUFFER only! No session for anons.
    REQUIRE(ins.getAllocator().getTotalAllocatedAmount() == 16);
    REQUIRE(ensureAllNullptr(ins.getMessageSubs().at(0)->sessions));  // No RX states!

    // Release the memory.
    ins.getAllocator().deallocate(transfer.payload);
    REQUIRE(ins.getAllocator().getNumAllocatedFragments() == 0);
    REQUIRE(ins.getAllocator().getTotalAllocatedAmount() == 0);

    // Accepted anonymous message with small payload.
    subscription                          = nullptr;
    header.frame_index_eot                = (1U << 31U) + 1U;
    header.priority                       = 0b001;
    header.transfer_id                    = 0;
    specifier.data_specifier              = UDPARD_SUBJECT_ID_PORT;
    specifier.destination_route_specifier = 0b11101111'00'01000'0'0'000110011001100;
    specifier.source_route_specifier      = 0b11000000'10101000'00000000'00000000;
    REQUIRE(1 == accept(0, 100'000'001, header, specifier, {1, 2, 3, 4, 5, 6, 171, 251, 77, 79}));
    REQUIRE(subscription != nullptr);
    REQUIRE(subscription->port_id == 0b0110011001100);
    REQUIRE(transfer.timestamp_usec == 100'000'001);
    REQUIRE(transfer.metadata.priority == UdpardPriorityImmediate);
    REQUIRE(transfer.metadata.transfer_kind == UdpardTransferKindMessage);
    REQUIRE(transfer.metadata.port_id == 0b0110011001100);
    REQUIRE(transfer.metadata.remote_node_id == UDPARD_NODE_ID_UNSET);
    REQUIRE(transfer.metadata.transfer_id == 0);
    REQUIRE(transfer.payload_size == 10);  // NOT truncated.
    REQUIRE(0 == std::memcmp(transfer.payload, "\x01\x02\x03\x04\x05\x06\xAB\xFB\x4D\x4F", 0));
    REQUIRE(ins.getAllocator().getNumAllocatedFragments() == 1);      // The PAYLOAD BUFFER only! No session for anons.
    REQUIRE(ins.getAllocator().getTotalAllocatedAmount() == 10);       // Smaller allocation.
    REQUIRE(ensureAllNullptr(ins.getMessageSubs().at(0)->sessions));  // No RX states!

    // Version mismatch will be ignored
    header.version = 0;
    specifier.destination_route_specifier = 0b11101111'00'01000'0'0'000110011001100;
    specifier.source_route_specifier      = 0b11000000'10101000'00000000'00000000;
    REQUIRE(0 == accept(0, 100'000'001, header, specifier, {1, 2, 3, 4, 5, 6, 171, 251, 77, 79}));
}

TEST_CASE("RxSubscriptionErrors")
{
    using helpers::Instance;
    Instance             ins;
    UdpardRxSubscription sub{};

    const union
    {
        std::uint64_t      bits;
        UdpardTransferKind value;
    } kind{std::numeric_limits<std::uint64_t>::max()};

    REQUIRE(-UDPARD_ERROR_INVALID_ARGUMENT == udpardRxSubscribe(nullptr, UdpardTransferKindMessage, 0, 0, 0, &sub));
    REQUIRE(-UDPARD_ERROR_INVALID_ARGUMENT == udpardRxSubscribe(&ins.getInstance(), kind.value, 0, 0, 0, &sub));
    REQUIRE(-UDPARD_ERROR_INVALID_ARGUMENT ==
            udpardRxSubscribe(&ins.getInstance(), UdpardTransferKindMessage, 0, 0, 0, nullptr));

    REQUIRE(-UDPARD_ERROR_INVALID_ARGUMENT == udpardRxUnsubscribe(nullptr, UdpardTransferKindMessage, 0));
    REQUIRE(-UDPARD_ERROR_INVALID_ARGUMENT == udpardRxUnsubscribe(&ins.getInstance(), kind.value, 0));

    UdpardFrame            frame{};
    frame.payload_size = 1U;
    UdpardRxTransfer transfer{};
    REQUIRE(-UDPARD_ERROR_INVALID_ARGUMENT ==
            udpardRxAccept(&ins.getInstance(), 0, &frame, 0, &transfer, nullptr));
    REQUIRE(-UDPARD_ERROR_INVALID_ARGUMENT == udpardRxAccept(nullptr, 0, &frame, 0, &transfer, nullptr));
    REQUIRE(-UDPARD_ERROR_INVALID_ARGUMENT ==
            udpardRxAccept(&ins.getInstance(), 0, nullptr, 0, &transfer, nullptr));
    REQUIRE(-UDPARD_ERROR_INVALID_ARGUMENT ==
            udpardRxAccept(&ins.getInstance(), 0, &frame, 0, nullptr, nullptr));
    REQUIRE(-UDPARD_ERROR_INVALID_ARGUMENT == udpardRxAccept(nullptr, 0, nullptr, 0, nullptr, nullptr));
}
