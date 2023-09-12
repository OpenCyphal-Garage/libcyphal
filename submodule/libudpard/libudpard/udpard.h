///                            ____                   ______            __          __
///                           / __ `____  ___  ____  / ____/_  ______  / /_  ____  / /
///                          / / / / __ `/ _ `/ __ `/ /   / / / / __ `/ __ `/ __ `/ /
///                         / /_/ / /_/ /  __/ / / / /___/ /_/ / /_/ / / / / /_/ / /
///                         `____/ .___/`___/_/ /_/`____/`__, / .___/_/ /_/`__,_/_/
///                             /_/                     /____/_/
///
/// description tbd
/// This library is based heavily on libcanard
// --------------------------------------------------------------------------------------------------------------------
///
/// This software is distributed under the terms of the MIT License.
/// Copyright (c) 2016 OpenCyphal.
/// Author: Pavel Kirienko <pavel@opencyphal.org>
/// Copyright 2022 Amazon.com, Inc. or its affiliates. All Rights Reserved.

#ifndef UDPARD_H_INCLUDED
#define UDPARD_H_INCLUDED

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

/// Semantic version of this library (not the Cyphal specification).
/// API will be backward compatible within the same major version.
#define UDPARD_VERSION_MAJOR 0
#define UDPARD_VERSION_MINOR 0

/// The version number of the Cyphal specification implemented by this library.
#define UDPARD_CYPHAL_SPECIFICATION_VERSION_MAJOR 1
#define UDPARD_CYPHAL_SPECIFICATION_VERSION_MINOR 0

/// The version number of the Cyphal Header supported by this library
#define UDPARD_CYPHAL_HEADER_VERSION 1

/// These error codes may be returned from the library API calls whose return type is a signed integer in the negated
/// form (e.g., error code 2 returned as -2). A non-negative return value represents success.
/// API calls whose return type is not a signed integer cannot fail by contract.
/// No other error states may occur in the library.
/// By contract, a well-characterized application with a properly sized memory pool will never encounter errors.
/// The error code 1 is not used because -1 is often used as a generic error code in 3rd-party code.
#define UDPARD_ERROR_INVALID_ARGUMENT 2
#define UDPARD_ERROR_OUT_OF_MEMORY 3
#define UDPARD_ERROR_OUT_OF_ORDER 4

/// In the case that we still need error codes but need to mutate an input we will default to a success code
#define UDPARD_SUCCESS 0

/// MTU values for the supported protocols.
/// RFC 791 states that hosts must be prepared to accept datagrams of up to 576 octets and it is expected that this
/// library will receive non ip-fragmented datagrams thus the minimum MTU should be larger than 576.
/// That being said, the MTU here is set to 1408 which is derived from 
/// A 1500B Ethernet MTU RFC 894  - 60B IPv4 max header - 8B UDP Header - 24B Cyphal header which is equal to 1408B
#define UDPARD_MTU_MAX 1408U // Note that to guarantee a single frame transfer your max payload size shall be 1404
                             // This value is to accomodate for a 4B CRC which is appended to the transfer.
#define UDPARD_MTU_UDP_IPV4 UDPARD_MTU_MAX
#define UDPARD_MTU_UDP_IPV6 UDPARD_MTU_MAX

/// Parameter ranges are inclusive; the lower bound is zero for all. See Cyphal/UDP Specification for background.
#define UDPARD_SUBJECT_ID_MAX 32767U   /// 15 bits subject ID
#define UDPARD_SERVICE_ID_MAX 65535U   /// The hard limit for ports
#define UDPARD_NODE_SUBNET_MAX 31U     /// 5 bits for subnet
#define UDPARD_NODE_ID_MAX 65534U      /// 16 bits - 1 is the hard limit. But this may change pending implementations
#define UDPARD_PRIORITY_MAX 7U
#define UDPARD_TRANSFER_ID_BIT_LENGTH 63ULL
#define UDPARD_TRANSFER_ID_MAX ((1ULL << UDPARD_TRANSFER_ID_BIT_LENGTH) - 1ULL)

#define UDPARD_NODE_ID_UNSET 65535U  /// For UDP 0xFFFF is the anonymous ID

/// This is the recommended transfer-ID timeout value given in the Cyphal Specification. The application may choose
/// different values per subscription (i.e., per data specifier) depending on its timing requirements.
#define UDPARD_DEFAULT_TRANSFER_ID_TIMEOUT_USEC 2000000UL  /// 2 seconds

// Forward declarations.
typedef struct UdpardInstance    UdpardInstance;
typedef struct UdpardTreeNode    UdpardTreeNode;
typedef struct UdpardTxQueueItem UdpardTxQueueItem;
typedef uint64_t                 UdpardMicrosecond;
typedef uint32_t                 UdpardNodeAddress;  /// Full Node IP Address for multi or uni cast
typedef uint32_t                 UdpardIPv4Addr;     /// Full Node IP Address for multi or uni cast
typedef uint16_t                 UdpardPortID;
typedef uint16_t                 UdpardUdpPortID;
typedef uint16_t                 UdpardNodeID;
typedef uint64_t                 UdpardTransferID;  /// TODO - This may need to be broken up for 32bit systems?
typedef uint8_t                  UdpardHeaderVersion;

/// Transfer priority level mnemonics per the recommendations given in the Cyphal Specification.
typedef enum
{
    UdpardPriorityExceptional = 0,
    UdpardPriorityImmediate   = 1,
    UdpardPriorityFast        = 2,
    UdpardPriorityHigh        = 3,
    UdpardPriorityNominal     = 4,  ///< Nominal priority level should be the default.
    UdpardPriorityLow         = 5,
    UdpardPrioritySlow        = 6,
    UdpardPriorityOptional    = 7,
} UdpardPriority;

/// Transfer kinds as defined by the Cyphal Specification.
typedef enum
{
    UdpardTransferKindMessage  = 0,  ///< Multicast, from publisher to all subscribers.
    UdpardTransferKindResponse = 1,  ///< Point-to-point, from server to client.
    UdpardTransferKindRequest  = 2,  ///< Point-to-point, from client to server.
} UdpardTransferKind;
#define UDPARD_NUM_TRANSFER_KINDS 3

/// The AVL tree node structure is exposed here to avoid pointer casting/arithmetics inside the library.
/// The user code is not expected to interact with this type except if advanced introspection is required.
struct UdpardTreeNode
{
    UdpardTreeNode* up;     ///< Do not access this field.
    UdpardTreeNode* lr[2];  ///< Left and right children of this node may be accessed for tree traversal.
    int8_t          bf;     ///< Do not access this field.
};

typedef struct
{
    uint8_t  version;
    uint8_t  priority;
    uint16_t source_node_id;
    uint16_t destination_node_id;
    uint16_t data_specifier;
    uint64_t transfer_id;
    uint32_t frame_index_eot;
    uint16_t _opaque;
    uint16_t cyphal_header_checksum;
} UdpardFrameHeader;

typedef struct
{
    UdpardFrameHeader udp_cyphal_header;
    size_t            payload_size;
    const void*       payload;
} UdpardFrame;

typedef struct
{
    /// The route specifier is defined by the 16 LSB of the IP address
    UdpardIPv4Addr source_route_specifier;
    UdpardIPv4Addr destination_route_specifier;
    /// For message transfers: the data specifier is the 16 LSB of the multicast group
    /// For service transfers: the data specifier is the UDP destination port number
    /// The service UDP port number for a request is determined by (SERVICE_BASE_PORT + service_id * 2)
    /// The service UDP port number for a response is determined by (SERVICE_BASE_PORT + service_id * 2 + 1)
    /// Note that message transfers will always use the same UDP port number (MESSAGE_BASE_PORT)
    UdpardUdpPortID data_specifier;
} UdpardSessionSpecifier;

/// A Cyphal transfer metadata (everything except the payload).
/// Per Specification, a transfer is represented on the wire as a non-empty set of transport frames (i.e., UDP frames).
/// The library is responsible for serializing transfers into transport frames when transmitting, and reassembling
/// transfers from an incoming stream of frames (possibly duplicated if redundant interfaces are used) during reception.
typedef struct
{
    /// Per the Specification, all frames belonging to a given transfer shall share the same priority level.
    /// If this is not the case, then this field contains the priority level of the last frame to arrive.
    UdpardPriority priority;

    UdpardTransferKind transfer_kind;

    /// Subject-ID for message publications; service-ID for service requests/responses.
    UdpardPortID port_id;

    /// For outgoing message transfers the value shall be UDPARD_NODE_ID_UNSET (otherwise the state is invalid).
    /// For outgoing service transfers this is the destination address (invalid if unset).
    /// For incoming non-anonymous transfers this is the node-ID of the origin.
    /// For incoming anonymous transfers the value is reported as UDPARD_NODE_ID_UNSET.
    UdpardNodeID remote_node_id;

    /// When responding to a service request, the response transfer SHALL have the same transfer-ID value as the
    /// request because the client will match the response with the request based on that.
    ///
    /// When publishing a message transfer, the value SHALL be one greater than the previous transfer under the same
    /// subject-ID; the initial value should be zero.
    ///
    /// When publishing a service request transfer, the value SHALL be one greater than the previous transfer under
    /// the same service-ID addressed to the same server node-ID; the initial value should be zero.
    ///
    /// The transfer-ID shall not overflow
    ///
    /// A simple and robust way of managing transfer-ID counters is to keep a separate static variable per subject-ID
    /// and per (service-ID, server-node-ID) pair.
    UdpardTransferID transfer_id;
} UdpardTransferMetadata;

/// Prioritized transmission queue that keeps UDP frames destined for transmission via one UDP interface.
/// Applications with redundant interfaces are expected to have one instance of this type per interface.
/// Applications that are not interested in transmission may have zero queues.
/// All operations (push, peek, pop) are O(log n); there is exactly one heap allocation per element.
/// API functions that work with this type are named "udpardTx*()", find them below.
typedef struct UdpardTxQueue
{
    /// The maximum number of frames this queue is allowed to contain. An attempt to push more will fail with an
    /// out-of-memory error even if the memory is not exhausted. This value can be changed by the user at any moment.
    /// The purpose of this limitation is to ensure that a blocked queue does not exhaust the heap memory.
    size_t capacity;

    /// The transport-layer maximum transmission unit (MTU). The value can be changed arbitrarily at any time between
    /// pushes. It defines the maximum number of data bytes per UDP data frame in outgoing transfers via this queue.
    ///
    /// Only the standard values should be used as recommended by the specification;
    /// otherwise, networking interoperability issues may arise. See recommended values UDPARD_MTU_*.
    ///
    /// Valid values are any valid UDP frame data length value not smaller than 8.
    /// Invalid values are treated as the nearest valid value. The default is the maximum valid value.
    size_t mtu_bytes;

    /// The number of frames that are currently contained in the queue, initially zero.
    /// Do not modify this field!
    size_t size;

    /// The root of the priority queue is NULL if the queue is empty. Do not modify this field!
    UdpardTreeNode* root;

    /// This field can be arbitrarily mutated by the user. It is never accessed by the library.
    /// Its purpose is to simplify integration with OOP interfaces.
    void* user_reference;
} UdpardTxQueue;

/// One frame stored in the transmission queue along with its metadata.
struct UdpardTxQueueItem
{
    /// Internal use only; do not access this field.
    UdpardTreeNode base;

    /// Points to the next frame in this transfer or NULL. This field is mostly intended for own needs of the library.
    /// Normally, the application would not use it because transfer frame ordering is orthogonal to global TX ordering.
    /// It can be useful though for pulling pending frames from the TX queue if at least one frame of their transfer
    /// failed to transmit; the idea is that if at least one frame is missing, the transfer will not be received by
    /// remote nodes anyway, so all its remaining frames can be dropped from the queue at once using udpardTxPop().
    UdpardTxQueueItem* next_in_transfer;

    /// This is the same value that is passed to udpardTxPush().
    /// Frames whose transmission deadline is in the past shall be dropped.
    UdpardMicrosecond tx_deadline_usec;

    /// We need some way to distinguish data and destination routing for each TxQueueItem. The SessionSepcifier gives us
    /// all of the information we need to do that.
    UdpardSessionSpecifier specifier;

    /// The actual UDP frame data.
    UdpardFrame frame;
};

/// Transfer subscription state. The application can register its interest in a particular kind of data exchanged
/// over the bus by creating such subscription objects. Frames that carry data for which there is no active
/// subscription will be silently dropped by the library. The entire RX pipeline is invariant to the number of
/// redundant UDP interfaces used.
///
/// SUBSCRIPTION INSTANCES SHALL NOT BE MOVED WHILE IN USE.
///
/// The memory footprint of a subscription is large. On a 32-bit platform it slightly exceeds half a KiB.
/// This is an intentional time-memory trade-off: use a large look-up table to ensure predictable temporal properties.
typedef struct UdpardRxSubscription
{
    UdpardTreeNode base;  ///< Read-only DO NOT MODIFY THIS

    UdpardMicrosecond transfer_id_timeout_usec;
    size_t            extent;   ///< Read-only DO NOT MODIFY THIS
    UdpardPortID      port_id;  ///< Read-only DO NOT MODIFY THIS

    /// This field can be arbitrarily mutated by the user. It is never accessed by the library.
    /// Its purpose is to simplify integration with OOP interfaces.
    void* user_reference;

    /// TODO - We may need to limit UDPARD_NODE_ID_MAX so that we aren't allocating 65 KiB for empty sessions...
    /// TODO - or we need to determine some other method of creating sessions while retaining O(1)

    /// The current architecture is an acceptable middle ground between worst-case execution time and memory
    /// consumption. Instead of statically pre-allocating a dedicated RX session for each remote node-ID here in
    /// this table, we only keep pointers, which are NULL by default, populating a new RX session dynamically
    /// on an ad-hoc basis when we first receive a transfer from that node. This is O(1) because our memory
    /// allocation routines are assumed to be O(1) and we make at most one allocation per remote node.
    ///
    /// A more predictable and simpler approach is to pre-allocate states here statically instead of keeping
    /// just pointers, but it would push the size of this instance from about 0.5 KiB to ~3 KiB for a typical 32-bit
    /// system. Since this is a general-purpose library, we have to pick a middle ground so we use the more complex
    /// but more memory-efficient approach.
    struct UdpardInternalRxSession* sessions[UDPARD_NODE_ID_MAX + 1U];  ///< Read-only DO NOT MODIFY THIS
} UdpardRxSubscription;

/// Reassembled incoming transfer returned by udpardRxAccept().
typedef struct UdpardRxTransfer
{
    UdpardTransferMetadata metadata;

    /// The timestamp of the first received UDP frame of this transfer.
    /// The time system may be arbitrary as long as the clock is monotonic (steady).
    UdpardMicrosecond timestamp_usec;

    /// If the payload is empty (payload_size = 0), the payload pointer may be NULL.
    /// The application is required to deallocate the payload buffer after the transfer is processed.
    size_t payload_size;
    void*  payload;
} UdpardRxTransfer;

/// A pointer to the memory allocation function. The semantics are similar to malloc():
///     - The returned pointer shall point to an uninitialized block of memory that is at least "amount" bytes large.
///     - If there is not enough memory, the returned pointer shall be NULL.
///     - The memory shall be aligned at least at max_align_t.
///     - The execution time should be constant (O(1)).
///     - The worst-case memory fragmentation should be bounded and easily predictable.
/// If the standard dynamic memory manager of the target platform does not satisfy the above requirements,
/// consider using O1Heap: https://github.com/pavel-kirienko/o1heap.
typedef void* (*UdpardMemoryAllocate)(UdpardInstance* ins, size_t amount);

/// The counterpart of the above -- this function is invoked to return previously allocated memory to the allocator.
/// The semantics are similar to free():
///     - The pointer was previously returned by the allocation function.
///     - The pointer may be NULL, in which case the function shall have no effect.
///     - The execution time should be constant (O(1)).
typedef void (*UdpardMemoryFree)(UdpardInstance* ins, void* pointer);

/// This is the core structure that keeps all of the states and allocated resources of the library instance.
struct UdpardInstance
{
    /// User pointer that can link this instance with other objects.
    /// This field can be changed arbitrarily, the library does not access it after initialization.
    /// The default value is NULL.
    void* user_reference;

    /// The node-ID of the local node.
    /// Per the Cyphal Specification, the node-ID should not be assigned more than once.
    /// Invalid values are treated as UDPARD_NODE_ID_UNSET. The default value is UDPARD_NODE_ID_UNSET.
    UdpardNodeID   node_id;
    UdpardIPv4Addr local_ip_addr;

    /// Dynamic memory management callbacks. See their type documentation for details.
    /// They SHALL be valid function pointers at all times.
    /// The time complexity models given in the API documentation are made on the assumption that the memory management
    /// functions have constant complexity O(1).
    ///
    /// The following API functions may allocate memory:   udpardRxAccept(), udpardTxPush().
    /// The following API functions may deallocate memory: udpardRxAccept(), udpardRxSubscribe(), udpardRxUnsubscribe().
    /// The exact memory requirement and usage model is specified for each function in its documentation.
    UdpardMemoryAllocate memory_allocate;
    UdpardMemoryFree     memory_free;

    /// Read-only DO NOT MODIFY THIS
    UdpardTreeNode* rx_subscriptions[UDPARD_NUM_TRANSFER_KINDS];
};

/// TODO - Make a UDP acceptance filter based on the UdpardFrameHeader

/// Construct a new library instance.
/// The default values will be assigned as specified in the structure field documentation.
/// If any of the pointers are NULL, the behavior is undefined.
///
/// The instance does not hold any resources itself except for the allocated memory.
/// To safely discard it, simply remove all existing subscriptions, and don't forget about the TX queues.
///
/// The time complexity is constant. This function does not invoke the dynamic memory manager.
UdpardInstance udpardInit(const UdpardMemoryAllocate memory_allocate, const UdpardMemoryFree memory_free);

/// Construct a new transmission queue instance with the specified values for capacity and mtu_bytes.
/// No memory allocation is going to take place until the queue is actually pushed to.
/// Applications are expected to have one instance of this type per redundant interface.
///
/// The instance does not hold any resources itself except for the allocated memory.
/// To safely discard it, simply pop all items from the queue.
///
/// The time complexity is constant. This function does not invoke the dynamic memory manager.
UdpardTxQueue udpardTxInit(const size_t capacity, const size_t mtu_bytes);

/// This function serializes a transfer into a sequence of transport frames and inserts them into the prioritized
/// transmission queue at the appropriate position. Afterwards, the application is supposed to take the enqueued frames
/// from the transmission queue using the function udpardTxPeek() and transmit them. Each transmitted (or otherwise
/// discarded, e.g., due to timeout) frame should be removed from the queue using udpardTxPop(). The queue is
/// prioritized following the normal UDP frame arbitration rules to avoid the inner priority inversion. The transfer
/// payload will be copied into the transmission queue so that the lifetime of the frames is not related to the
/// lifetime of the input payload buffer.
///
/// The MTU of the generated frames is dependent on the value of the MTU setting at the time when this function
/// is invoked. The MTU setting can be changed arbitrarily between invocations.
///
/// The tx_deadline_usec will be used to populate the timestamp values of the resulting transport
/// frames (so all frames will have the same timestamp value). This feature is intended to facilitate transmission
/// deadline tracking, i.e., aborting frames that could not be transmitted before the specified deadline.
/// Therefore, normally, the timestamp value should be in the future.
/// The library itself, however, does not use or check this value in any way, so it can be zero if not needed.
///
/// The function returns the number of frames enqueued into the prioritized TX queue (which is always a positive
/// number) in case of success (so that the application can track the number of items in the TX queue if necessary).
/// In case of failure, the function returns a negated error code: either invalid argument or out-of-memory.
///
/// An invalid argument error may be returned in the following cases:
///     - Any of the input arguments are NULL.
///     - The remote node-ID is not UDPARD_NODE_ID_UNSET and the transfer is a message transfer.
///     - The remote node-ID is above UDPARD_NODE_ID_MAX and the transfer is a service transfer.
///     - The priority, subject-ID, or service-ID exceed their respective maximums.
///     - The transfer kind is invalid.
///     - The payload pointer is NULL while the payload size is nonzero.
///     - The local node is anonymous and a message transfer is requested that requires a multi-frame transfer.
///     - The local node is anonymous and a service transfer is requested.
/// The following cases are handled without raising an invalid argument error:
///     - If the transfer-ID is above the maximum, the excessive bits are silently masked away
///       (i.e., the modulo is computed automatically, so the caller doesn't have to bother).
///
/// An out-of-memory error is returned if a TX frame could not be allocated due to the memory being exhausted,
/// or if the capacity of the queue would be exhausted by this operation. In such cases, all frames allocated for
/// this transfer (if any) will be deallocated automatically. In other words, either all frames of the transfer are
/// enqueued successfully, or none are.
///
/// The time complexity is O(p + log e), where p is the amount of payload in the transfer, and e is the number of
/// frames already enqueued in the transmission queue.
///
/// The memory allocation requirement is one allocation per transport frame. A single-frame transfer takes one
/// allocation; a multi-frame transfer of N frames takes N allocations. The size of each allocation is
/// (sizeof(udpardTxQueueItem) + MTU).
int32_t udpardTxPush(UdpardTxQueue* const                que,
                     UdpardInstance* const               ins,
                     const UdpardMicrosecond             tx_deadline_usec,
                     const UdpardTransferMetadata* const metadata,
                     const size_t                        payload_size,
                     const void* const                   payload);

/// This function accesses the top element of the prioritized transmission queue. The queue itself is not modified
/// (i.e., the accessed element is not removed). The application should invoke this function to collect the transport
/// frames of serialized transfers pushed into the prioritized transmission queue by udpardTxPush().
///
/// The timestamp values of returned frames are initialized with tx_deadline_usec from udpardTxPush().
/// Timestamps are used to specify the transmission deadline. It is up to the application and/or the media layer
/// to implement the discardment of timed-out transport frames. The library does not check it, so a frame that is
/// already timed out may be returned here.
///
/// If the queue is empty or if the argument is NULL, the returned value is NULL.
///
/// If the queue is non-empty, the returned value is a pointer to its top element (i.e., the next frame to transmit).
/// The returned pointer points to an object allocated in the dynamic storage; it should be eventually freed by the
/// application by calling udpardInstance::memory_free(). The memory shall not be freed before the entry is removed
/// from the queue by calling udpardTxPop(); this is because until udpardTxPop() is executed, the library retains
/// ownership of the object. The pointer retains validity until explicitly freed by the application; in other words,
/// calling udpardTxPop() does not invalidate the object.
///
/// The payload buffer is located shortly after the object itself, in the same memory fragment. The application shall
/// not attempt to free it.
///
/// The time complexity is logarithmic of the queue size. This function does not invoke the dynamic memory manager.
const UdpardTxQueueItem* udpardTxPeek(const UdpardTxQueue* const que);

/// This function transfers the ownership of the specified element of the prioritized transmission queue from the queue
/// to the application. The element does not necessarily need to be the top one -- it is safe to dequeue any element.
/// The element is dequeued but not invalidated; it is the responsibility of the application to deallocate the
/// memory used by the object later. The memory SHALL NOT be deallocated UNTIL this function is invoked.
/// The function returns the same pointer that it is given except that it becomes mutable.
///
/// If any of the arguments are NULL, the function has no effect and returns NULL.
///
/// The time complexity is logarithmic of the queue size. This function does not invoke the dynamic memory manager.
UdpardTxQueueItem* udpardTxPop(UdpardTxQueue* const que, const UdpardTxQueueItem* const item);

/// This function implements the transfer reassembly logic. It accepts a transport frame from any of the redundant
/// interfaces, locates the appropriate subscription state, and, if found, updates it. If the frame completed a
/// transfer, the return value is 1 (one) and the out_transfer pointer is populated with the parameters of the
/// newly reassembled transfer. The transfer reassembly logic is defined in the Cyphal specification.
///
/// The MTU of the accepted frame can be arbitrary; that is, any MTU is accepted. The DLC validity is irrelevant.
///
/// Any value of redundant_transport_index is accepted; that is, up to 256 redundant transports are supported.
/// The index of the transport from which the transfer is accepted is always the same as redundant_transport_index
/// of the current invocation, so the application can always determine which transport has delivered the transfer.
///
/// Upon return, the out_subscription pointer will point to the instance of udpardRxSubscription that accepted this
/// frame; if no matching subscription exists (i.e., frame discarded), the pointer will be NULL.
/// If this information is not relevant, set out_subscription to NULL.
/// The purpose of this argument is to allow integration with OOP adapters built on top of libudpard; see also the
/// user_reference provided in udpardRxSubscription.
///
/// The function invokes the dynamic memory manager in the following cases only:
///
///     1. New memory for a session state object is allocated when a new session is initiated.
///        This event occurs when a transport frame that matches a known subscription is received from a node that
///        did not emit matching frames since the subscription was created.
///        Once a new session is created, it is not destroyed until the subscription is terminated by invoking
///        udpardRxUnsubscribe(). The number of sessions is bounded and the bound is low (at most the number of nodes
///        in the network minus one), also the size of a session instance is very small, so the removal is unnecessary.
///        Real-time networks typically do not change their configuration at runtime, so it is possible to reduce
///        the time complexity by never deallocating sessions.
///        The size of a session instance is at most 48 bytes on any conventional platform (typically much smaller).
///
///     2. New memory for the transfer payload buffer is allocated when a new transfer is initiated, unless the buffer
///        was already allocated at the time.
///        This event occurs when a transport frame that matches a known subscription is received and it begins a
///        new transfer (that is, the start-of-frame flag is set and it is not a duplicate).
///        The amount of the allocated memory equals the extent as configured via udpardRxSubscribe(); please read
///        its documentation for further information about the extent and related edge cases.
///        The worst case occurs when every node on the bus initiates a multi-frame transfer for which there is a
///        matching subscription: in this case, the library will allocate number_of_nodes allocations, where each
///        allocation is the same size as the configured extent.
///
///     3. Memory allocated for the transfer payload buffer may be deallocated at the discretion of the library.
///        This operation does not increase the worst case execution time and does not improve the worst case memory
///        consumption, so a deterministic application need not consider this behavior in the resource analysis.
///        This behavior is implemented for the benefit of applications where rigorous characterization is unnecessary.
///
/// The worst case dynamic memory consumption per subscription is:
///
///     (sizeof(session instance) + extent) * number_of_nodes
///
/// Where sizeof(session instance) and extent are defined above, and number_of_nodes is the number of remote
/// nodes emitting transfers that match the subscription (which cannot exceed (UDPARD_NODE_ID_MAX-1) by design).
/// If the dynamic memory pool is sized correctly, the application is guaranteed to never encounter an
/// out-of-memory (OOM) error at runtime. The actual size of the dynamic memory pool is typically larger;
/// for a detailed treatment of the problem and the related theory please refer to the documentation of O1Heap --
/// a deterministic memory allocator for hard real-time embedded systems.
///
/// The time complexity is O(p + log n) where n is the number of subject-IDs or service-IDs subscribed to by the
/// application, depending on the transfer kind of the supplied frame, and p is the amount of payload in the received
/// frame (because it will be copied into an internal contiguous buffer). Observe that the time complexity is
/// invariant to the network configuration (such as the number of online nodes) -- this is a very important
/// design guarantee for real-time applications because the execution time is dependent only on the number of
/// active subscriptions for a given transfer kind, and the MTU, both of which are easy to predict and account for.
/// Excepting the subscription search and the payload data copying, the entire RX pipeline contains neither loops
/// nor recursion.
/// Misaddressed and malformed frames are discarded in constant time.
///
/// The function returns 1 (one) if the new frame completed a transfer. In this case, the details of the transfer
/// are stored into out_transfer, and the transfer payload buffer ownership is passed to that object. The lifetime
/// of the resulting transfer object is not related to the lifetime of the input transport frame (that is, even if
/// it is a single-frame transfer, its payload is copied out into a new dynamically allocated buffer storage).
/// If the extent is zero, the payload pointer may be NULL, since there is no data to store and so a
/// buffer is not needed. The application is responsible for deallocating the payload buffer when the processing
/// is done by invoking memory_free on the transfer payload pointer.
///
/// The function returns a negated out-of-memory error if it was unable to allocate dynamic memory.
///
/// The function does nothing and returns a negated invalid argument error immediately if any condition is true:
///     - Any of the input arguments that are pointers are NULL.
///     - The payload pointer of the input frame is NULL while its size is non-zero.
///     - The UDP ID of the input frame is not less than 2**29=0x20000000.
///
/// The function returns zero if any of the following conditions are true (the general policy is that protocol
/// errors are not escalated because they do not construe a node-local error):
///     - The received frame is not a valid Cyphal/UDP transport frame.
///     - The received frame is a valid Cyphal/UDP transport frame, but there is no matching subscription,
///       the frame did not complete a transfer, the frame forms an invalid frame sequence, the frame is a duplicate,
///       the frame is unicast to a different node (address mismatch).
int8_t udpardRxAccept(UdpardInstance* const         ins,
                      const UdpardMicrosecond       timestamp_usec,
                      UdpardFrame* const            frame,
                      const uint8_t                 redundant_transport_index,
                      UdpardRxTransfer* const       out_transfer,
                      UdpardRxSubscription** const  out_subscription);

/// This function creates a new subscription, allowing the application to register its interest in a particular
/// category of transfers. The library will reject all transport frames for which there is no active subscription.
/// The reference out_subscription shall retain validity until the subscription is terminated (the referred object
/// cannot be moved or destroyed).
///
/// If such subscription already exists, it will be removed first as if udpardRxUnsubscribe() was
/// invoked by the application, and then re-created anew with the new parameters.
///
/// The extent defines the size of the transfer payload memory buffer; or, in other words, the maximum possible size
/// of received objects, considering also possible future versions with new fields. It is safe to pick larger values.
/// Note well that the extent is not the same thing as the maximum size of the object, it is usually larger!
/// Transfers that carry payloads that exceed the specified extent will be accepted anyway but the excess payload
/// will be truncated away, as mandated by the Specification. The transfer CRC is always validated regardless of
/// whether its payload is truncated.
///
/// The default transfer-ID timeout value is defined as UDPARD_DEFAULT_TRANSFER_ID_TIMEOUT_USEC; use it if not sure.
/// The redundant transport fail-over timeout (if redundant transports are used) is the same as the transfer-ID timeout.
/// It may be reduced in a future release of the library, but it will not affect the backward compatibility.
///
/// The return value is 1 if a new subscription has been created as requested.
/// The return value is 0 if such subscription existed at the time the function was invoked. In this case,
/// the existing subscription is terminated and then a new one is created in its place. Pending transfers may be lost.
/// The return value is a negated invalid argument error if any of the input arguments are invalid.
///
/// The time complexity is logarithmic from the number of current subscriptions under the specified transfer kind.
/// This function does not allocate new memory. The function may deallocate memory if such subscription already
/// existed; the deallocation behavior is specified in the documentation for udpardRxUnsubscribe().
///
/// Subscription instances have large look-up tables to ensure that the temporal properties of the algorithms are
/// invariant to the network configuration (i.e., a node that is validated on a network containing one other node
/// will provably perform identically on a network that contains X nodes). This is a conscious time-memory trade-off.
int8_t udpardRxSubscribe(UdpardInstance* const       ins,
                         const UdpardTransferKind    transfer_kind,
                         const UdpardPortID          port_id,
                         const size_t                extent,
                         const UdpardMicrosecond     transfer_id_timeout_usec,
                         UdpardRxSubscription* const out_subscription);

/// This function reverses the effect of udpardRxSubscribe().
/// If the subscription is found, all its memory is de-allocated (session states and payload buffers); to determine
/// the amount of memory freed, please refer to the memory allocation requirement model of udpardRxAccept().
///
/// The return value is 1 if such subscription existed (and, therefore, it was removed).
/// The return value is 0 if such subscription does not exist. In this case, the function has no effect.
/// The return value is a negated invalid argument error if any of the input arguments are invalid.
///
/// The time complexity is logarithmic from the number of current subscriptions under the specified transfer kind.
/// This function does not allocate new memory.
int8_t udpardRxUnsubscribe(UdpardInstance* const    ins,
                           const UdpardTransferKind transfer_kind,
                           const UdpardPortID       port_id);
#ifdef __cplusplus
}
#endif
#endif
