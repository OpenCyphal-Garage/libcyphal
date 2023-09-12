/// This library is based heavily on libcanard
/// This software is distributed under the terms of the MIT License.
/// Copyright (c) 2016 OpenCyphal.
/// Author: Pavel Kirienko <pavel@opencyphal.org>
/// Copyright 2022 Amazon.com, Inc. or its affiliates. All Rights Reserved.

#include "udpard.h"
#include "cavl.h"
#include <string.h>

// --------------------------------------------- BUILD CONFIGURATION ---------------------------------------------

/// Define this macro to include build configuration header.
/// Usage example with CMake: "-DUDPARD_CONFIG_HEADER=\"${CMAKE_CURRENT_SOURCE_DIR}/my_udpard_config.h\""
#ifdef UDPARD_CONFIG_HEADER
#    include UDPARD_CONFIG_HEADER
#endif

/// By default, this macro resolves to the standard assert(). The user can redefine this if necessary.
/// To disable assertion checks completely, make it expand into `(void)(0)`.
#ifndef UDPARD_ASSERT
// Intentional violation of MISRA: inclusion not at the top of the file to eliminate unnecessary dependency on assert.h.
#    include <assert.h>  // NOSONAR
// Intentional violation of MISRA: assertion macro cannot be replaced with a function definition.
#    define UDPARD_ASSERT(x) assert(x)  // NOSONAR
#endif

/// Define UDPARD_CRC_TABLE=0 to use slow but ROM-efficient transfer-CRC computation algorithm.
/// Doing so is expected to save ca. 500 bytes of ROM and increase the cost of RX/TX transfer processing by ~half.
#ifndef UDPARD_CRC_TABLE
#    define UDPARD_CRC_TABLE 1
#endif

/// This macro is needed for testing and for library development.
#ifndef UDPARD_PRIVATE
#    define UDPARD_PRIVATE static inline
#endif

#if !defined(__STDC_VERSION__) || (__STDC_VERSION__ < 199901L)
#    error "Unsupported language: ISO C99 or a newer version is required."
#endif

// --------------------------------------------- COMMON DEFINITIONS ---------------------------------------------

static const uint8_t BITS_PER_BYTE = 8U;
static const uint8_t BYTE_MAX = 0xFFU;

static const uint8_t UDPARD_END_OF_TRANSFER_OFFSET = 31U;
static const uint32_t UDPARD_MAX_FRAME_INDEX = 0x7FFFFFFFU; // ((1U << UDPARD_END_OF_TRANSFER_OFFSET) - 1U);
static const uint16_t UDPARD_NODE_ID_MASK = 65535U;  /// 0xFFFF

/*
             fixed   Cyphal/UDP
           (9 bits)   Ipv4 Addr
                      Version                    Destination Node ID
      ________________(1 bit)       SNM   _______________________________
     /                 \ |           |   /                               \
     1 1 1 0 1 1 1 1 . 0 c d d d d d m . 0 s s s s s s s . s s s s s s s s
     \_____/ \______/      \_______/     | \_____________________________/
   (4 bits)  (4 bits)        Subnet      |      (15 bits) subject-ID
     IPv4      Scope        Reserved    Reserved
   multicast             \_______________________________________________/
    prefix                          (23 bits)
                              collision-free multicast
                                addressing limit of
                               Ethernet MAC for IPv4
*/

/// The multicast message transfer IP address node ID is formed of 1 reserved 0 bits and 15 bits for a subject id.
static const uint16_t UDPARD_SUBJECT_ID_MASK = 0x7FFFU;
static const uint32_t UDPARD_SUBNET_MASK = 0x3E0000U; // (31U << UDPARD_SUBNET_OFFSET (17));
static const uint8_t UDPARD_TRANSMIT_SUBNET_VALUE = 0U;
static const uint16_t UDPARD_RESERVED_1BIT_MASK = 0x8000U; // (1U << UDPARD_RESERVED_1BIT_OFFSET (15));
static const uint32_t UDPARD_SERVICE_NOT_MESSAGE_MASK = 0x10000U; // (1U << UDPARD_SERVICE_NOT_MESSAGE_OFFSET (16));
static const uint32_t UDPARD_MULTICAST_PREFIX = 0xEF000000U; // (478U << UDPARD_MULTICAST_OFFSET (23))

/* The 16 bit data specifier in the Cyphal header consists of
SNM + 15 bit Subject-ID (Message)
SNM + IRNR + Service-ID (Service Request/Response)

SNM - Service, Not Message
IRNR - Is Request, Not Response
*/
static const uint16_t UDPARD_SERVICE_NOT_MESSAGE_DATA_SPECIFIER_OFFSET = 15U;
static const uint16_t UDPARD_IRNR_DATA_SPECIFIER_OFFSET = 14U;
static const uint16_t UDPARD_SERVICE_ID_MASK = 0x3FFFU;
static const uint16_t UDPARD_DATA_SPECIFIER_MESSAGE_MASK = 0x7FFFU; // SNM (0) + SubjectID
static const uint16_t UDPARD_DATA_SPECIFIER_SERVICE_RESPONSE = 0x8000U; // (2U << UDPARD_IRNR_DATA_SPECIFIER_OFFSET)  // Set SNM in Cyphal data specifier - SNM (1) + IRNR (0) + ServiceID
static const uint16_t UDPARD_DATA_SPECIFIER_SERVICE_REQUEST = 0xC000U; // (3U << UDPARD_IRNR_DATA_SPECIFIER_OFFSET)   // Set SNM and IRNR in Cyphal data specifier - SNM (1) + IRNR (1) + ServiceID

static const uint16_t UDPARD_UDP_PORT = 9382U;

/// Used for inserting new items into AVL trees.
UDPARD_PRIVATE UdpardTreeNode* avlTrivialFactory(void* const user_reference)
{
    return (UdpardTreeNode*) user_reference;
}

/// --------------------------------------------- TRANSFER CRC ---------------------------------------------

typedef uint32_t TransferCRC;

// Those crc values come from https://crc32c.machinezoo.com/
static const TransferCRC CRC_INITIAL = 0xFFFFFFFFU;
static const TransferCRC CRC_RESIDUE = 0xB798B438U;
static const TransferCRC CRC_XOR = 0xFFFFFFFFU;
static const size_t CRC_SIZE_BYTES = 4U;

static const uint8_t CRC_BYTE_MASK = 0xFFU;

static const uint32_t CRCTable[256] =
    {0x00000000, 0xf26b8303, 0xe13b70f7, 0x1350f3f4, 0xc79a971f, 0x35f1141c, 0x26a1e7e8, 0xd4ca64eb, 0x8ad958cf,
     0x78b2dbcc, 0x6be22838, 0x9989ab3b, 0x4d43cfd0, 0xbf284cd3, 0xac78bf27, 0x5e133c24, 0x105ec76f, 0xe235446c,
     0xf165b798, 0x030e349b, 0xd7c45070, 0x25afd373, 0x36ff2087, 0xc494a384, 0x9a879fa0, 0x68ec1ca3, 0x7bbcef57,
     0x89d76c54, 0x5d1d08bf, 0xaf768bbc, 0xbc267848, 0x4e4dfb4b, 0x20bd8ede, 0xd2d60ddd, 0xc186fe29, 0x33ed7d2a,
     0xe72719c1, 0x154c9ac2, 0x061c6936, 0xf477ea35, 0xaa64d611, 0x580f5512, 0x4b5fa6e6, 0xb93425e5, 0x6dfe410e,
     0x9f95c20d, 0x8cc531f9, 0x7eaeb2fa, 0x30e349b1, 0xc288cab2, 0xd1d83946, 0x23b3ba45, 0xf779deae, 0x05125dad,
     0x1642ae59, 0xe4292d5a, 0xba3a117e, 0x4851927d, 0x5b016189, 0xa96ae28a, 0x7da08661, 0x8fcb0562, 0x9c9bf696,
     0x6ef07595, 0x417b1dbc, 0xb3109ebf, 0xa0406d4b, 0x522bee48, 0x86e18aa3, 0x748a09a0, 0x67dafa54, 0x95b17957,
     0xcba24573, 0x39c9c670, 0x2a993584, 0xd8f2b687, 0x0c38d26c, 0xfe53516f, 0xed03a29b, 0x1f682198, 0x5125dad3,
     0xa34e59d0, 0xb01eaa24, 0x42752927, 0x96bf4dcc, 0x64d4cecf, 0x77843d3b, 0x85efbe38, 0xdbfc821c, 0x2997011f,
     0x3ac7f2eb, 0xc8ac71e8, 0x1c661503, 0xee0d9600, 0xfd5d65f4, 0x0f36e6f7, 0x61c69362, 0x93ad1061, 0x80fde395,
     0x72966096, 0xa65c047d, 0x5437877e, 0x4767748a, 0xb50cf789, 0xeb1fcbad, 0x197448ae, 0x0a24bb5a, 0xf84f3859,
     0x2c855cb2, 0xdeeedfb1, 0xcdbe2c45, 0x3fd5af46, 0x7198540d, 0x83f3d70e, 0x90a324fa, 0x62c8a7f9, 0xb602c312,
     0x44694011, 0x5739b3e5, 0xa55230e6, 0xfb410cc2, 0x092a8fc1, 0x1a7a7c35, 0xe811ff36, 0x3cdb9bdd, 0xceb018de,
     0xdde0eb2a, 0x2f8b6829, 0x82f63b78, 0x709db87b, 0x63cd4b8f, 0x91a6c88c, 0x456cac67, 0xb7072f64, 0xa457dc90,
     0x563c5f93, 0x082f63b7, 0xfa44e0b4, 0xe9141340, 0x1b7f9043, 0xcfb5f4a8, 0x3dde77ab, 0x2e8e845f, 0xdce5075c,
     0x92a8fc17, 0x60c37f14, 0x73938ce0, 0x81f80fe3, 0x55326b08, 0xa759e80b, 0xb4091bff, 0x466298fc, 0x1871a4d8,
     0xea1a27db, 0xf94ad42f, 0x0b21572c, 0xdfeb33c7, 0x2d80b0c4, 0x3ed04330, 0xccbbc033, 0xa24bb5a6, 0x502036a5,
     0x4370c551, 0xb11b4652, 0x65d122b9, 0x97baa1ba, 0x84ea524e, 0x7681d14d, 0x2892ed69, 0xdaf96e6a, 0xc9a99d9e,
     0x3bc21e9d, 0xef087a76, 0x1d63f975, 0x0e330a81, 0xfc588982, 0xb21572c9, 0x407ef1ca, 0x532e023e, 0xa145813d,
     0x758fe5d6, 0x87e466d5, 0x94b49521, 0x66df1622, 0x38cc2a06, 0xcaa7a905, 0xd9f75af1, 0x2b9cd9f2, 0xff56bd19,
     0x0d3d3e1a, 0x1e6dcdee, 0xec064eed, 0xc38d26c4, 0x31e6a5c7, 0x22b65633, 0xd0ddd530, 0x0417b1db, 0xf67c32d8,
     0xe52cc12c, 0x1747422f, 0x49547e0b, 0xbb3ffd08, 0xa86f0efc, 0x5a048dff, 0x8ecee914, 0x7ca56a17, 0x6ff599e3,
     0x9d9e1ae0, 0xd3d3e1ab, 0x21b862a8, 0x32e8915c, 0xc083125f, 0x144976b4, 0xe622f5b7, 0xf5720643, 0x07198540,
     0x590ab964, 0xab613a67, 0xb831c993, 0x4a5a4a90, 0x9e902e7b, 0x6cfbad78, 0x7fab5e8c, 0x8dc0dd8f, 0xe330a81a,
     0x115b2b19, 0x020bd8ed, 0xf0605bee, 0x24aa3f05, 0xd6c1bc06, 0xc5914ff2, 0x37faccf1, 0x69e9f0d5, 0x9b8273d6,
     0x88d28022, 0x7ab90321, 0xae7367ca, 0x5c18e4c9, 0x4f48173d, 0xbd23943e, 0xf36e6f75, 0x0105ec76, 0x12551f82,
     0xe03e9c81, 0x34f4f86a, 0xc69f7b69, 0xd5cf889d, 0x27a40b9e, 0x79b737ba, 0x8bdcb4b9, 0x988c474d, 0x6ae7c44e,
     0xbe2da0a5, 0x4c4623a6, 0x5f16d052, 0xad7d5351};

UDPARD_PRIVATE TransferCRC crcAddByte(const TransferCRC crc, const uint8_t byte)
{
    return CRCTable[(uint32_t) ( byte ^ (crc & BYTE_MAX))] ^ (crc >> BITS_PER_BYTE);
}

UDPARD_PRIVATE TransferCRC crcAdd(const TransferCRC crc, const size_t size, const void* const data)
{
    UDPARD_ASSERT((data != NULL) || (size == 0U));
    TransferCRC    out = crc;
    const uint8_t* p   = (const uint8_t*) data;
    for (size_t i = 0; i < size; i++)
    {
        out = crcAddByte(out, *p);
        ++p;
    }
    return out;
}

UDPARD_PRIVATE TransferCRC crcValue(const TransferCRC crc)
{
    return (uint32_t) (crc ^ CRC_XOR);
}

/// --------------------------------------------- CYPHAL HEADER CRC ---------------------------------------------

typedef uint16_t CyphalHeaderCRC;

// Based on CRC-16-CCITT-FALSE Function
static const uint16_t CYPHAL_HEADER_CRC_INITIAL = 0xFFFFU;
static const size_t CYPHAL_HEADER_CRC_SIZE_BYTES = 2U;

UDPARD_PRIVATE CyphalHeaderCRC cyphalHeaderCrcAddByte(const CyphalHeaderCRC crc, const uint8_t byte)
{
    // Based on CRC-16-CCITT-FALSE Function
    static const CyphalHeaderCRC Top  = 0x8000U;
    static const CyphalHeaderCRC Poly = 0x1021U;
    CyphalHeaderCRC              out  = crc ^ (uint16_t) ((uint16_t) (byte) << BITS_PER_BYTE);
    // Do not fold this into a loop because a size-optimizing compiler won't unroll it degrading the performance.
    out = (uint16_t) ((uint16_t) (out << 1U) ^ (((out & Top) != 0U) ? Poly : 0U));
    out = (uint16_t) ((uint16_t) (out << 1U) ^ (((out & Top) != 0U) ? Poly : 0U));
    out = (uint16_t) ((uint16_t) (out << 1U) ^ (((out & Top) != 0U) ? Poly : 0U));
    out = (uint16_t) ((uint16_t) (out << 1U) ^ (((out & Top) != 0U) ? Poly : 0U));
    out = (uint16_t) ((uint16_t) (out << 1U) ^ (((out & Top) != 0U) ? Poly : 0U));
    out = (uint16_t) ((uint16_t) (out << 1U) ^ (((out & Top) != 0U) ? Poly : 0U));
    out = (uint16_t) ((uint16_t) (out << 1U) ^ (((out & Top) != 0U) ? Poly : 0U));
    out = (uint16_t) ((uint16_t) (out << 1U) ^ (((out & Top) != 0U) ? Poly : 0U));
    return out;
}

UDPARD_PRIVATE CyphalHeaderCRC cyphalHeaderCrcAdd(const CyphalHeaderCRC crc, const size_t size, const void* const header)
{
    UDPARD_ASSERT(header != NULL);
    CyphalHeaderCRC    out = crc;
    const uint8_t* p   = (const uint8_t*) header;
    for (size_t i = 0; i < size; i++)
    {
        out = cyphalHeaderCrcAddByte(out, *p);
        ++p;
    }
    return out;
}

// --------------------------------------------- TRANSMISSION ---------------------------------------------

/// This is a subclass of UdpardTxQueueItem. A pointer to this type can be cast to UdpardTxQueueItem safely.
/// This is standard-compliant. The paragraph 6.7.2.1.15 says:
///     A pointer to a structure object, suitably converted, points to its initial member (or if that member is a
///     bit-field, then to the unit in which it resides), and vice versa. There may be unnamed padding within a
///     structure object, but not at its beginning.
typedef struct TxItem
{
    UdpardTxQueueItem base;
    uint8_t           payload_buffer[UDPARD_MTU_MAX];
} TxItem;

/// Chain of TX frames prepared for insertion into a TX queue.
typedef struct
{
    TxItem* head;
    TxItem* tail;
    size_t  size;
} TxChain;

/// TODO - Determine what is needed for a Message Sessions Specifier
UDPARD_PRIVATE int32_t txMakeMessageSessionSpecifier(const UdpardPortID            subject_id,
                                                     const UdpardNodeID            src_node_id,
                                                     const UdpardIPv4Addr          local_node_addr,
                                                     UdpardSessionSpecifier* const out_spec)
{
    UDPARD_ASSERT(subject_id <= UDPARD_SUBJECT_ID_MAX);
    /// Just the local ip address + source node id
    out_spec->source_route_specifier =
        (local_node_addr & ~(UdpardIPv4Addr) UDPARD_NODE_ID_MASK) |
        (UdpardIPv4Addr) src_node_id;
    out_spec->destination_route_specifier =
        ((UDPARD_TRANSMIT_SUBNET_VALUE & (UdpardIPv4Addr) UDPARD_SUBNET_MASK) |
        (UdpardIPv4Addr) UDPARD_MULTICAST_PREFIX |
        ((UdpardIPv4Addr) UDPARD_SUBJECT_ID_MASK & (UdpardIPv4Addr) subject_id)) &
        ~(UdpardIPv4Addr) UDPARD_SERVICE_NOT_MESSAGE_MASK &
        ~(UdpardIPv4Addr) UDPARD_RESERVED_1BIT_MASK;
    out_spec->data_specifier = (UdpardUdpPortID) UDPARD_UDP_PORT;
    return UDPARD_SUCCESS;
}

UDPARD_PRIVATE int32_t txMakeServiceSessionSpecifier(const UdpardPortID            service_id,
                                                     const UdpardNodeID            src_node_id,
                                                     const UdpardIPv4Addr          local_node_addr,
                                                     UdpardSessionSpecifier* const out_spec)
{
    UDPARD_ASSERT(service_id < UDPARD_SERVICE_ID_MAX);
    /// Just the local ip address + source node id
    out_spec->source_route_specifier =
        (local_node_addr & ~(UdpardIPv4Addr) UDPARD_NODE_ID_MASK) |
        (UdpardIPv4Addr) src_node_id;
    out_spec->destination_route_specifier =
        ((UDPARD_TRANSMIT_SUBNET_VALUE & (UdpardIPv4Addr) UDPARD_SUBNET_MASK) |
        (UdpardIPv4Addr) UDPARD_MULTICAST_PREFIX |
        ((UdpardIPv4Addr) UDPARD_NODE_ID_MASK & (UdpardIPv4Addr) service_id)) |
        (UdpardIPv4Addr) UDPARD_SERVICE_NOT_MESSAGE_MASK;
    out_spec->data_specifier = (UdpardUdpPortID) UDPARD_UDP_PORT;
    return UDPARD_SUCCESS;
}

// This may need to be adjusted...
UDPARD_PRIVATE size_t adjustPresentationLayerMTU(const size_t mtu_bytes)
{
    if (mtu_bytes >= UDPARD_MTU_UDP_IPV4)
    {
        return UDPARD_MTU_UDP_IPV4;
    }
    return mtu_bytes;
}

UDPARD_PRIVATE int32_t txMakeSessionSpecifier(const UdpardTransferMetadata* const tr,
                                              const UdpardNodeID                  local_node_id,
                                              const UdpardIPv4Addr                local_node_addr,
                                              UdpardSessionSpecifier* const       spec)
{
    UDPARD_ASSERT(tr != NULL);
    int32_t out = -UDPARD_ERROR_INVALID_ARGUMENT;
    if ((tr->transfer_kind == UdpardTransferKindMessage) && (UDPARD_NODE_ID_UNSET == tr->remote_node_id) &&
        (tr->port_id <= UDPARD_SUBJECT_ID_MAX))
    {
        if ((local_node_id == UDPARD_NODE_ID_UNSET) || (local_node_id <= UDPARD_NODE_ID_MAX)) {

            out = txMakeMessageSessionSpecifier(tr->port_id, local_node_id, local_node_addr, spec);
            UDPARD_ASSERT(out >= 0);
        } else {
            out = -UDPARD_ERROR_INVALID_ARGUMENT; // Node must be at least 1 less than the unset id
        }
    }
    else if (((tr->transfer_kind == UdpardTransferKindRequest) || (tr->transfer_kind == UdpardTransferKindResponse)) &&
             (tr->port_id < UDPARD_SERVICE_ID_MAX) && (tr->remote_node_id != UDPARD_NODE_ID_UNSET))
    {
        if (local_node_id != UDPARD_NODE_ID_UNSET)
        {
            out = txMakeServiceSessionSpecifier(tr->port_id,
                                                local_node_id,
                                                local_node_addr,
                                                spec);
            UDPARD_ASSERT(out >= 0);
        }
        else
        {
            out = -UDPARD_ERROR_INVALID_ARGUMENT;  // Anonymous service transfers are not allowed.
        }
    }
    else
    {
        out = -UDPARD_ERROR_INVALID_ARGUMENT;
    }

    if (out >= 0)
    {
        const uint32_t prio = (uint32_t) tr->priority;
        if (prio > UDPARD_PRIORITY_MAX)
        {
            out = -UDPARD_ERROR_INVALID_ARGUMENT;  // Priority can't be greater than max value
        }
    }
    return out;
}

UDPARD_PRIVATE void txMakeFrameHeader(UdpardFrameHeader* const header,
                                      const UdpardNodeID       src_node_id,
                                      const UdpardNodeID       dst_node_id,
                                      const UdpardPortID       port_id,
                                      const UdpardTransferKind transfer_kind,
                                      const UdpardPriority     priority,
                                      const UdpardTransferID   transfer_id,
                                      const bool               end_of_transfer,
                                      const uint32_t           frame_index)
{
    UDPARD_ASSERT(frame_index <= UDPARD_MAX_FRAME_INDEX);
    uint32_t end_of_transfer_mask = (uint32_t) (end_of_transfer ? 1 : 0) << (uint32_t) UDPARD_END_OF_TRANSFER_OFFSET;
    size_t cyphal_header_size_without_crc = sizeof(UdpardFrameHeader) - CYPHAL_HEADER_CRC_SIZE_BYTES;
    header->transfer_id           = transfer_id;
    header->version               = (uint8_t) UDPARD_CYPHAL_HEADER_VERSION;
    header->priority              = (uint8_t) priority;
    header->frame_index_eot       = end_of_transfer_mask | frame_index;
    header->source_node_id        = src_node_id;
    header->destination_node_id   = dst_node_id;
    header->cyphal_header_checksum = cyphalHeaderCrcAdd(CYPHAL_HEADER_CRC_INITIAL, cyphal_header_size_without_crc, header);
    if (transfer_kind == UdpardTransferKindMessage)
    {
        // both port_id and the data_specifier start at bit-0. No shift of the port_id value is necessary.
        header->data_specifier = port_id & UDPARD_DATA_SPECIFIER_MESSAGE_MASK;
    }
    else
    {
         // SNM (1) + IRNR + ServiceID
        if (transfer_kind == UdpardTransferKindRequest)
        {
            header->data_specifier = UDPARD_DATA_SPECIFIER_SERVICE_REQUEST | port_id;
        }
        else
        {
            header->data_specifier = UDPARD_DATA_SPECIFIER_SERVICE_RESPONSE | port_id;
        }
    }
}

/// The item is only allocated and initialized, but NOT included into the queue! The caller needs to do that.
UDPARD_PRIVATE TxItem* txAllocateQueueItem(UdpardInstance* const               ins,
                                           const UdpardSessionSpecifier* const spec,
                                           const UdpardMicrosecond             deadline_usec,
                                           const size_t                        payload_size)
{
    UDPARD_ASSERT(ins != NULL);
    UDPARD_ASSERT(payload_size > 0U);
    TxItem* const out = (TxItem*) ins->memory_allocate(ins, sizeof(TxItem) - UDPARD_MTU_MAX + payload_size);
    if (out != NULL)
    {
        out->base.base.up    = NULL;
        out->base.base.lr[0] = NULL;
        out->base.base.lr[1] = NULL;
        out->base.base.bf    = 0;

        out->base.next_in_transfer = NULL;  // Last by default.
        out->base.tx_deadline_usec = deadline_usec;

        out->base.frame.payload_size                    = payload_size;
        out->base.frame.payload                         = out->payload_buffer;
        out->base.specifier.data_specifier              = spec->data_specifier;
        out->base.specifier.destination_route_specifier = spec->destination_route_specifier;
        out->base.specifier.source_route_specifier      = spec->source_route_specifier;
    }
    return out;
}

/// Frames with identical UDP ID that are added later always compare greater than their counterparts with same UDP ID.
/// This ensures that UDP frames with the same UDP ID are transmitted in the FIFO order.
/// Frames that should be transmitted earlier compare smaller (i.e., put on the left side of the tree).
UDPARD_PRIVATE int8_t txAVLPredicate(void* const user_reference,  // NOSONAR Cavl API requires pointer to non-const.
                                     const UdpardTreeNode* const node)
{
    const UdpardTxQueueItem* const target = (const UdpardTxQueueItem*) user_reference;
    const UdpardTxQueueItem* const other  = (const UdpardTxQueueItem*) node;
    UDPARD_ASSERT((target != NULL) && (other != NULL));
    if (target->frame.udp_cyphal_header.priority > other->frame.udp_cyphal_header.priority)
    {
        return +1;
    }
    if (target->frame.udp_cyphal_header.transfer_id >= other->frame.udp_cyphal_header.transfer_id)
    {
        return +1;
    }
    return -1;
}

/// Returns the number of frames enqueued or error (i.e., =1 or <0).
UDPARD_PRIVATE int32_t txPushSingleFrame(UdpardTxQueue* const                que,
                                         UdpardInstance* const               ins,
                                         const UdpardMicrosecond             deadline_usec,
                                         const UdpardSessionSpecifier* const specifier,
                                         const UdpardNodeID                  src_node_id,
                                         const UdpardNodeID                  dst_node_id,
                                         const UdpardPortID                  port_id,
                                         const UdpardTransferKind            transfer_kind,
                                         const UdpardPriority                priority,
                                         const UdpardTransferID              transfer_id,
                                         const size_t                        payload_size,
                                         const void* const                   payload)
{
    UDPARD_ASSERT(ins != NULL);
    UDPARD_ASSERT((payload != NULL) || (payload_size == 0));
    // The size of a Frame header shouldn't change, but best to check it is at least bigger than 0
    UDPARD_ASSERT(sizeof(UdpardFrameHeader) > 0);  // NOLINT
    const size_t frame_payload_size = payload_size + sizeof(UdpardFrameHeader) + CRC_SIZE_BYTES;
    UDPARD_ASSERT(frame_payload_size > payload_size);
    int32_t       out = 0;
    TxItem* const tqi =
        (que->size < que->capacity) ? txAllocateQueueItem(ins, specifier, deadline_usec, frame_payload_size) : NULL;
    if (tqi != NULL)
    {
        if (payload_size > 0U)  // The check is needed to avoid calling memcpy() with a NULL pointer, it's an UB.
        {
            UDPARD_ASSERT(payload != NULL);
            // Clang-Tidy raises an error recommending the use of memcpy_s() instead.
            // We ignore it because the safe functions are poorly supported; reliance on them may limit the portability.
            (void) memcpy(&tqi->payload_buffer[sizeof(UdpardFrameHeader)], payload, payload_size);  // NOLINT
        }
        /// Create the FrameHeader
        txMakeFrameHeader(&tqi->base.frame.udp_cyphal_header, src_node_id, dst_node_id, port_id, transfer_kind, priority, transfer_id, true, 1);
        // Clang-Tidy raises an error recommending the use of memcpy_s() instead.
        // We ignore it because the safe functions are poorly supported; reliance on them may limit the portability.
        (void) memcpy(&tqi->payload_buffer[0], &tqi->base.frame.udp_cyphal_header, sizeof(UdpardFrameHeader));  // NOLINT
 
        // Insert CRC
        size_t frame_offset = payload_size + sizeof(UdpardFrameHeader);
        TransferCRC crc = crcValue(crcAdd(CRC_INITIAL, payload_size, payload));
        uint8_t crc_as_byte[] = {(uint8_t)(crc & BYTE_MAX & CRC_BYTE_MASK),
                                 (uint8_t)(crc >> (BITS_PER_BYTE * 1U) & CRC_BYTE_MASK),
                                 (uint8_t)(crc >> (BITS_PER_BYTE * 2U) & CRC_BYTE_MASK),
                                 (uint8_t)(crc >> (BITS_PER_BYTE * 3U) & CRC_BYTE_MASK)};
        for (unsigned int i = 0; i < sizeof(crc_as_byte); i++)
        {
            tqi->payload_buffer[frame_offset++] = crc_as_byte[i];
        }

        // Insert the newly created TX item into the queue.
        const UdpardTreeNode* const res = cavlSearch(&que->root, &tqi->base.base, &txAVLPredicate, &avlTrivialFactory);
        (void) res;
        UDPARD_ASSERT(res == &tqi->base.base);
        que->size++;
        UDPARD_ASSERT(que->size <= que->capacity);
        out = 1;  // One frame enqueued.
    }
    else
    {
        out = -UDPARD_ERROR_OUT_OF_MEMORY;
    }
    UDPARD_ASSERT((out < 0) || (out == 1));
    return out;
}

// Produces a chain of Tx queue items for later insertion into the Tx queue. The tail is NULL if OOM.
UDPARD_PRIVATE TxChain txGenerateMultiFrameChain(UdpardInstance* const        ins,
                                                 const size_t                 presentation_layer_mtu,
                                                 const                        UdpardMicrosecond deadline_usec,
                                                 const UdpardSessionSpecifier* const specifier,
                                                 const UdpardNodeID           src_node_id,
                                                 const UdpardNodeID           dst_node_id,
                                                 const UdpardPortID           port_id,
                                                 const UdpardTransferKind     transfer_kind,
                                                 const UdpardPriority         priority,
                                                 const UdpardTransferID       transfer_id,
                                                 const size_t                 payload_size,
                                                 const void* const            payload)
{
    UDPARD_ASSERT(ins != NULL);
    UDPARD_ASSERT(presentation_layer_mtu > sizeof(UdpardFrameHeader));
    
    // Otherwise, a single-frame transfer should be used.
    // CRC_SIZE_BYTES 
    UDPARD_ASSERT(payload_size + CRC_SIZE_BYTES > presentation_layer_mtu - sizeof(UdpardFrameHeader)); 
    UDPARD_ASSERT(payload != NULL);

    TxChain        out                   = {NULL, NULL, 0};
    const size_t   payload_size_with_crc = payload_size + CRC_SIZE_BYTES;
    size_t         offset                = 0U;
    TransferCRC    crc                   = crcValue(crcAdd(CRC_INITIAL, payload_size, payload));
    const uint8_t* payload_ptr           = (const uint8_t*) payload;
    uint32_t       frame_index           = 0U;
    uint8_t        crc_as_byte[]         = {(uint8_t)(crc & BYTE_MAX & CRC_BYTE_MASK), 
                                            (uint8_t)(crc >> (BITS_PER_BYTE * 1U) & CRC_BYTE_MASK),
                                            (uint8_t)(crc >> (BITS_PER_BYTE * 2U) & CRC_BYTE_MASK),
                                            (uint8_t)(crc >> (BITS_PER_BYTE * 3U) & CRC_BYTE_MASK)};
    size_t         last_crc_index        = 0U;
    size_t         inserted_crc_amount   = 0;
    while (offset < payload_size_with_crc)
    {
        out.size++;
        size_t frame_payload_size_with_header = 0U;
        frame_payload_size_with_header =
                ((payload_size_with_crc - offset + sizeof(UdpardFrameHeader)) < presentation_layer_mtu)
                ? (payload_size_with_crc - offset + sizeof(UdpardFrameHeader))
                : (presentation_layer_mtu);

        TxItem* const tqi = txAllocateQueueItem(ins, specifier, deadline_usec, frame_payload_size_with_header);
        if (NULL == out.head)
        {
            out.head = tqi;
        }
        else
        {
            out.tail->base.next_in_transfer = (UdpardTxQueueItem*) tqi;
        }
        out.tail = tqi;
        // If the queue item is not generated successfully,
        // we'll end the while loop early to stop the chain
        // generation.
        // We'll do the memory clean up after this function is done.
        if (NULL == out.tail)
        {
            break;
        }
        const size_t frame_payload_size = frame_payload_size_with_header;
        size_t frame_offset = 0U;
        // Index to start copying payload to the frame.
        size_t start_index = offset;
        // Size of remaining payload + CRC
        // We'll compare the size of remaining payload+crc and available size of current frame
        // and set the move_size_with_crc to the smaller one.
        size_t move_size_with_crc = payload_size_with_crc - offset 
                                    >= frame_payload_size - sizeof(UdpardFrameHeader)
                                    ? frame_payload_size - sizeof(UdpardFrameHeader)
                                    : payload_size_with_crc - offset;
        // Index referring the end of copied payload with CRC.
        size_t end_index = start_index + move_size_with_crc;
        // True if all payload has been process and is ready to insert CRC
        bool initial_payload_overrun = (end_index >= payload_size);
        // Number of CRC byte that has been inserted.
        size_t overrun_amount = ((initial_payload_overrun) ? end_index - payload_size : inserted_crc_amount) - inserted_crc_amount;
        inserted_crc_amount += overrun_amount;
        // Size of remaining payload without CRC
        size_t payload_move_size = move_size_with_crc - overrun_amount;

        txMakeFrameHeader(&out.tail->base.frame.udp_cyphal_header, src_node_id, dst_node_id, port_id, transfer_kind, priority, transfer_id, false, ++frame_index);
        if(initial_payload_overrun)
        {
            // Insert payload and header
            // Clang-Tidy raises an error recommending the use of memcpy_s() instead.
            // We ignore it because the safe functions are poorly supported; reliance on them may limit the portability.
            (void) memcpy(&out.tail->payload_buffer[0], &out.tail->base.frame.udp_cyphal_header, sizeof(UdpardFrameHeader)); // NOLINT
            (void) memcpy(&out.tail->payload_buffer[sizeof(UdpardFrameHeader)], payload_ptr, payload_move_size); // NOLINT
            frame_offset += sizeof(UdpardFrameHeader) + payload_move_size;
            // Insert crc into same frame
            for(unsigned int i = 0; i < overrun_amount; i++)
            {
                out.tail->payload_buffer[frame_offset++] = crc_as_byte[i + last_crc_index];
            }
            last_crc_index += overrun_amount;
        }
        else
        {
            // Clang-Tidy raises an error recommending the use of memcpy_s() instead.
            // We ignore it because the safe functions are poorly supported; reliance on them may limit the portability.
            (void) memcpy(&out.tail->payload_buffer[0], &out.tail->base.frame.udp_cyphal_header, sizeof(UdpardFrameHeader)); // NOLINT
            (void) memcpy(&out.tail->payload_buffer[sizeof(UdpardFrameHeader)], payload_ptr, payload_move_size); // NOLINT
            frame_offset += sizeof(UdpardFrameHeader) + payload_move_size;
        }
        payload_ptr += payload_move_size;
        offset += move_size_with_crc;

        // Deal with last frame after inserting all payload and crc
        if(end_index == payload_size_with_crc)
        {
            out.tail->base.frame.udp_cyphal_header.frame_index_eot = (uint32_t) (1U << (uint32_t) UDPARD_END_OF_TRANSFER_OFFSET) + frame_index;
            // Clang-Tidy raises an error recommending the use of memcpy_s() instead.
            // We ignore it because the safe functions are poorly supported; reliance on them may limit the portability.
            (void) memcpy(&out.tail->payload_buffer[0], &out.tail->base.frame.udp_cyphal_header, sizeof(UdpardFrameHeader)); // NOLINT
            UDPARD_ASSERT((frame_offset) == out.tail->base.frame.payload_size);
        }
    }
    return out;
}

/// Returns the number of frames enqueued or error.
UDPARD_PRIVATE int32_t txPushMultiFrame(UdpardTxQueue* const    que,
                                        UdpardInstance* const   ins,
                                        const size_t            presentation_layer_mtu,
                                        const UdpardMicrosecond deadline_usec,
                                        const UdpardSessionSpecifier* const specifier,
                                        const UdpardNodeID      src_node_id,
                                        const UdpardNodeID      dst_node_id,
                                        const UdpardPortID      port_id,
                                        const UdpardTransferKind transfer_kind,
                                        const UdpardPriority    priority,
                                        const UdpardTransferID  transfer_id,
                                        const size_t            payload_size,
                                        const void* const       payload)
{

    UDPARD_ASSERT((ins != NULL) && (que != NULL));
    UDPARD_ASSERT(presentation_layer_mtu > sizeof(UdpardFrameHeader));
    UDPARD_ASSERT(payload_size + CRC_SIZE_BYTES > presentation_layer_mtu - sizeof(UdpardFrameHeader));
    const size_t payload_size_with_crc = payload_size + CRC_SIZE_BYTES;
    const size_t num_frames = (payload_size_with_crc
                               + presentation_layer_mtu 
                               - sizeof(UdpardFrameHeader) - 1)
                               / (presentation_layer_mtu 
                               - sizeof(UdpardFrameHeader));
    UDPARD_ASSERT(num_frames >= 2);

    int32_t out = 0;
    if ((que->size + num_frames) <= que->capacity)
    {
        const TxChain sq = txGenerateMultiFrameChain(ins,
                                                     presentation_layer_mtu,
                                                     deadline_usec,
                                                     specifier,
                                                     src_node_id,
                                                     dst_node_id,
                                                     port_id,
                                                     transfer_kind,
                                                     priority,
                                                     transfer_id,
                                                     payload_size,
                                                     payload);
        if (sq.tail != NULL)
        {
            UdpardTxQueueItem* next = &sq.head->base;
            do
            {
               const UdpardTreeNode* const res = cavlSearch(&que->root, &next->base, &txAVLPredicate, &avlTrivialFactory);
               (void) res;
               UDPARD_ASSERT(res == &next->base);
               UDPARD_ASSERT(que->root != NULL);
               next = next->next_in_transfer;
            } while (next != NULL);
            UDPARD_ASSERT(num_frames == sq.size);
            que->size += sq.size;
            UDPARD_ASSERT(que->size <= que->capacity);
            UDPARD_ASSERT((sq.size + 0ULL) <= INT32_MAX);
            out = (int32_t) sq.size;
        }
        else
        {
            out = -UDPARD_ERROR_OUT_OF_MEMORY;
            UdpardTxQueueItem* head = &sq.head->base;
            // We'll clean up the memory after txGenerateMultiFrameChain is done
            // with an UDPARD_ERROR_OUT_OF_MEMORY error raised.
            while (head != NULL)
            {
                UdpardTxQueueItem* const next = head->next_in_transfer;
                ins->memory_free(ins, head);
                head = next;
            }
        }

    }
    else  // We predict that we're going to run out of queue, don't bother serializing the transfer.
    {
        out = -UDPARD_ERROR_OUT_OF_MEMORY;
    }
    UDPARD_ASSERT((out < 0) || (out >= 2));
    return out;
}

// --------------------------------------------- RECEPTION ---------------------------------------------

#define RX_SESSIONS_PER_SUBSCRIPTION (UDPARD_NODE_ID_MAX + 1U)

/// The memory requirement model provided in the documentation assumes that the maximum size of this structure never
/// exceeds 48 bytes on any conventional platform.
/// A user that needs a detailed analysis of the worst-case memory consumption may compute the size of this
/// structure for the particular platform at hand manually or by evaluating its sizeof(). The fields are ordered to
/// minimize the amount of padding on all conventional platforms.
typedef struct UdpardInternalRxSession
{
    UdpardMicrosecond transfer_timestamp_usec;  ///< Timestamp of the last received start-of-transfer.
    size_t            total_payload_size;       ///< The payload size before the implicit truncation, including the CRC.
    size_t            payload_size;             ///< How many bytes received so far.
    uint8_t*          payload;                  ///< Dynamically allocated and handed off to the application when done.
    TransferCRC       calculated_crc;           ///< Updated with the received payload in real time.
    UdpardTransferID  transfer_id;
    uint8_t           redundant_transport_index;  ///< Arbitrary value in [0, 255].
    uint32_t          last_udp_header_index;
} UdpardInternalRxSession;

/// High-level transport frame model.
typedef struct
{
    UdpardMicrosecond  timestamp_usec;
    UdpardHeaderVersion version;
    UdpardPriority     priority;
    UdpardTransferKind transfer_kind;
    UdpardPortID       port_id;
    UdpardNodeID       source_node_id;
    UdpardNodeID       destination_node_id;
    UdpardTransferID   transfer_id;
    bool               start_of_transfer;
    bool               end_of_transfer;
    size_t             payload_size;
    const void*        payload;
    uint32_t           frame_index;
} RxFrameModel;

UDPARD_PRIVATE UdpardPortID getPortIdFromDataSpecifiers(UdpardUdpPortID data_specifier)
{
    if ((uint16_t)(data_specifier >> UDPARD_SERVICE_NOT_MESSAGE_DATA_SPECIFIER_OFFSET) & 1U)
    {
        return (UdpardPortID) (data_specifier & UDPARD_SERVICE_ID_MASK);
    }
    return (UdpardPortID) (data_specifier & UDPARD_SUBJECT_ID_MASK);
}

UDPARD_PRIVATE UdpardTransferKind getTransferKindFromDataSpecifier(UdpardUdpPortID data_specifier)
{
    if ((uint16_t)(data_specifier >> UDPARD_SERVICE_NOT_MESSAGE_DATA_SPECIFIER_OFFSET) & 1U)
    {
        return ((uint16_t)(data_specifier >> UDPARD_IRNR_DATA_SPECIFIER_OFFSET) & 1U) ? UdpardTransferKindRequest : UdpardTransferKindResponse;
    }
    return UdpardTransferKindMessage;
}

/// Returns truth if the frame is valid and parsed successfully. False if the frame is not a valid Cyphal/UDP frame.
UDPARD_PRIVATE bool rxTryParseFrame(const UdpardMicrosecond             timestamp_usec,
                                    UdpardFrame* const                  frame,
                                    RxFrameModel* const                 out)
{

    UDPARD_ASSERT(frame != NULL);
    UDPARD_ASSERT(out != NULL);
    if (frame->payload_size < sizeof(frame->udp_cyphal_header))
    {
        return false;
    }
    bool valid = true;

    // Get the Header out of the frame
    UDPARD_ASSERT(frame->payload != NULL);
    // Clang-Tidy raises an error recommending the use of memcpy_s() instead.
    // We ignore it because the safe functions are poorly supported; reliance on them may limit the portability.
    (void) memcpy(&frame->udp_cyphal_header, frame->payload, sizeof(frame->udp_cyphal_header));  // NOLINT
    out->timestamp_usec = timestamp_usec;

    out->version             = frame->udp_cyphal_header.version;
    out->priority            = (UdpardPriority) frame->udp_cyphal_header.priority;
    out->source_node_id     = frame->udp_cyphal_header.source_node_id;
    out->transfer_kind       = getTransferKindFromDataSpecifier(frame->udp_cyphal_header.data_specifier);
    out->port_id             = getPortIdFromDataSpecifiers(frame->udp_cyphal_header.data_specifier);
    out->destination_node_id = frame->udp_cyphal_header.destination_node_id;
    // Payload parsing.
    out->payload_size        = frame->payload_size - sizeof(frame->udp_cyphal_header);  // Cut off the header size.
    out->payload             = (void*) ((uint8_t*) frame->payload + sizeof(frame->udp_cyphal_header));

    out->transfer_id         = frame->udp_cyphal_header.transfer_id;
    out->start_of_transfer   = (((frame->udp_cyphal_header.frame_index_eot) & (UDPARD_MAX_FRAME_INDEX)) == 1);
    out->end_of_transfer     = ((frame->udp_cyphal_header.frame_index_eot >> UDPARD_END_OF_TRANSFER_OFFSET) == 1);
    out->frame_index         = frame->udp_cyphal_header.frame_index_eot;
    // Make sure header version is supported
    valid = valid && (out->version >= UDPARD_CYPHAL_HEADER_VERSION);
    if (out->transfer_kind != UdpardTransferKindMessage)
    {
        valid = valid && (out->source_node_id != out->destination_node_id);
    }
    // Anonymous transfers can be only single-frame transfers.
    valid =
        valid && ((out->start_of_transfer && out->end_of_transfer) || (UDPARD_NODE_ID_UNSET != out->source_node_id));
    // A frame that is a part of a multi-frame transfer cannot be empty (tail byte not included).
    valid = valid && ((out->payload_size > 0) || (out->start_of_transfer && out->end_of_transfer));
    return valid;
}

UDPARD_PRIVATE void rxInitTransferMetadataFromFrame(const RxFrameModel* const     frame,
                                                    UdpardTransferMetadata* const out_transfer)
{
    UDPARD_ASSERT(frame != NULL);
    UDPARD_ASSERT(frame->payload != NULL);
    UDPARD_ASSERT(out_transfer != NULL);
    out_transfer->priority       = frame->priority;
    out_transfer->transfer_kind  = frame->transfer_kind;
    out_transfer->port_id        = frame->port_id;
    out_transfer->remote_node_id = frame->source_node_id;
    out_transfer->transfer_id    = frame->transfer_id;
}

// Assume we will never roll over a transfer id with 64bits
UDPARD_PRIVATE uint64_t rxComputeTransferIDDifference(const uint64_t a, const uint64_t b)
{
    UDPARD_ASSERT(a <= UDPARD_TRANSFER_ID_MAX);
    UDPARD_ASSERT(b <= UDPARD_TRANSFER_ID_MAX);
    return a - b;
}

UDPARD_PRIVATE int8_t rxSessionWritePayload(UdpardInstance* const          ins,
                                            UdpardInternalRxSession* const rxs,
                                            const size_t                   extent,
                                            const size_t                   payload_size,
                                            const void* const              payload)
{
    UDPARD_ASSERT(ins != NULL);
    UDPARD_ASSERT(rxs != NULL);
    UDPARD_ASSERT((payload != NULL) || (payload_size == 0U));
    UDPARD_ASSERT(rxs->payload_size <= extent);  // This invariant is enforced by the subscription logic.
    UDPARD_ASSERT(rxs->payload_size <= rxs->total_payload_size);

    rxs->total_payload_size += payload_size;

    // Allocate the payload lazily, as late as possible.
    if ((NULL == rxs->payload) && (extent > 0U))
    {
        UDPARD_ASSERT(rxs->payload_size == 0);
        rxs->payload = ins->memory_allocate(ins, extent);
    }

    int8_t out = 0;
    if (rxs->payload != NULL)
    {
        // Copy the payload into the contiguous buffer. Apply the implicit truncation rule if necessary.
        size_t bytes_to_copy = payload_size;
        if ((rxs->payload_size + bytes_to_copy) > extent)
        {
            UDPARD_ASSERT(rxs->payload_size <= extent);
            bytes_to_copy = extent - rxs->payload_size;
            UDPARD_ASSERT((rxs->payload_size + bytes_to_copy) == extent);
            UDPARD_ASSERT(bytes_to_copy < payload_size);
        }
        // This memcpy() call here is one of the two variable-complexity operations in the RX pipeline;
        // the other one is the search of the matching subscription state.
        // Excepting these two cases, the entire RX pipeline contains neither loops nor recursion.
        // Intentional violation of MISRA: indexing on a pointer. This is done to avoid pointer arithmetics.
        // Clang-Tidy raises an error recommending the use of memcpy_s() instead.
        // We ignore it because the safe functions are poorly supported; reliance on them may limit the portability.
        (void) memcpy(&rxs->payload[rxs->payload_size], payload, bytes_to_copy);  // NOLINT NOSONAR
        rxs->payload_size += bytes_to_copy;
        UDPARD_ASSERT(rxs->payload_size <= extent);
    }
    else
    {
        UDPARD_ASSERT(rxs->payload_size == 0);
        out = (extent > 0U) ? -UDPARD_ERROR_OUT_OF_MEMORY : 0;
    }
    UDPARD_ASSERT(out <= 0);
    return out;
}

UDPARD_PRIVATE void rxSessionRestart(UdpardInstance* const ins, UdpardInternalRxSession* const rxs)
{
    UDPARD_ASSERT(ins != NULL);
    UDPARD_ASSERT(rxs != NULL);
    ins->memory_free(ins, rxs->payload);  // May be NULL, which is OK.
    rxs->total_payload_size = 0U;
    rxs->payload_size       = 0U;
    rxs->payload            = NULL;
    rxs->calculated_crc     = CRC_INITIAL;
    rxs->transfer_id        = (UdpardTransferID) (rxs->transfer_id + 1U) & UDPARD_TRANSFER_ID_MAX;
}

UDPARD_PRIVATE int8_t rxSessionAcceptFrame(UdpardInstance* const          ins,
                                           UdpardInternalRxSession* const rxs,
                                           const RxFrameModel* const      frame,
                                           const size_t                   extent,
                                           UdpardRxTransfer* const        out_transfer)
{
    UDPARD_ASSERT(ins != NULL);
    UDPARD_ASSERT(rxs != NULL);
    UDPARD_ASSERT(frame != NULL);
    UDPARD_ASSERT(frame->payload != NULL);
    UDPARD_ASSERT(frame->transfer_id <= UDPARD_TRANSFER_ID_MAX);
    UDPARD_ASSERT(out_transfer != NULL);

    if (frame->start_of_transfer)  // The transfer timestamp is the timestamp of its first frame.
    {
        rxs->transfer_timestamp_usec = frame->timestamp_usec;
    }

    rxs->calculated_crc = crcAdd(rxs->calculated_crc, frame->payload_size, frame->payload);

    int8_t out = rxSessionWritePayload(ins, rxs, extent, frame->payload_size, frame->payload);
    if (out < 0)
    {
        UDPARD_ASSERT(-UDPARD_ERROR_OUT_OF_MEMORY == out);
        rxSessionRestart(ins, rxs);  // Out-of-memory.
    }
    else if (frame->end_of_transfer)
    {
        UDPARD_ASSERT(0 == out);
        if (CRC_RESIDUE == rxs->calculated_crc)
        {

            out = 1;  // One transfer received, notify the application.
            rxInitTransferMetadataFromFrame(frame, &out_transfer->metadata);
            out_transfer->timestamp_usec = rxs->transfer_timestamp_usec;
            out_transfer->payload_size   = rxs->payload_size;
            out_transfer->payload        = rxs->payload;

            // Cut off the CRC from the payload if it's there -- we don't want to expose it to the user.
            UDPARD_ASSERT(rxs->total_payload_size >= rxs->payload_size);
            // For single frame transfers, the truncated amount will be 0 (total_payload_size == payload_size)
            const size_t truncated_amount = rxs->total_payload_size - rxs->payload_size;
            if (CRC_SIZE_BYTES > truncated_amount)
            {
                UDPARD_ASSERT(out_transfer->payload_size >= (CRC_SIZE_BYTES - truncated_amount));
                out_transfer->payload_size -= CRC_SIZE_BYTES - truncated_amount;
            }

            rxs->payload = NULL;  // Ownership passed over to the application, nullify to prevent freeing.
        }
        rxSessionRestart(ins, rxs);  // Successful completion.
    }
    return out;
}

/// RX session state machine update is the most intricate part of any Cyphal transport implementation.
/// The state model used here is derived from the reference pseudocode given in the original UAVCAN v0
/// specification. The Cyphal/UDP v1 specification, which this library is an implementation of, does not provide any
/// reference pseudocode. Instead, it takes a higher-level, more abstract approach, where only the high-level
/// requirements are given and the particular algorithms are left to be implementation-defined. Such abstract
/// approach is much advantageous because it allows implementers to choose whatever solution works best for the
/// specific application at hand, while the wire compatibility is still guaranteed by the high-level requirements
/// given in the specification.
UDPARD_PRIVATE int8_t rxSessionUpdate(UdpardInstance* const          ins,
                                      UdpardInternalRxSession* const rxs,
                                      const RxFrameModel* const      frame,
                                      const uint8_t                  redundant_transport_index,
                                      const UdpardMicrosecond        transfer_id_timeout_usec,
                                      const size_t                   extent,
                                      UdpardRxTransfer* const        out_transfer)
{
    UDPARD_ASSERT(ins != NULL);
    UDPARD_ASSERT(rxs != NULL);
    UDPARD_ASSERT(frame != NULL);
    UDPARD_ASSERT(out_transfer != NULL);
    UDPARD_ASSERT(rxs->transfer_id <= UDPARD_TRANSFER_ID_MAX);
    UDPARD_ASSERT(frame->transfer_id <= UDPARD_TRANSFER_ID_MAX);

    const bool tid_timed_out = (frame->timestamp_usec > rxs->transfer_timestamp_usec) &&
                               ((frame->timestamp_usec - rxs->transfer_timestamp_usec) > transfer_id_timeout_usec);

    const bool not_previous_tid = rxComputeTransferIDDifference(rxs->transfer_id, frame->transfer_id) > 1;

    const bool need_restart = tid_timed_out || ((rxs->redundant_transport_index == redundant_transport_index) &&
                                                frame->start_of_transfer && not_previous_tid);

    if (need_restart)
    {
        rxs->total_payload_size        = 0U;
        rxs->payload_size              = 0U;
        rxs->calculated_crc            = CRC_INITIAL;
        rxs->transfer_id               = frame->transfer_id;
        rxs->redundant_transport_index = redundant_transport_index;
        rxs->last_udp_header_index     = 0U;
    }

    int8_t out = 0;
    if (need_restart && (!frame->start_of_transfer))
    {
        rxSessionRestart(ins, rxs);  // SOT-miss, no point going further.
    }
    else
    {
        // multi-frame transfer
        if (!(frame->start_of_transfer && frame->end_of_transfer))
        {
            if (frame->end_of_transfer)
            {
                uint32_t next_expected_frame_index = (1U << UDPARD_END_OF_TRANSFER_OFFSET) + rxs->last_udp_header_index + 1;
                if (frame->frame_index != next_expected_frame_index)
                {
                    // Out of order multiframe packet received
                    out = -UDPARD_ERROR_OUT_OF_ORDER;
                    // Reset previous frame index to 0
                    rxs->last_udp_header_index = 0U;
                    rxSessionRestart(ins, rxs);
                    return out;
                }
                rxs->last_udp_header_index = 0U;
            }
            else
            {
                if ((!frame->start_of_transfer && frame->frame_index != rxs->last_udp_header_index + 1)
                    || (frame->start_of_transfer && frame->frame_index != 1U))
                {
                    // Out of order multiframe packet received
                    out = -UDPARD_ERROR_OUT_OF_ORDER;
                    rxSessionRestart(ins, rxs);
                    return out;
                }
                rxs->last_udp_header_index = frame->frame_index;
            }
        }
        const bool correct_transport = (rxs->redundant_transport_index == redundant_transport_index);
        const bool correct_tid       = (frame->transfer_id == rxs->transfer_id);
        if (correct_transport && correct_tid)
        {
            out = rxSessionAcceptFrame(ins, rxs, frame, extent, out_transfer);
        }
    }
    return out;
}

UDPARD_PRIVATE int8_t rxAcceptFrame(UdpardInstance* const       ins,
                                    UdpardRxSubscription* const subscription,
                                    const RxFrameModel* const   frame,
                                    const uint8_t               redundant_transport_index,
                                    UdpardRxTransfer* const     out_transfer)
{
    UDPARD_ASSERT(ins != NULL);
    UDPARD_ASSERT(subscription != NULL);
    UDPARD_ASSERT(subscription->port_id == frame->port_id);
    UDPARD_ASSERT(frame != NULL);
    UDPARD_ASSERT(frame->payload != NULL);
    UDPARD_ASSERT(frame->transfer_id <= UDPARD_TRANSFER_ID_MAX);
    UDPARD_ASSERT((UDPARD_NODE_ID_UNSET == frame->destination_node_id) || (ins->node_id == frame->destination_node_id));
    UDPARD_ASSERT(out_transfer != NULL);

    int8_t out = 0;
    if ((frame->source_node_id <= UDPARD_NODE_ID_MAX) && (frame->source_node_id != UDPARD_NODE_ID_UNSET))
    {
        // If such session does not exist, create it. This only makes sense if this is the first frame of a
        // transfer, otherwise, we won't be able to receive the transfer anyway so we don't bother.
        if ((NULL == subscription->sessions[frame->source_node_id]) && frame->start_of_transfer)
        {
            UdpardInternalRxSession* const rxs =
                (UdpardInternalRxSession*) ins->memory_allocate(ins, sizeof(UdpardInternalRxSession));
            subscription->sessions[frame->source_node_id] = rxs;
            if (rxs != NULL)
            {
                rxs->transfer_timestamp_usec   = frame->timestamp_usec;
                rxs->total_payload_size        = 0U;
                rxs->payload_size              = 0U;
                rxs->payload                   = NULL;
                rxs->calculated_crc            = CRC_INITIAL;
                rxs->transfer_id               = frame->transfer_id;
                rxs->redundant_transport_index = redundant_transport_index;
            }
            else
            {
                out = -UDPARD_ERROR_OUT_OF_MEMORY;
            }
        }
        // There are two possible reasons why the session may not exist: 1. OOM; 2. SOT-miss.
        if (subscription->sessions[frame->source_node_id] != NULL)
        {
            UDPARD_ASSERT(out == 0);
            out = rxSessionUpdate(ins,
                                  subscription->sessions[frame->source_node_id],
                                  frame,
                                  redundant_transport_index,
                                  subscription->transfer_id_timeout_usec,
                                  subscription->extent,
                                  out_transfer);
        }
    }
    else
    {
        UDPARD_ASSERT(frame->source_node_id == UDPARD_NODE_ID_UNSET);
        // Anonymous transfers are stateless. No need to update the state machine, just blindly accept it.
        // We have to copy the data into an allocated storage because the API expects it: the lifetime shall be
        // independent of the input data and the memory shall be free-able.
        const size_t payload_size =
            (subscription->extent < frame->payload_size) ? subscription->extent : frame->payload_size;
        void* const payload = ins->memory_allocate(ins, payload_size);
        if (payload != NULL)
        {
            rxInitTransferMetadataFromFrame(frame, &out_transfer->metadata);
            out_transfer->timestamp_usec = frame->timestamp_usec;
            out_transfer->payload_size   = payload_size;
            out_transfer->payload        = payload;
            // Clang-Tidy raises an error recommending the use of memcpy_s() instead.
            // We ignore it because the safe functions are poorly supported; reliance on them may limit the
            // portability.
            (void) memcpy(payload, frame->payload, payload_size);  // NOLINT
            out = 1;
        }
        else
        {
            out = -UDPARD_ERROR_OUT_OF_MEMORY;
        }
    }
    return out;
}

UDPARD_PRIVATE int8_t
rxSubscriptionPredicateOnPortID(void* const user_reference,  // NOSONAR Cavl API requires pointer to non-const.
                                const UdpardTreeNode* const node)
{
    const UdpardPortID  sought    = *((const UdpardPortID*) user_reference);
    const UdpardPortID  other     = ((const UdpardRxSubscription*) node)->port_id;
    static const int8_t NegPos[2] = {-1, +1};
    // Clang-Tidy mistakenly identifies a narrowing cast to int8_t here, which is incorrect.
    return (sought == other) ? 0 : NegPos[sought > other];  // NOLINT no narrowing conversion is taking place here
}

UDPARD_PRIVATE int8_t
rxSubscriptionPredicateOnStruct(void* const user_reference,  // NOSONAR Cavl API requires pointer to non-const.
                                const UdpardTreeNode* const node)
{
    return rxSubscriptionPredicateOnPortID(&((UdpardRxSubscription*) user_reference)->port_id, node);
}

// --------------------------------------------- PUBLIC API ---------------------------------------------

UdpardInstance udpardInit(const UdpardMemoryAllocate memory_allocate, const UdpardMemoryFree memory_free)
{
    UDPARD_ASSERT(memory_allocate != NULL);
    UDPARD_ASSERT(memory_free != NULL);
    const UdpardInstance out = {
        .user_reference   = NULL,
        .node_id          = UDPARD_NODE_ID_UNSET,
        .memory_allocate  = memory_allocate,
        .memory_free      = memory_free,
        .rx_subscriptions = {NULL, NULL, NULL},
    };
    return out;
}

UdpardTxQueue udpardTxInit(const size_t capacity, const size_t mtu_bytes)
{
    UdpardTxQueue out = {
        .capacity       = capacity,
        .mtu_bytes      = mtu_bytes,
        .size           = 0,
        .root           = NULL,
        .user_reference = NULL,
    };
    return out;
}

int32_t udpardTxPush(UdpardTxQueue* const                que,
                     UdpardInstance* const               ins,
                     const UdpardMicrosecond             tx_deadline_usec,
                     const UdpardTransferMetadata* const metadata,
                     const size_t                        payload_size,
                     const void* const                   payload)
{
    int32_t out = -UDPARD_ERROR_INVALID_ARGUMENT;
    if ((ins != NULL) && (que != NULL) && (metadata != NULL) && ((payload != NULL) || (0U == payload_size)))
    {
        const size_t           pl_mtu = adjustPresentationLayerMTU(que->mtu_bytes);
        UdpardSessionSpecifier specifier;
        const int32_t specifier_result = txMakeSessionSpecifier(metadata, ins->node_id, ins->local_ip_addr, &specifier);
        if (specifier_result >= 0)
        {
            if (payload_size + CRC_SIZE_BYTES <= pl_mtu - sizeof(UdpardFrameHeader))
            {
                out = txPushSingleFrame(que,
                                        ins,
                                        tx_deadline_usec,
                                        &specifier,
                                        ins->node_id,
                                        metadata->remote_node_id,
                                        metadata->port_id,
                                        metadata->transfer_kind,
                                        metadata->priority,
                                        metadata->transfer_id,
                                        payload_size,
                                        payload);
                UDPARD_ASSERT((out < 0) || (out == 1));
            }
            else
            {
                out = txPushMultiFrame(que,
                                       ins,
                                       pl_mtu,
                                       tx_deadline_usec,
                                       &specifier,
                                       ins->node_id,
                                       metadata->remote_node_id,
                                       metadata->port_id,
                                       metadata->transfer_kind,
                                       metadata->priority,
                                       metadata->transfer_id,
                                       payload_size,
                                       payload);
                UDPARD_ASSERT((out < 0) || (out >= 2));
            }
        }
        else
        {
            out = specifier_result;
        }
    }
    UDPARD_ASSERT(out != 0);
    return out;
}

const UdpardTxQueueItem* udpardTxPeek(const UdpardTxQueue* const que)
{
    const UdpardTxQueueItem* out = NULL;
    if (que != NULL)
    {
        // Paragraph 6.7.2.1.15 of the C standard says:
        //     A pointer to a structure object, suitably converted, points to its initial member, and vice versa.
        out = (const UdpardTxQueueItem*) cavlFindExtremum(que->root, false);
    }
    return out;
}

UdpardTxQueueItem* udpardTxPop(UdpardTxQueue* const que, const UdpardTxQueueItem* const item)
{
    UdpardTxQueueItem* out = NULL;
    if ((que != NULL) && (item != NULL))
    {
        // Intentional violation of MISRA: casting away const qualifier. This is considered safe because the API
        // contract dictates that the pointer shall point to a mutable entity in RAM previously allocated by the
        // memory manager. It is difficult to avoid this cast in this context.
        out = (UdpardTxQueueItem*) item;  // NOSONAR casting away const qualifier.
        // Paragraph 6.7.2.1.15 of the C standard says:
        //     A pointer to a structure object, suitably converted, points to its initial member, and vice versa.
        // Note that the highest-priority frame is always a leaf node in the AVL tree, which means that it is very
        // cheap to remove.
        cavlRemove(&que->root, &item->base);
        que->size--;
    }
    return out;
}

int8_t udpardRxAccept(UdpardInstance* const         ins,
                      const UdpardMicrosecond       timestamp_usec,
                      UdpardFrame* const            frame,
                      const uint8_t                 redundant_transport_index,
                      UdpardRxTransfer* const       out_transfer,
                      UdpardRxSubscription** const  out_subscription)
{
    int8_t out = -UDPARD_ERROR_INVALID_ARGUMENT;
    if ((ins != NULL) && (out_transfer != NULL) && (frame != NULL) &&
        ((frame->payload != NULL) || (sizeof(frame->udp_cyphal_header) == frame->payload_size)))
    {
        RxFrameModel model = {0};
        if (rxTryParseFrame(timestamp_usec, frame, &model))
        {
            if ((UDPARD_NODE_ID_UNSET == model.destination_node_id) || (ins->node_id == model.destination_node_id))
            {
                // This is the reason the function has a logarithmic time complexity of the number of subscriptions.
                // Note also that this one of the two variable-complexity operations in the RX pipeline; the other
                // one is memcpy(). Excepting these two cases, the entire RX pipeline contains neither loops nor
                // recursion.
                UdpardRxSubscription* const sub =
                    (UdpardRxSubscription*) cavlSearch(&ins->rx_subscriptions[(size_t) model.transfer_kind],
                                                       &model.port_id,
                                                       &rxSubscriptionPredicateOnPortID,
                                                       NULL);
                if (out_subscription != NULL)
                {
                    *out_subscription = sub;  // Expose selected instance to the caller.
                }
                if (sub != NULL)
                {
                    UDPARD_ASSERT(sub->port_id == model.port_id);
                    out = rxAcceptFrame(ins, sub, &model, redundant_transport_index, out_transfer);
                }
                else
                {
                    out = 0;  // No matching subscription.
                }
            }
            else
            {
                out = 0;  // Mis-addressed frame (normally it should be filtered out by the hardware).
            }
        }
        else
        {
            out = 0;  // A non-Cyphal/UDP input frame.
        }
    }
    UDPARD_ASSERT(out <= 1);
    return out;
}

/// DONE -> This shouldn't change from canard to udpard
int8_t udpardRxSubscribe(UdpardInstance* const       ins,
                         const UdpardTransferKind    transfer_kind,
                         const UdpardPortID          port_id,
                         const size_t                extent,
                         const UdpardMicrosecond     transfer_id_timeout_usec,
                         UdpardRxSubscription* const out_subscription)
{
    int8_t       out = -UDPARD_ERROR_INVALID_ARGUMENT;
    const size_t tk  = (size_t) transfer_kind;
    if ((ins != NULL) && (out_subscription != NULL) && (tk < UDPARD_NUM_TRANSFER_KINDS))
    {
        // Reset to the initial state. This is absolutely critical because the new payload size limit may be larger
        // than the old value; if there are any payload buffers allocated, we may overrun them because they are
        // shorter than the new payload limit. So we clear the subscription and thus ensure that no overrun may
        // occur.
        out = udpardRxUnsubscribe(ins, transfer_kind, port_id);
        if (out >= 0)
        {
            out_subscription->transfer_id_timeout_usec = transfer_id_timeout_usec;
            out_subscription->extent                   = extent;
            out_subscription->port_id                  = port_id;
            for (size_t i = 0; i < RX_SESSIONS_PER_SUBSCRIPTION; i++)
            {
                // The sessions will be created ad-hoc. Normally, for a low-jitter deterministic system,
                // we could have pre-allocated sessions here, but that requires too much memory to be feasible.
                // We could accept an extra argument that would instruct us to pre-allocate sessions here?
                out_subscription->sessions[i] = NULL;
            }
            const UdpardTreeNode* const res = cavlSearch(&ins->rx_subscriptions[tk],
                                                         out_subscription,
                                                         &rxSubscriptionPredicateOnStruct,
                                                         &avlTrivialFactory);
            (void) res;
            UDPARD_ASSERT(res == &out_subscription->base);
            out = (out > 0) ? 0 : 1;
        }
    }
    return out;
}

/// DONE -> This shouldn't change from canard to udpard
int8_t udpardRxUnsubscribe(UdpardInstance* const    ins,
                           const UdpardTransferKind transfer_kind,
                           const UdpardPortID       port_id)
{
    int8_t       out = -UDPARD_ERROR_INVALID_ARGUMENT;
    const size_t tk  = (size_t) transfer_kind;
    if ((ins != NULL) && (tk < UDPARD_NUM_TRANSFER_KINDS))
    {
        UdpardPortID                port_id_mutable = port_id;
        UdpardRxSubscription* const sub             = (UdpardRxSubscription*)
            cavlSearch(&ins->rx_subscriptions[tk], &port_id_mutable, &rxSubscriptionPredicateOnPortID, NULL);
        if (sub != NULL)
        {
            cavlRemove(&ins->rx_subscriptions[tk], &sub->base);
            UDPARD_ASSERT(sub->port_id == port_id);
            out = 1;
            for (size_t i = 0; i < RX_SESSIONS_PER_SUBSCRIPTION; i++)
            {
                ins->memory_free(ins, (sub->sessions[i] != NULL) ? sub->sessions[i]->payload : NULL);
                ins->memory_free(ins, sub->sessions[i]);
                sub->sessions[i] = NULL;
            }
        }
        else
        {
            out = 0;
        }
    }
    return out;
}
