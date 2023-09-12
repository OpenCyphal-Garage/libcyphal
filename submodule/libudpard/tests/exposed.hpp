// This software is distributed under the terms of the MIT License.
// Copyright (c) 2016 Cyphal Development Team.
/// Copyright 2022 Amazon.com, Inc. or its affiliates. All Rights Reserved.

#pragma once

#include "../libudpard/udpard.h"
#include <cstdarg>
#include <cstdint>
#include <limits>
#include <stdexcept>

/// Definitions that are not exposed by the library but that are needed for testing.
/// Please keep them in sync with the library by manually updating as necessary.
namespace exposed
{
using TransferCRC = std::uint32_t;

struct TxItem final : UdpardTxQueueItem
{
    [[nodiscard]] auto getPayloadByte(const std::size_t offset) const -> std::uint8_t
    {
        return reinterpret_cast<const std::uint8_t*>(frame.payload)[offset + sizeof(UdpardFrameHeader)];
    }

    [[nodiscard]] auto getFrameHeader() const
    {
        if (frame.payload_size < sizeof(UdpardFrameHeader))
        {
            // Can't use REQUIRE because it is not thread-safe.
            throw std::logic_error(
                "Can't get the frame header because the payload is not large enough to hold the header.");
        }
        return reinterpret_cast<const UdpardFrameHeader*>(frame.payload);
    }

    [[nodiscard]] auto isStartOfTransfer() const
    {
        return (getFrameHeader()->frame_index_eot & ((1U << 31U) - 1U)) == 1;
    }
    [[nodiscard]] auto isEndOfTransfer() const { return (getFrameHeader()->frame_index_eot >> 31U) == 1; }

    ~TxItem()                                 = default;
    TxItem(const TxItem&)                     = delete;
    TxItem(const TxItem&&)                    = delete;
    auto operator=(const TxItem&) -> TxItem&  = delete;
    auto operator=(const TxItem&&) -> TxItem& = delete;
};

struct RxSession
{
    UdpardMicrosecond transfer_timestamp_usec   = std::numeric_limits<std::uint64_t>::max();
    std::size_t       total_payload_size        = 0U;
    std::size_t       payload_size              = 0U;
    std::uint8_t*     payload                   = nullptr;
    TransferCRC       calculated_crc            = 0U;
    UdpardTransferID  transfer_id               = std::numeric_limits<std::uint8_t>::max();
    std::uint8_t      redundant_transport_index = std::numeric_limits<std::uint8_t>::max();
};

struct RxFrameModel
{
    UdpardMicrosecond   timestamp_usec      = std::numeric_limits<std::uint64_t>::max();
    UdpardHeaderVersion version             = 1U;
    UdpardPriority      priority            = UdpardPriorityOptional;
    UdpardTransferKind  transfer_kind       = UdpardTransferKindMessage;
    UdpardPortID        port_id             = std::numeric_limits<std::uint16_t>::max();
    UdpardNodeID        source_node_id      = UDPARD_NODE_ID_UNSET;
    UdpardNodeID        destination_node_id = UDPARD_NODE_ID_UNSET;
    UdpardTransferID    transfer_id         = std::numeric_limits<std::uint64_t>::max();
    bool                start_of_transfer   = false;
    bool                end_of_transfer     = false;
    std::size_t         payload_size        = 0U;
    const std::uint8_t* payload             = nullptr;
    uint32_t            frame_index         = 0U;
};

// Extern C effectively discards the outer namespaces.
extern "C" {

auto crcAdd(const std::uint32_t crc, const std::size_t size, const void* const bytes) -> std::uint32_t;

auto cyphalHeaderCrcAdd(const std::uint16_t crc, const std::size_t size, const void* const bytes) -> std::uint16_t;

auto crcValue(const std::uint32_t crc) -> std::uint32_t;

auto txMakeMessageSessionSpecifier(const UdpardPortID            subject_id,
                                   const UdpardNodeID            src_node_id,
                                   const UdpardIPv4Addr          local_node_addr,
                                   UdpardSessionSpecifier* const out_spec) -> std::uint32_t;

auto txMakeServiceSessionSpecifier(const UdpardPortID            service_id,
                                   const UdpardNodeID            src_node_id,
                                   const UdpardIPv4Addr          local_node_addr,
                                   UdpardSessionSpecifier* const out_spec) -> std::uint32_t;

auto adjustPresentationLayerMTU(const std::size_t mtu_bytes) -> std::size_t;

auto txMakeSessionSpecifier(const UdpardTransferMetadata* const tr,
                            const UdpardNodeID                  local_node_id,
                            const UdpardIPv4Addr                local_node_addr,
                            UdpardSessionSpecifier* const       spec) -> std::int32_t;

void txMakeFrameHeader(UdpardFrameHeader* const header,
                       const std::uint8_t       priority,
                       const UdpardTransferID   transfer_id,
                       const bool               end_of_transfer,
                       const std::uint32_t      frame_index);

auto txRoundFramePayloadSizeUp(const std::size_t x) -> std::size_t;

auto rxTryParseFrame(const UdpardMicrosecond             timestamp_usec,
                     UdpardFrame* const                  frame,
                     RxFrameModel* const                 out) -> bool;

auto rxSessionWritePayload(UdpardInstance* const ins,
                           RxSession* const      rxs,
                           const std::size_t     extent,
                           const std::size_t     payload_size,
                           const void* const     payload) -> std::int8_t;

void rxSessionRestart(UdpardInstance* const ins, RxSession* const rxs);

auto rxSessionUpdate(UdpardInstance* const     ins,
                     RxSession* const          rxs,
                     const RxFrameModel* const frame,
                     const std::uint8_t        redundant_transport_index,
                     const UdpardMicrosecond   transfer_id_timeout_usec,
                     const std::size_t         extent,
                     UdpardRxTransfer* const   out_transfer) -> std::int8_t;
}
}  // namespace exposed
