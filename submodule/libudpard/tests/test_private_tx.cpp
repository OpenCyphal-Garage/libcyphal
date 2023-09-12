// This software is distributed under the terms of the MIT License.
// Copyright (c) 2016-2020 OpenCyphal Development Team.
/// Copyright 2022 Amazon.com, Inc. or its affiliates. All Rights Reserved.

#include "exposed.hpp"
#include "helpers.hpp"
#include "catch/catch.hpp"

constexpr uint16_t UDPARD_SUBJECT_ID_PORT = 16383U;
constexpr uint16_t UDPARD_UDP_PORT = 9382U;

TEST_CASE("SessionSpecifier")
{
    // Message
    UdpardSessionSpecifier specifier = {};
    REQUIRE(0 == exposed::txMakeMessageSessionSpecifier(0b0110011001100, 0b0100111, 0xc0a80000, &specifier));
    REQUIRE(UDPARD_UDP_PORT == specifier.data_specifier);
    REQUIRE(0b11101111'0'0'00000'0'0'0001100'11001100 == specifier.destination_route_specifier);
    REQUIRE(0b11000000'10101000'00000000'00100111 == specifier.source_route_specifier);
    // Service Request
    REQUIRE(0 ==
            exposed::txMakeServiceSessionSpecifier(0b0100110011, 0b1010101, 0xc0a80000, &specifier));
    REQUIRE(UDPARD_UDP_PORT == specifier.data_specifier);
    REQUIRE(0b11101111'0'0'00000'1'0'0000001'00110011 == specifier.destination_route_specifier);
    REQUIRE(0b11000000'10101000'00000000'01010101 == specifier.source_route_specifier);
    // Service Response
    REQUIRE(0 ==
            exposed::txMakeServiceSessionSpecifier(0b0100110011, 0b1010101, 0xc0a80000, &specifier));
    REQUIRE(UDPARD_UDP_PORT == specifier.data_specifier);
    REQUIRE(0b11101111'0'0'00000'1'0'0000001'00110011 == specifier.destination_route_specifier);
    REQUIRE(0b11000000'10101000'00000000'01010101 == specifier.source_route_specifier);
}

TEST_CASE("adjustPresentationLayerMTU") {}

TEST_CASE("txMakeSessionSpecifier")
{
    using exposed::txMakeSessionSpecifier;

    UdpardTransferMetadata meta{};
    UdpardSessionSpecifier specifier{};

    const auto mk_meta = [&](const UdpardPriority     priority,
                             const UdpardTransferKind kind,
                             const std::uint16_t      port_id,
                             const std::uint16_t       remote_node_id) {
        meta.priority       = priority;
        meta.transfer_kind  = kind;
        meta.port_id        = port_id;
        meta.remote_node_id = remote_node_id;
        return &meta;
    };

    union PriorityAlias
    {
        std::uint8_t   bits;
        UdpardPriority prio;
    };
    /*
    int32_t txMakeSessionSpecifier(const UdpardTransferMetadata* const tr,
                                   const UdpardNodeID                  local_node_id,
                                   const UdpardIPv4Addr                local_node_addr,
                                   const size_t                        presentation_layer_mtu,
                                   UdpardSessionSpecifier* const       spec);
    */

    // MESSAGE TRANSFERS
    REQUIRE(0 ==  // Regular message.
            txMakeSessionSpecifier(mk_meta(UdpardPriorityExceptional,
                                           UdpardTransferKindMessage,
                                           0b1001100110011,
                                           UDPARD_NODE_ID_UNSET),
                                   0b1010101,
                                   0xc0a80000,
                                   &specifier));

    REQUIRE(UDPARD_UDP_PORT == specifier.data_specifier);
    REQUIRE(0b11101111'0'0'00000'0'0'0010011'00110011 == specifier.destination_route_specifier);
    REQUIRE(0b11000000'10101000'00000000'01010101 == specifier.source_route_specifier);
    REQUIRE(-UDPARD_ERROR_INVALID_ARGUMENT ==  // Bad subject-ID.
            txMakeSessionSpecifier(mk_meta(UdpardPriorityExceptional,
                                           UdpardTransferKindMessage,
                                           0xFFFFU,
                                           UDPARD_NODE_ID_UNSET),
                                   0b1010101,
                                   0xc0a80000,
                                   &specifier));

    REQUIRE(-UDPARD_ERROR_INVALID_ARGUMENT ==  // Bad priority.
            txMakeSessionSpecifier(mk_meta(PriorityAlias{123}.prio,
                                           UdpardTransferKindMessage,
                                           0b1001100110011,
                                           UDPARD_NODE_ID_UNSET),
                                   0b1010101,
                                   0xc0a80000,
                                   &specifier));

    // SERVICE TRANSFERS
    REQUIRE(
        0 ==  // Request.
        txMakeSessionSpecifier(mk_meta(UdpardPriorityExceptional, UdpardTransferKindRequest, 0b0100110011, 0b0101010),
                               0b1010101,
                               0xc0a80000,
                               &specifier));
    REQUIRE(
        0 ==  // Response.
        txMakeSessionSpecifier(mk_meta(UdpardPriorityExceptional, UdpardTransferKindResponse, 0b0100110011, 0b0101010),
                               0b1010101,
                               0xc0a80000,
                               &specifier));
    REQUIRE(
        -UDPARD_ERROR_INVALID_ARGUMENT ==  // Anonymous source service transfers not permitted.
        txMakeSessionSpecifier(mk_meta(UdpardPriorityExceptional, UdpardTransferKindRequest, 0b0100110011, 0b0101010),
                               UDPARD_NODE_ID_UNSET,
                               0xc0a80000,
                               &specifier));

    REQUIRE(-UDPARD_ERROR_INVALID_ARGUMENT ==  // Anonymous destination service transfers not permitted.
            txMakeSessionSpecifier(mk_meta(UdpardPriorityExceptional,
                                           UdpardTransferKindResponse,
                                           0b0100110011,
                                           UDPARD_NODE_ID_UNSET),
                                   0b1010101,
                                   0xc0a80000,
                                   &specifier));

    REQUIRE(-UDPARD_ERROR_INVALID_ARGUMENT ==  // Bad service-ID.
            txMakeSessionSpecifier(mk_meta(UdpardPriorityExceptional, UdpardTransferKindResponse, 0xFFFFU, 0b0101010),
                                   0b1010101,
                                   0xc0a80000,
                                   &specifier));
    REQUIRE(
        -UDPARD_ERROR_INVALID_ARGUMENT ==  // Bad priority.
        txMakeSessionSpecifier(mk_meta(PriorityAlias{123}.prio, UdpardTransferKindResponse, 0b0100110011, 0b0101010),
                               0b1010101,
                               0xc0a80000,
                               &specifier));
}

TEST_CASE("txMakeTailByte") {}

TEST_CASE("txRoundFramePayloadSizeUp") {}
