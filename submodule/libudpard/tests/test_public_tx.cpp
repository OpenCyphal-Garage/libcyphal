// This software is distributed under the terms of the MIT License.
// Copyright (c) 2016 OpenCyphal Development Team.
/// Copyright 2022 Amazon.com, Inc. or its affiliates. All Rights Reserved.

#include "exposed.hpp"
#include "helpers.hpp"
#include "catch/catch.hpp"
#include <cstring>

TEST_CASE("TxBasic0")
{
    using exposed::TxItem;

    helpers::Instance ins;
    helpers::TxQueue  que(200, UDPARD_MTU_UDP_IPV4);

    auto& alloc = ins.getAllocator();

    std::array<std::uint8_t, 1024> payload{};
    for (std::size_t i = 0; i < std::size(payload); i++)
    {
        payload.at(i) = static_cast<std::uint8_t>(i & 0xFFU);
    }

    REQUIRE(UDPARD_NODE_ID_UNSET == ins.getNodeID());
    ins.setNodeAddr(0xc0a80000);
    REQUIRE(0xc0a80000 == ins.getNodeAddr());
    REQUIRE(UDPARD_MTU_UDP_IPV4 == que.getMTU());
    REQUIRE(0 == que.getSize());
    REQUIRE(0 == alloc.getNumAllocatedFragments());

    alloc.setAllocationCeiling(4000);

    UdpardTransferMetadata meta{};

    // Single-frame with crc.
    meta.priority       = UdpardPriorityNominal;
    meta.transfer_kind  = UdpardTransferKindMessage;
    meta.port_id        = 321;
    meta.remote_node_id = UDPARD_NODE_ID_UNSET;
    meta.transfer_id    = 21;
    REQUIRE(1 == que.push(&ins.getInstance(), 1'000'000'000'000ULL, meta, 8, payload.data()));
    REQUIRE(1 == que.getSize());
    REQUIRE(1 == alloc.getNumAllocatedFragments());
    REQUIRE(10 < alloc.getTotalAllocatedAmount());
    REQUIRE(160 > alloc.getTotalAllocatedAmount());
    REQUIRE(que.peek()->tx_deadline_usec == 1'000'000'000'000ULL);
    REQUIRE(que.peek()->frame.payload_size == 36);  // 8 + 24 header + 4 CRC
    REQUIRE(que.peek()->getPayloadByte(0) == 0);    // Payload start. (starts after header)
    REQUIRE(que.peek()->getPayloadByte(1) == 1);
    REQUIRE(que.peek()->getPayloadByte(2) == 2);
    REQUIRE(que.peek()->getPayloadByte(3) == 3);
    REQUIRE(que.peek()->getPayloadByte(4) == 4);
    REQUIRE(que.peek()->getPayloadByte(5) == 5);
    REQUIRE(que.peek()->getPayloadByte(6) == 6);
    REQUIRE(que.peek()->getPayloadByte(7) == 7);  // Payload end.
    REQUIRE(que.peek()->isStartOfTransfer());     // Tail byte at the end.
    REQUIRE(que.peek()->isEndOfTransfer());

    meta.priority    = UdpardPriorityLow;
    meta.transfer_id = 22;
    ins.setNodeID(42);
    REQUIRE(1 == que.push(&ins.getInstance(), 1'000'000'000'100ULL, meta, 8, payload.data()));  // 8 bytes --> 2 frames
    REQUIRE(2 == que.getSize());
    REQUIRE(2 == alloc.getNumAllocatedFragments());
    REQUIRE(20 < alloc.getTotalAllocatedAmount());
    REQUIRE(400 > alloc.getTotalAllocatedAmount());

    // Check the TX queue.
    {
        const auto q = que.linearize();
        REQUIRE(2 == q.size());
        REQUIRE(q.at(0)->tx_deadline_usec == 1'000'000'000'000ULL);
        REQUIRE(q.at(0)->frame.payload_size == 36);
        REQUIRE(q.at(0)->isStartOfTransfer());
        REQUIRE(q.at(0)->isEndOfTransfer());
        //
        REQUIRE(q.at(1)->tx_deadline_usec == 1'000'000'000'100ULL);
        REQUIRE(q.at(1)->frame.payload_size == 36);
        REQUIRE(q.at(1)->isStartOfTransfer());
        REQUIRE(q.at(1)->isEndOfTransfer());
    }

    // Single-frame, OOM.
    alloc.setAllocationCeiling(alloc.getTotalAllocatedAmount());  // Seal up the heap at this level.
    meta.priority    = UdpardPriorityLow;
    meta.transfer_id = 23;
    REQUIRE(-UDPARD_ERROR_OUT_OF_MEMORY == que.push(&ins.getInstance(), 1'000'000'000'200ULL, meta, 1, payload.data()));
    REQUIRE(2 == que.getSize());
    REQUIRE(2 == alloc.getNumAllocatedFragments());

    alloc.setAllocationCeiling(alloc.getTotalAllocatedAmount() + sizeof(TxItem) + 10U);
    meta.priority    = UdpardPriorityHigh;
    meta.transfer_id = 24;
    REQUIRE(-UDPARD_ERROR_OUT_OF_MEMORY ==
            que.push(&ins.getInstance(), 1'000'000'000'300ULL, meta, 100, payload.data()));
    REQUIRE(2 == que.getSize());
    REQUIRE(2 == alloc.getNumAllocatedFragments());
    REQUIRE(20 < alloc.getTotalAllocatedAmount());
    REQUIRE(400 > alloc.getTotalAllocatedAmount());

    // Pop the queue.
    const UdpardTxQueueItem* ti = que.peek();
    REQUIRE(nullptr != ti);
    REQUIRE(ti->frame.payload_size == 36);
    REQUIRE(0 == std::memcmp(reinterpret_cast<const std::uint8_t*>(ti->frame.payload) + 24, payload.data(), 8));
    REQUIRE(ti->tx_deadline_usec == 1'000'000'000'000ULL);
    ti = que.peek();
    REQUIRE(nullptr != ti);  // Make sure we get the same frame again.
    REQUIRE(ti->frame.payload_size == 36);
    REQUIRE(0 == std::memcmp(reinterpret_cast<const std::uint8_t*>(ti->frame.payload) + 24, payload.data(), 8));
    REQUIRE(ti->tx_deadline_usec == 1'000'000'000'000ULL);
    ins.getAllocator().deallocate(que.pop(ti));
    REQUIRE(1 == que.getSize());
    REQUIRE(1 == alloc.getNumAllocatedFragments());
    ti = que.peek();
    REQUIRE(nullptr != ti);
    REQUIRE(ti->frame.payload_size == 36);
    REQUIRE(ti->tx_deadline_usec == 1'000'000'000'100ULL);
    ins.getAllocator().deallocate(que.pop(ti));
    REQUIRE(0 == que.getSize());
    REQUIRE(0 == alloc.getNumAllocatedFragments());
    ti = que.peek();
    REQUIRE(nullptr == ti);
    REQUIRE(nullptr == que.pop(nullptr));
    REQUIRE(0 == que.getSize());
    REQUIRE(0 == alloc.getNumAllocatedFragments());
    ti = que.peek();
    REQUIRE(nullptr == ti);

    alloc.setAllocationCeiling(1000);
    // Single-frame empty.
    meta.transfer_id = 28;
    REQUIRE(1 == que.push(&ins.getInstance(), 1'000'000'004'000ULL, meta, 0, nullptr));
    REQUIRE(1 == que.getSize());
    REQUIRE(1 == alloc.getNumAllocatedFragments());
    REQUIRE(140 > alloc.getTotalAllocatedAmount());
    REQUIRE(que.peek()->tx_deadline_usec == 1'000'000'004'000ULL);
    REQUIRE(que.peek()->frame.payload_size == 28);
    REQUIRE(que.peek()->isStartOfTransfer());
    REQUIRE(que.peek()->isEndOfTransfer());
    ti = que.peek();
    REQUIRE(nullptr != ti);
    REQUIRE(ti->frame.payload_size == 28);
    REQUIRE(ti->tx_deadline_usec == 1'000'000'004'000ULL);
    ins.getAllocator().deallocate(que.pop(ti));
    REQUIRE(0 == que.getSize());
    REQUIRE(0 == alloc.getNumAllocatedFragments());

    // Nothing left to peek at.
    ti = que.peek();
    REQUIRE(nullptr == ti);

    // Invalid transfer.
    meta.transfer_kind  = UdpardTransferKindMessage;
    meta.remote_node_id = 42;
    meta.transfer_id    = 123;
    REQUIRE(-UDPARD_ERROR_INVALID_ARGUMENT ==
            que.push(&ins.getInstance(), 1'000'000'005'000ULL, meta, 8, payload.data()));
    ti = que.peek();
    REQUIRE(nullptr == ti);
    // Error handling.
    REQUIRE(-UDPARD_ERROR_INVALID_ARGUMENT == udpardTxPush(nullptr, nullptr, 0, nullptr, 0, nullptr));
    REQUIRE(-UDPARD_ERROR_INVALID_ARGUMENT == udpardTxPush(nullptr, nullptr, 0, &meta, 0, nullptr));
    REQUIRE(-UDPARD_ERROR_INVALID_ARGUMENT == udpardTxPush(nullptr, &ins.getInstance(), 0, &meta, 0, nullptr));
    REQUIRE(-UDPARD_ERROR_INVALID_ARGUMENT ==
            udpardTxPush(&que.getInstance(), &ins.getInstance(), 0, nullptr, 0, nullptr));
    REQUIRE(-UDPARD_ERROR_INVALID_ARGUMENT == que.push(&ins.getInstance(), 1'000'000'006'000ULL, meta, 1, nullptr));

    REQUIRE(nullptr == udpardTxPeek(nullptr));
    REQUIRE(nullptr == udpardTxPop(nullptr, nullptr));             // No effect.
    REQUIRE(nullptr == udpardTxPop(&que.getInstance(), nullptr));  // No effect.
}

TEST_CASE("TxBasic1")
{
    using exposed::TxItem;
    using exposed::crcValue;

    helpers::Instance ins;
    helpers::TxQueue  que(4, UDPARD_MTU_UDP_IPV4);

    auto& alloc = ins.getAllocator();

    std::array<std::uint8_t, 1024> payload{};
    for (std::size_t i = 0; i < std::size(payload); i++)
    {
        payload.at(i) = static_cast<std::uint8_t>(i & 0xFFU);
    }

    REQUIRE(UDPARD_NODE_ID_UNSET == ins.getNodeID());
    ins.setNodeAddr(0xc0a80000);
    REQUIRE(0xc0a80000 == ins.getNodeAddr());
    REQUIRE(UDPARD_MTU_UDP_IPV4 == que.getMTU());
    REQUIRE(0 == que.getSize());
    REQUIRE(0 == alloc.getNumAllocatedFragments());

    alloc.setAllocationCeiling(4000);

    UdpardTransferMetadata meta{};

    // Single-frame with padding.
    meta.priority       = UdpardPriorityNominal;
    meta.transfer_kind  = UdpardTransferKindMessage;
    meta.port_id        = 321;
    meta.remote_node_id = UDPARD_NODE_ID_UNSET;
    meta.transfer_id    = 21;
    REQUIRE(1 == que.push(&ins.getInstance(), 1'000'000'000'000ULL, meta, 8, payload.data()));
    REQUIRE(1 == que.getSize());
    REQUIRE(1 == alloc.getNumAllocatedFragments());
    REQUIRE(10 < alloc.getTotalAllocatedAmount());
    REQUIRE(160 > alloc.getTotalAllocatedAmount());
    REQUIRE(que.peek()->tx_deadline_usec == 1'000'000'000'000ULL);
    REQUIRE(que.peek()->frame.payload_size == 36);  // 8 + 24 header + 4 CRC
    REQUIRE(que.peek()->getPayloadByte(0) == 0);    // Payload start. (starts after header)
    REQUIRE(que.peek()->getPayloadByte(1) == 1);
    REQUIRE(que.peek()->getPayloadByte(2) == 2);
REQUIRE(que.peek()->getPayloadByte(3) == 3);
    REQUIRE(que.peek()->getPayloadByte(4) == 4);
    REQUIRE(que.peek()->getPayloadByte(5) == 5);
    REQUIRE(que.peek()->getPayloadByte(6) == 6);
    REQUIRE(que.peek()->getPayloadByte(7) == 7);  // Payload end.
    REQUIRE(que.peek()->isStartOfTransfer());     // Tail byte at the end.
    REQUIRE(que.peek()->isEndOfTransfer());

    // Multi-frame transfer
    meta.priority    = UdpardPriorityLow;
    meta.transfer_id = 22;
    que.setMTU(64U);
    ins.setNodeID(42);
    REQUIRE(64U == que.getMTU());
    REQUIRE(2 == que.push(&ins.getInstance(), 1'000'000'000'100ULL, meta, 68, payload.data()));  // 68 bytes --> 2 frames
    REQUIRE(3 == que.getSize());
    REQUIRE(3 == alloc.getNumAllocatedFragments());
    REQUIRE(20 < alloc.getTotalAllocatedAmount());
    REQUIRE(600 > alloc.getTotalAllocatedAmount());

    // Check the TX queue.
    {
        const auto q = que.linearize();
        REQUIRE(3 == q.size());
        REQUIRE(q.at(0)->tx_deadline_usec == 1'000'000'000'000ULL);
        REQUIRE(q.at(0)->frame.payload_size == 36);
        REQUIRE(q.at(0)->isStartOfTransfer());
        REQUIRE(q.at(0)->isEndOfTransfer());

        REQUIRE(q.at(1)->tx_deadline_usec == 1'000'000'000'100ULL);
        REQUIRE(q.at(1)->frame.payload_size == 64); //mtu
        REQUIRE(q.at(1)->isStartOfTransfer());
        REQUIRE(!q.at(1)->isEndOfTransfer());

        REQUIRE(q.at(2)->tx_deadline_usec == 1'000'000'000'100ULL);
        REQUIRE(q.at(2)->frame.payload_size == 56); // mtu(= 28 data + 24 header + 4 crc)
        REQUIRE(!q.at(2)->isStartOfTransfer());
        REQUIRE(q.at(2)->isEndOfTransfer());
    }

    // Single-frame, OOM.
    alloc.setAllocationCeiling(alloc.getTotalAllocatedAmount());  // Seal up the heap at this level.
    meta.priority    = UdpardPriorityLow;
    meta.transfer_id = 23;
REQUIRE(-UDPARD_ERROR_OUT_OF_MEMORY == que.push(&ins.getInstance(), 1'000'000'000'200ULL, meta, 1, payload.data()));
    REQUIRE(3 == que.getSize());
    REQUIRE(3 == alloc.getNumAllocatedFragments());

    alloc.setAllocationCeiling(alloc.getTotalAllocatedAmount() + sizeof(TxItem) + 10U);
    meta.priority    = UdpardPriorityHigh;
    meta.transfer_id = 24;
    REQUIRE(-UDPARD_ERROR_OUT_OF_MEMORY ==
            que.push(&ins.getInstance(), 1'000'000'000'300ULL, meta, 100, payload.data()));
    REQUIRE(3 == que.getSize());
    REQUIRE(3 == alloc.getNumAllocatedFragments());
    REQUIRE(20 < alloc.getTotalAllocatedAmount());
    REQUIRE(1400 > alloc.getTotalAllocatedAmount());

    // Pop the queue.
    const UdpardTxQueueItem* ti = que.peek();
    REQUIRE(nullptr != ti);
    REQUIRE(ti->frame.payload_size == 36);
    REQUIRE(0 == std::memcmp(reinterpret_cast<const std::uint8_t*>(ti->frame.payload) + 24, payload.data(), 8));
    REQUIRE(ti->tx_deadline_usec == 1'000'000'000'000ULL);

    ti = que.peek(); // Make sure we get the same frame again.
    REQUIRE(nullptr != ti);
    REQUIRE(ti->frame.payload_size == 36);
    REQUIRE(0 == std::memcmp(reinterpret_cast<const std::uint8_t*>(ti->frame.payload) + 24, payload.data(), 8));
    REQUIRE(ti->tx_deadline_usec == 1'000'000'000'000ULL);
    ins.getAllocator().deallocate(que.pop(ti));
    REQUIRE(2 == que.getSize());
    REQUIRE(2 == alloc.getNumAllocatedFragments());

    ti = que.peek();
    REQUIRE(nullptr != ti);
    REQUIRE(ti->frame.payload_size == 64);
    REQUIRE(0 == std::memcmp(reinterpret_cast<const std::uint8_t*>(ti->frame.payload) + 24, payload.data(), 40)); // 64 - 24
    REQUIRE(ti->tx_deadline_usec == 1'000'000'000'100ULL);
    ins.getAllocator().deallocate(que.pop(ti));
    REQUIRE(1 == que.getSize());
    REQUIRE(1 == alloc.getNumAllocatedFragments());
    ti = que.peek();
    REQUIRE(nullptr != ti);
    REQUIRE(ti->frame.payload_size == 56);
    REQUIRE(ti->tx_deadline_usec == 1'000'000'000'100ULL);
    ins.getAllocator().deallocate(que.pop(ti));
    REQUIRE(0 == que.getSize());
    REQUIRE(0 == alloc.getNumAllocatedFragments());
    REQUIRE(nullptr == que.pop(nullptr));
    REQUIRE(0 == que.getSize());
    REQUIRE(0 == alloc.getNumAllocatedFragments());
    ti = que.peek();
    REQUIRE(nullptr == ti);

    alloc.setAllocationCeiling(1000);
    // Single-frame empty.
    meta.transfer_id = 28;
    REQUIRE(1 == que.push(&ins.getInstance(), 1'000'000'004'000ULL, meta, 0, nullptr));
    REQUIRE(1 == que.getSize());
    REQUIRE(1 == alloc.getNumAllocatedFragments());
    REQUIRE(140 > alloc.getTotalAllocatedAmount());
    REQUIRE(que.peek()->tx_deadline_usec == 1'000'000'004'000ULL);
    REQUIRE(que.peek()->frame.payload_size == 28);
    REQUIRE(que.peek()->isStartOfTransfer());
    REQUIRE(que.peek()->isEndOfTransfer());
    ti = que.peek();
    REQUIRE(nullptr != ti);
    REQUIRE(ti->frame.payload_size == 28);
    REQUIRE(ti->tx_deadline_usec == 1'000'000'004'000ULL);
    ins.getAllocator().deallocate(que.pop(ti));
    REQUIRE(0 == que.getSize());
    REQUIRE(0 == alloc.getNumAllocatedFragments());

    // Multi-frame, success, CRC in last frame
    // crc = pycyphal.transport.commons.crc.CRC32C.new(list(range(68))).value
    // (hex)crc = OxDBC9DD7B
    constexpr std::uint32_t CRC68 = 0xDBC9DD7BU;
    meta.priority    = UdpardPriorityLow;
    meta.transfer_id = 25;
    REQUIRE(2 == que.push(&ins.getInstance(), 1'000'000'001'000ULL, meta, 40 + 28, payload.data()));
    REQUIRE(2 == que.getSize());
    REQUIRE(2 == alloc.getNumAllocatedFragments());
    REQUIRE(40 < alloc.getTotalAllocatedAmount());
    REQUIRE(500 > alloc.getTotalAllocatedAmount());
    // Read the generated frame
    ti = que.peek();
    REQUIRE(nullptr != ti);
    REQUIRE(ti->frame.payload_size == 64);
    REQUIRE(0 == std::memcmp(reinterpret_cast<const std::uint8_t*>(ti->frame.payload) + 24, payload.data(), 40));
    ins.getAllocator().deallocate(que.pop(ti));
    REQUIRE(1 == que.getSize());
    REQUIRE(1 == alloc.getNumAllocatedFragments());
    ti = que.peek();
    REQUIRE(nullptr != ti);
    REQUIRE(ti->frame.payload_size == 56);
    REQUIRE(0 == std::memcmp(reinterpret_cast<const std::uint8_t*>(ti->frame.payload) + 24, payload.data() + 40U, 24));
    // last 4 bytes are crc
    REQUIRE(((uint8_t)(CRC68 & 0xFFU)) == reinterpret_cast<const std::uint8_t*>(ti->frame.payload)[52]);
    REQUIRE(((uint8_t)(CRC68 >> 8U)) == reinterpret_cast<const std::uint8_t*>(ti->frame.payload)[53]);
    REQUIRE(((uint8_t)(CRC68 >> 16U)) == reinterpret_cast<const std::uint8_t*>(ti->frame.payload)[54]);
    REQUIRE(((uint8_t)(CRC68 >> 24U)) == reinterpret_cast<const std::uint8_t*>(ti->frame.payload)[55]);
    ins.getAllocator().deallocate(que.pop(ti));
    REQUIRE(0 == que.getSize());
    REQUIRE(0 == alloc.getNumAllocatedFragments());
    // Nothing left to peek at.
    ti = que.peek();
    REQUIRE(nullptr == ti);


    // Multi-frame, success,
    // part of the CRC in one frame, rest of it in next frame.
    // crc = pycyphal.transport.commons.crc.CRC32C.new(list(range(79))).value
    // (hex)crc = OxADBA3FD
    constexpr std::uint32_t CRC79 = 0xADBA3FDU;
    meta.priority    = UdpardPriorityLow;
    meta.transfer_id = 26;
    REQUIRE(3 == que.push(&ins.getInstance(), 1'000'000'001'000ULL, meta, 79, payload.data()));
    REQUIRE(3 == que.getSize());
    REQUIRE(3 == alloc.getNumAllocatedFragments());
    REQUIRE(40 < alloc.getTotalAllocatedAmount());
    REQUIRE(500 > alloc.getTotalAllocatedAmount());
    // Read the generated frame
    ti = que.peek();
    REQUIRE(nullptr != ti);
    REQUIRE(ti->frame.payload_size == 64);
    REQUIRE(0 == std::memcmp(reinterpret_cast<const std::uint8_t*>(ti->frame.payload) + 24, payload.data(), 40));
    ins.getAllocator().deallocate(que.pop(ti));
    REQUIRE(2 == que.getSize());
    REQUIRE(2 == alloc.getNumAllocatedFragments());
    ti = que.peek();
    REQUIRE(nullptr != ti);
    REQUIRE(ti->frame.payload_size == 64);
    REQUIRE(0 == std::memcmp(reinterpret_cast<const std::uint8_t*>(ti->frame.payload) + 24, payload.data() + 40U, 39));
    // CRC first byte check
    REQUIRE(((uint8_t)(CRC79 & 0xFFU)) == reinterpret_cast<const std::uint8_t*>(ti->frame.payload)[63]);
    ins.getAllocator().deallocate(que.pop(ti));
    REQUIRE(1 == que.getSize());
    REQUIRE(1 == alloc.getNumAllocatedFragments());
    ti = que.peek();
    REQUIRE(nullptr != ti);
    REQUIRE(ti->frame.payload_size == 27); // 24 header + 3 crc
    // rest of the CRC check
    REQUIRE(((uint8_t)(CRC79 >> 8U)) == reinterpret_cast<const std::uint8_t*>(ti->frame.payload)[24]);
    REQUIRE(((uint8_t)(CRC79 >> 16U)) == reinterpret_cast<const std::uint8_t*>(ti->frame.payload)[25]);
    REQUIRE(((uint8_t)(CRC79 >> 24U)) == reinterpret_cast<const std::uint8_t*>(ti->frame.payload)[26]);
ins.getAllocator().deallocate(que.pop(ti));
    REQUIRE(0 == que.getSize());
    REQUIRE(0 == alloc.getNumAllocatedFragments());
    // Nothing left to peek at.
    ti = que.peek();
    REQUIRE(nullptr == ti);

    // Invalid transfer.
    meta.transfer_kind  = UdpardTransferKindMessage;
    meta.remote_node_id = 42;
    meta.transfer_id    = 123;
    REQUIRE(-UDPARD_ERROR_INVALID_ARGUMENT ==
            que.push(&ins.getInstance(), 1'000'000'005'000ULL, meta, 8, payload.data()));
    ti = que.peek();
    REQUIRE(nullptr == ti);

    // Error handling.
    REQUIRE(-UDPARD_ERROR_INVALID_ARGUMENT == udpardTxPush(nullptr, nullptr, 0, nullptr, 0, nullptr));
    REQUIRE(-UDPARD_ERROR_INVALID_ARGUMENT == udpardTxPush(nullptr, nullptr, 0, &meta, 0, nullptr));
    REQUIRE(-UDPARD_ERROR_INVALID_ARGUMENT == udpardTxPush(nullptr, &ins.getInstance(), 0, &meta, 0, nullptr));
    REQUIRE(-UDPARD_ERROR_INVALID_ARGUMENT ==
            udpardTxPush(&que.getInstance(), &ins.getInstance(), 0, nullptr, 0, nullptr));
    REQUIRE(-UDPARD_ERROR_INVALID_ARGUMENT == que.push(&ins.getInstance(), 1'000'000'006'000ULL, meta, 1, nullptr));
    REQUIRE(nullptr == udpardTxPeek(nullptr));
    REQUIRE(nullptr == udpardTxPop(nullptr, nullptr));             // No effect.
    REQUIRE(nullptr == udpardTxPop(&que.getInstance(), nullptr));  // No effect.
}
