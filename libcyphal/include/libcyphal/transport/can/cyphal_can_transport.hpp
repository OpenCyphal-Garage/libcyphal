/// @copyright Copyright Amazon.com Inc. and its affiliates. All Rights Reserved.
/// @file
/// The internal implementation of the cyphal::can::Transport as a template.

#ifndef LIBCYPHAL_TRANSPORT_CAN_CYPHAL_CAN_TRANSPORT_HPP_INCLUDED
#define LIBCYPHAL_TRANSPORT_CAN_CYPHAL_CAN_TRANSPORT_HPP_INCLUDED

#include <cstdint>
#include <limits>
#include <type_traits>
#include <array>
#include <canard.h>
#include "libcyphal/transport.hpp"
#include "libcyphal/media/can/filter.hpp"
#include "libcyphal/media/can/frame.hpp"
#include "libcyphal/media/can/identifier.hpp"
#include "libcyphal/transport/id_types.hpp"
#include "libcyphal/transport/listener.hpp"
#include "libcyphal/transport/metadata.hpp"
#include "libcyphal/transport/can/interface.hpp"
#include "libcyphal/transport/can/transport.hpp"
#include "libcyphal/types/status.hpp"
#include "libcyphal/types/time.hpp"

#include "cetl/variable_length_array.hpp"
#include "cetl/pf17/memory_resource.hpp"

namespace libcyphal
{
namespace transport
{
namespace can
{
using CanardStatus = std::int32_t;

/// @todo I heard somewhere this should be 0 and not 255... need to double check this value
static constexpr CanardNodeID     AnonymousNodeID   = CANARD_NODE_ID_UNSET;  //!< Canard standard anonymous node ID
static constexpr CanardTransferID InitialTransferID = 0;  //!< Transfer IDs for new transactions start at 0
/// @todo pick a value that is not arbitrarily large -> Transfer payload maximum size, overflowing data will be
/// truncated
static constexpr std::size_t MaxMessageExtent = 1024;

namespace canard
{
// Canard CAN frame ID bitfields
// CAN frame ID field encoding specifications provided by https://opencyphal.org/specification section 4.2.1

/// Transfer priority
static constexpr std::uint32_t FrameIDPriorityMask     = 0x7;
static constexpr std::size_t   FrameIDPriorityPosition = 26;

/// Service ID
static constexpr std::uint32_t FrameIDServiceIdMask     = 0x1FF;
static constexpr std::size_t   FrameIDServiceIdPosition = 14;

/// Subject ID
static constexpr std::uint32_t FrameIDSubjectIdMask     = 0x1FFF;
static constexpr std::size_t   FrameIDSubjectIdPosition = 8;

/// Node ID. Position within the CAN frame ID is dependent on the kind of transfer (message or service)
static constexpr std::uint32_t FrameIDNodeIdMask = 0x7F;  // Node ID location changes based on the type of transfer

/// Request bit. Set when the CAN frame is part of a service request transfer.
static constexpr std::size_t   FrameRequestBitPosition = 24;
static constexpr std::uint32_t FrameRequestBit         = 0x1 << FrameRequestBitPosition;

/// Service bit. Set when the CAN frame is part of a service message transfer (request or response).
static constexpr std::size_t   FrameServiceBitPosition = 25;
static constexpr std::uint32_t FrameServiceBit =
    0x1 << FrameServiceBitPosition;  // Bit is set when CAN frame is for service (request or response)

}  // namespace canard

/// Cyphal transport layer implementation for CAN
class CyphalCANTransport final : public Transport, public Interface::Receiver
{
public:
    // Cyphal over CAN always encodes transfers in to extended frames
    static constexpr bool UsesExtendedFrames = true;   //!< Cyphal and Canard use extended frames only
    static constexpr bool UsesStandardFrames = false;  //!< Cyphal and Canard use extended frames only

    // Sets the maximum number of possible transfers that an instance of cyphal::can::Transport can manage
    static constexpr std::size_t MaxNumberOfBroadcasts =
        LIBCYPHAL_TRANSPORT_MAX_BROADCASTS;  //!< Maximum number of broadcast message types that an instance can publish
    static constexpr std::size_t MaxNumberOfSubscriptions =
        LIBCYPHAL_TRANSPORT_MAX_SUBSCRIPTIONS;  //!< Maximum number of broadcast subscriptions that an instance can
                                                //!< receive
    static constexpr std::size_t MaxNumberOfResponses =
        LIBCYPHAL_TRANSPORT_MAX_RESPONSES;  //!< Maximum number of response message types that an instance can handle
    static constexpr std::size_t MaxNumberOfRequests =
        LIBCYPHAL_TRANSPORT_MAX_REQUESTS;  //!< Maximum number of request message types that an instance can handle

    // Maximum number of subscription records that an instance can manage, cannot be 0
    static constexpr std::size_t MaxNumberOfSubscriptionRecords =
        MaxNumberOfSubscriptions + MaxNumberOfResponses + MaxNumberOfRequests;
    static_assert(MaxNumberOfSubscriptionRecords, "kMaxNumberOfSubscriptions, Responses, or Requests must be nonzero");

    /// Constructor
    /// @param[in] transport_index The index associated with this transport
    /// @param[in] primary_bus Pointer to primary CAN driver interface
    /// @param[in] backup_bus Pointer to backup CAN driver interface
    /// @param[in] timer ABSP timer interface
    /// @param[in] resource the memory resource for the transport.
    CyphalCANTransport(TransportID                       transport_index,
                       Interface&                        primary_bus,
                       Interface*                        backup_bus,
                       const time::Timer&                timer,
                       cetl::pf17::pmr::memory_resource* resource,
                       CanardMemoryAllocate              allocator,
                       CanardMemoryFree                  releaser)
        : transport_id_{transport_index}
        , cleanup_initiated_{false}
        , fn_canard_mem_allocate_{allocator}
        , fn_canard_mem_free_{releaser}
        , timer_{timer}
        , primary_bus_{primary_bus}
        , backup_bus_{backup_bus}
        , consolidated_filter_{std::numeric_limits<std::uint32_t>::max(), 0}
        , resource_{resource}
        , canard_{canardInit(allocator, releaser)}
        , canard_tx_fifo_{canardTxInit(TXFIFOSize, MTUSize)}
    {
        canard_.user_reference = resource_;
    }

    CyphalCANTransport(const CyphalCANTransport&)            = delete;
    CyphalCANTransport(CyphalCANTransport&&)                 = delete;
    CyphalCANTransport& operator=(const CyphalCANTransport&) = delete;
    CyphalCANTransport& operator=(CyphalCANTransport&&)      = delete;

    virtual ~CyphalCANTransport() noexcept {};

    /// @todo Remove this and replace with common *ard metadata types
    CanardTransferKind libcyphalToCanardTransferKind(TransferKind kind)
    {
        return static_cast<CanardTransferKind>(kind);
    }

    /// @todo Remove this and replace with common *ard metadata types
    CanardPriority libcyphalToCanardPriority(TransferPriority priority)
    {
        return static_cast<CanardPriority>(priority);
    }

    /// @todo Remove this and replace with common *ard metadata types
    TransferKind canardToLibcyphalTransferKind(CanardTransferKind kind)
    {
        return static_cast<TransferKind>((kind));
    }

    /// @todo Remove this and replace with common *ard metadata types
    TransferPriority canardToLibcyphalPriority(CanardPriority priority)
    {
        return static_cast<TransferPriority>(priority);
    }

    /// @brief Unsubscribes for active records, removes from TX queue, and deallocates
    Status cleanup() override
    {
        Status ret{Status()};
        if (!cleanup_initiated_)
        {
            // Unsubscribe from all subscription records
            for (size_t i = 0; i < MaxNumberOfSubscriptionRecords; i++)
            {
                CanardRxSubscription& record = subscription_records_[i];

                // Check activation status of the subscription record via the user reference
                if (record.user_reference)
                {
                    // Record is active, unsubscribe
                    // Transfer type enum value is cached in the record's 'user_reference' field, which is void*, so we
                    // need to reinterpret_cast() that to a uintptr_t before it can be static cast to a uint
                    std::uintptr_t transfer_kind_ref = reinterpret_cast<std::uintptr_t>(record.user_reference);
                    TransferKind   transfer_kind     = static_cast<TransferKind>(transfer_kind_ref);
                    canardRxUnsubscribe(&canard_, libcyphalToCanardTransferKind(transfer_kind), record.port_id);

                    // Clear subscription record to deactivate
                    std::memset(&record, 0, sizeof(CanardRxSubscription));
                }
                else
                {
                    // First inactive record, the rest are as well
                    // Break early
                    break;
                }
            }
        }
        return ret;
    }

    /// @brief Sets the node ID for this transport
    /// @param[in] node_id The new node ID
    void setNodeID(NodeID node_id) noexcept
    {
        canard_.node_id = static_cast<CanardNodeID>(node_id);
    }

    /// @brief Initializes and verifies all input variables
    /// @return Status of proper initialization
    Status initialize() override
    {
        if ((nullptr == fn_canard_mem_allocate_) || (nullptr == fn_canard_mem_free_))
        {
            return Status(ResultCode::Invalid, CauseCode::Parameter);
        }
        return ResultCode::Success;
    }

    /// @brief retrieves port ID given a CAN frame
    /// @param[in] frame CAN frame
    PortID getPortId(const CanardFrame& frame) noexcept
    {
        return (frame.extended_can_id >> canard::FrameIDSubjectIdPosition) & canard::FrameIDSubjectIdMask;
    }

    /// @brief Retrieves Transport ID
    TransportID getTransportID() noexcept
    {
        return transport_id_;
    }

    /// @brief Retrieves Transfer kind based on CAN Frame
    /// @param[in] frame CAN frame
    TransferKind getTransferKind(const CanardFrame& frame) noexcept
    {
        bool transfer_is_service = ((frame.extended_can_id & canard::FrameServiceBit) > 0);
        bool transfer_is_request = ((frame.extended_can_id & canard::FrameRequestBit) > 0);

        TransferKind transfer_kind = TransferKindMessage;
        if (transfer_is_service)
        {
            if (transfer_is_request)
            {
                transfer_kind = TransferKindRequest;
            }
            else
            {
                transfer_kind = TransferKindResponse;
            }
        }

        return transfer_kind;
    }

    /// @brief Transmit a serialized message with the subjectID
    /// @param[in] subject_id the subject id of the message
    /// @param[in] msg The read only reference to the message information.
    /// @retval Success - Message transmitted
    /// @retval Invalid - No record found for response or trying to broadcast anonymously
    /// @retval Failure - Could not transmit the message.
    Status broadcast(PortID subject_id, const Message& msg)
    {
        TxMetadata metadata{};
        metadata.port_id        = subject_id;
        metadata.kind           = TransferKindMessage;
        metadata.priority       = PriorityNominal;
        metadata.remote_node_id = AnonymousNodeID;
        return transmit(metadata, msg);
    }

    /// @brief Allows a transport to transmit a serialized broadcast
    /// @param[in] tx_metadata the metadata of the message
    /// @param[in] msg The read only reference to the message information.
    /// @retval Success - Message transmitted
    /// @retval Invalid - No record found for response or trying to broadcast anonymously
    /// @retval Failure - Could not transmit the message.
    Status transmit(TxMetadata tx_metadata, const Message& msg) override
    {
        if (tx_metadata.remote_node_id > std::numeric_limits<CanardNodeID>::max())
        {
            // Remote node ID is out of range
            return Status(ResultCode::Invalid, CauseCode::Parameter);
        }
        // Get message publication record (transfer metadata) for this subject ID
        // Broadcast message records do not utilize the node ID field, leave it unset
        CanardTransferMetadata* record = getPublicationRecord(publication_records_,
                                                              tx_metadata.port_id,
                                                              static_cast<CanardNodeID>(tx_metadata.remote_node_id));
        if (record == nullptr)
        {
            // Should not be here, a lack of records for this response means that the transport was not informed of
            // this broadcast via 'RegisterBroadcast()'
            return Status(ResultCode::NotInitialized, CauseCode::Session);
        }

        if (tx_metadata.kind == TransferKindResponse)
        {
            if (record->remote_node_id == AnonymousNodeID)
            {
                // An anonymous 'remote_node_id' field for this response record means that the transport was informed of
                // this response, but the predicating request has not been received yet. Thus, the response record is
                // still inactive and the transfer will likely be ignored by the other devices on the bus.
                return Status(ResultCode::NotReady, CauseCode::Resource);
            }
        }
        else
        {
            // Activate the record if it has not been already, otherwise is harmless reassignment of the same value
            record->remote_node_id = static_cast<CanardNodeID>(tx_metadata.remote_node_id);
        }

        Status publication_status = publishTransfer(*record, msg);
        if (publication_status.isSuccess())
        {
            // Increment the transfer ID for the next broadcast
            // Ensure it stays within the accepted range
            record->transfer_id = static_cast<CanardTransferID>(
                (record->transfer_id + 1U) % CANARD_TRANSFER_ID_MAX);  // TODO: Do responses increment this?
        }

        return publication_status;
    }

    /// @brief Called by the Interface when an CAN Frame is available
    /// @note Implements libcyphal::transport::Receiver::onReceive
    /// @param[in] frame The CAN frame
    void onReceive(const media::can::extended::Frame& frame) override
    {
        // Copy frame over to Canard compatible struct
        CanardFrame canard_frame{frame.id_.getID(), frame.dlc_.toLength(), frame.data_};

        // Pass frame through acceptance
        CanardRxTransfer             received;  // Incoming transfer
        CanardRxSubscription** const subscription =
            nullptr;  // Optional, unused reference to the subscription which accepts the incoming frame
        time::Monotonic::MicrosecondType now = timer_.getTimeInUs().toMicrosecond();
        int8_t                           accept_status =
            canardRxAccept(&canard_, now, &canard_frame, current_rx_bus_index_, &received, subscription);

        // If `accept_status` is 1, a new transfer is available for processing.
        // If it is 0, a transfer may still be in progress or the frame was discarded by Canard. It is not a failure or
        // success. If it is negative, an error occurred while accepting the new frame

        // Check 'accept_status' manually to not confuse 0 for success
        if (accept_status == 1)
        {
            // Frame has been accepted, new transfer available
            // Store transfer payload in to serialized message
            Message msg(static_cast<uint8_t*>(received.payload), received.payload_size);

            // Determine if message or request, then call the appropriate callback
            if (static_cast<unsigned char>(received.metadata.transfer_kind) ==
                static_cast<unsigned char>(TransferKind::TransferKindRequest))
            {
                // Incoming transfer is a service request
                // Update response record (transfer metadata) associated with this request
                CanardTransferMetadata* record = getPublicationRecord(publication_records_,
                                                                      received.metadata.port_id,
                                                                      received.metadata.remote_node_id);
                if (record != nullptr)
                {
                    // Cache the request's transfer and node ID within the response record
                    record->transfer_id    = received.metadata.transfer_id;
                    record->remote_node_id = received.metadata.remote_node_id;
                    // ^ Activates record if this is the first accepted request and is otherwise just writing the same
                    // value to its 'remote_node_id' field
                }
            }
            RxMetadata rx_metadata;
            rx_metadata.kind           = canardToLibcyphalTransferKind(received.metadata.transfer_kind);
            rx_metadata.priority       = canardToLibcyphalPriority(received.metadata.priority);
            rx_metadata.port_id        = static_cast<PortID>(received.metadata.port_id);
            rx_metadata.remote_node_id = static_cast<NodeID>(received.metadata.remote_node_id);
            rx_metadata.transfer_id    = static_cast<TransferID>(received.metadata.transfer_id);
            rx_metadata.timestamp_us   = received.timestamp_usec;
            current_listener_->onReceive(rx_metadata, msg);

            // Deallocate transfer payload, nullptr is handled gracefully
            fn_canard_mem_free_(&canard_, received.payload);
        }
    }

    /// Called by clients in order to processes incoming CAN Frames
    /// @param[in] listener Object that provides callbacks to the application layer to trigger from the transport
    /// @note The implement will invoke the listener with the appropriately typed Frames.
    ///     1. The user defines a Listener by implementing the Listener APIs. For example, if the user wants custom
    ///        behavior after receiving a broadcast message, onReceiveBroadcast could perhaps deserialize and print
    ///        the message as an example
    ///     2. The user defines CAN Interfaces by implementing libcyphal::transport::can::transport.hpp. This is
    ///        considered the primary/secondary "buses".
    ///     3. The user application or libcyphal Application layer calls processIncomingTransfers(<Listener>)
    ///     4. CyphaCANTransport triggers a CAN Interface call to the primary/secondary bus and calls
    ///        processIncomignFrames
    ///     5. This calls whatever OS level APIs are available to receive CAN packets
    ///     6. After the message is received and canard is notified, the listener's onReceiveBroadcast API is called
    /// @note The lifecycle of the Listener is maintained by the application/application layer and not this class
    /// @note Multiple calls to this API are needed for large payloads until the EOT flag in the header is set
    /// indicating
    ///       the transfer is complete and thus sending the buffer back to the user. Use caution as very large payloads
    ///       can take a while before downloading the full buffer. It is up to the user whether to block (for example
    ///       looping back to back waiting for the buffer) or download frame by frame per loop cycle.
    /// @returns The state of the Interface after processing inputs.
    /// @retval result_e::SUCCESS
    /// @retval Other values indicate underlying failures from the Driver.
    Status processIncomingTransfers(Listener& listener) override
    {
        if (current_listener_ != nullptr)
        {
            //  If this Transport is already in use by another listener, return BUSY status
            return Status(ResultCode::Busy, CauseCode::Session);
        }

        // Otherwise cache the provided listener reference and continue
        current_listener_ = &listener;

        BusStatus bus_status;
        current_rx_bus_index_ = Primary;
        bus_status.primary    = primary_bus_.processIncomingFrames(*this);

        if (backup_bus_ != nullptr)
        {
            // Set the current receiving bus index to backup then process incoming frames
            current_rx_bus_index_ = Backup;
            bus_status.backup     = backup_bus_->processIncomingFrames(*this);
        }
        else
        {
            bus_status.backup = Status(ResultCode::NotAvailable, CauseCode::Resource);
        }

        // Clear current listener to make it available for the next call to this method
        current_listener_ = nullptr;

        // Compare and return driver statuses
        // If either bus successfully processed incoming frames, return the first success.
        // Otherwise, return why the primary bus failed to receive incoming frames.
        if (bus_status.primary.isSuccess())
        {
            return bus_status.primary;
        }

        if (bus_status.backup.isSuccess())
        {
            return bus_status.backup;
        }

        return bus_status.primary;
    }

    /// @brief Creates a publication record to hold the metadata associated with the transfer
    /// @param[in] port_id PortID of the transfer (subject id / service id)
    /// @param[in] transfer_kind Type of transfer (Message / service request/response)
    /// @return Status of creating the record
    Status registerPublication(PortID port_id, TransferKind transfer_kind) noexcept override
    {
        // Create a publication record to hold the metadata associated with this individual broadcast
        return createPublicationRecord(publication_records_,
                                       CanardPriorityNominal,  // FIXME Eventually priorities should be assigned per
                                                               // message node and port ID pair
                                       libcyphalToCanardTransferKind(transfer_kind),
                                       port_id);
    }

    /// @brief Registers interest in a specific subject ID from this transport.
    /// This allows messages to be delivered to @ref Listener::Receive
    /// @param[in] port_id The Subject id / service id to register
    /// @param[in] transfer_kind Type of transfer (Message / service request/response)
    /// @return status of udpard subscribing
    Status registerSubscription(PortID port_id, TransferKind transfer_kind) noexcept override
    {
        if (is_registration_closed_)
        {
            return Status(ResultCode::NotAllowed, CauseCode::FiniteStateMachine);
        }
        Status subscription_status = canardSubscribe(port_id, transfer_kind);

        if (subscription_status.isSuccess())
        {
            // Generate new filter for the subscribed message then consolidate
            CanardFilter new_filter;
            if (transfer_kind == TransferKind::TransferKindMessage)
            {
                new_filter = canardMakeFilterForSubject(port_id);
            }
            else
            {
                new_filter = canardMakeFilterForService(port_id, canard_.node_id);
            }
            // FIXME AVF-411 determine why 'canardConsolidateFilters' is not working or store filters in List
            // consolidated_filter = canardConsolidateFilters(&consolidated_filter_, &new_filter);
            (void) new_filter;
        }

        return subscription_status;
    }

    /// @brief Disallow any further subscriptions to be added
    /// @return Whether or not closing the registration was successful
    Status closeRegistration() noexcept override
    {
        BusStatus bus_status;

        // Convert consolidated filter to ABSP style CAN filter and use it to configure drivers
        media::can::Filter can_filter;
        can_filter.raw.setID(UsesExtendedFrames, consolidated_filter_.extended_can_id);
        can_filter.mask = consolidated_filter_.extended_mask;

        bus_status.primary = (dynamic_cast<CANTransport&>(primary_bus_)).configure(&can_filter, 1);

        if (backup_bus_ != nullptr)
        {
            bus_status.backup = (dynamic_cast<CANTransport*>(backup_bus_))->configure(&can_filter, 1);
        }

        is_registration_closed_ = true;

        // Compare and return configuration statuses
        // Both busses must be successful to return SUCCESS
        return bus_status.allSuccess();
    }

private:
    /// A publication record is the metadata associated with the latest transfer for a node and port ID pair
    using PublicationRecordsList =
        cetl::VariableLengthArray<CanardTransferMetadata,
                                  cetl::pf17::pmr::polymorphic_allocator<CanardTransferMetadata>>;

    static constexpr std::size_t MTUSize = CANARD_MTU_CAN_FD;
    // This is the number of frames that can be held in the TX FIFO at once
    static constexpr std::size_t                      TXFIFOSize = LIBCYPHAL_TRANSPORT_MAX_FIFO_QUEUE_SIZE;
    static constexpr time::Monotonic::MicrosecondType DefaultSubscriptionTimeoutUS{60'000'000ULL};

    // Redundant bus enumerations
    enum BusIndex : std::uint8_t
    {
        Primary     = 0,
        Backup      = 1,
        MaxBusIndex = 2
    };

    // Status fields for the primary and backup conveniently grouped together
    struct BusStatus
    {
        Status primary;
        Status backup;
        Status allSuccess()
        {
            if (primary.isFailure())
            {
                return primary;
            }
            return backup;
        };
        Status anySuccess()
        {
            if (primary.isSuccess())
            {
                return primary;
            }
            return backup;
        };
    };

    TransportID          transport_id_;
    bool                 cleanup_initiated_;
    CanardMemoryAllocate fn_canard_mem_allocate_;
    CanardMemoryFree     fn_canard_mem_free_;
    const time::Timer&   timer_;                  //!< For timing transfers
    Interface&           primary_bus_;            //!< Primary CAN bus
    Interface*           backup_bus_;             //!< Backup CAN bus for fully redundant transports
    CanardFilter         consolidated_filter_;    //!< Current acceptance filter applied to primary and backup buses
    cetl::pf17::pmr::memory_resource* resource_;  //!< Pointer to the memory resource for this transport.
    CanardInstance                    canard_;    //!< Canard handler instance
    CanardTxQueue                     canard_tx_fifo_;  //!< Primary CAN bus TX frame queue

    /// Subscription records, initialized and used by Canard but managed by this class
    /// Each represents an instance of one of three types of subscription:
    ///  1. Message - Accepts multi-cast transfers of a specific subject ID
    ///  2. Request - Accepts request transfers from a specific port and node ID pair
    ///  3. Response - Accepts response transfers from a specific port and node ID pair
    CanardRxSubscription subscription_records_[MaxNumberOfSubscriptionRecords]{};
    std::size_t          current_sub_index_{0};

    /// Publication records, split between multi-cast, request, and response transfer types to increase search
    /// efficiency. Each entry caches the transfer metadata for the next transfer of its respective type to be
    /// published.
    std::array<CanardTransferMetadata, MaxNumberOfBroadcasts>
        publication_record_storage_{};  //!< Records of all publications from this transport

    cetl::pf17::pmr::deviant::basic_monotonic_buffer_resource
        publication_records_resource_{publication_record_storage_.data(), publication_record_storage_.size()};
    PublicationRecordsList publication_records_{&publication_records_resource_};

    // The following are cached during 'ProcessIncomingFrames()' and not used for transmit operations
    BusIndex  current_rx_bus_index_{Primary};  //!< The current receiving bus index
    Listener* current_listener_{nullptr};      //!< The current listener to received frames

    bool is_registration_closed_{
        false};  //!< Indicates if registration has been closed. i.e. subscriptions are no longer allowed

    /// @brief Converts udpard Metadata type to libcyphal metadata type
    /// @param[in] metadata the udpard metadata type to convert
    /// @todo Have common metadata between udpard/canard and libcyphal
    TxMetadata canardToLibcyphalTxMetadata(const CanardTransferMetadata& metadata) noexcept
    {
        TxMetadata converted{};
        converted.kind           = static_cast<TransferKind>(metadata.transfer_kind);
        converted.port_id        = static_cast<PortID>(metadata.port_id);
        converted.priority       = static_cast<TransferPriority>(metadata.priority);
        converted.remote_node_id = static_cast<NodeID>(metadata.remote_node_id);
        return converted;
    }

    /// Registers a new subscription with Canard
    /// @param[in] port Subject or service ID, "port ID" is the ambiguous term
    /// @param[in] transfer_type Transfer type (Message, Request, Response)
    Status canardSubscribe(PortID port, TransferKind transfer_type) noexcept
    {
        // The subscription records list should be declared with exactly enough space, this line should not be reached
        // once the list is at capacity.
        if (current_sub_index_ >= MaxNumberOfSubscriptionRecords)
        {
            return Status(ResultCode::NotEnough, CauseCode::Resource);
        }
        // Get reference to the next empty subscription record and pass it to 'canardRxSubscribe' to be populated
        CanardRxSubscription& new_sub       = subscription_records_[current_sub_index_];
        std::int8_t           canard_status = canardRxSubscribe(&canard_,
                                                      libcyphalToCanardTransferKind(transfer_type),
                                                      port,
                                                      MaxMessageExtent,
                                                      DefaultSubscriptionTimeoutUS,
                                                      &new_sub);

        // If the subscription is successful, the record will be activated by assigning the transfer type
        // (broadcast, request, response) to its 'user_reference' field. The current subscription index is then iterated
        // forward to the next empty record.
        Status subscription_status = ardStatusToCyphalStatus(static_cast<ArdStatus>(canard_status));
        if (subscription_status.isSuccess())
        {
            // We need to keep track of the 'transfer_type' per subscription
            // Store its value in subscription record's 'user_reference' field as a void*
            new_sub.user_reference = reinterpret_cast<void*>(transfer_type);

            // Finalize registration by incrementing index to next empty slot
            current_sub_index_++;
        }

        return subscription_status;
    }

    /// @brief Creates a publication record given a priority, type and subjectid/serviceid
    /// @tparam Count Sets the size of the publication record list
    /// @param[out] out_records publication record list to write to
    /// @param[in] priority priority of message
    /// @param[in] transfer_type transfer type (service or message)
    /// @param[in] port Canard PortID (subjectid/serviceid)
    Status createPublicationRecord(PublicationRecordsList& out_records,
                                   CanardPriority          priority,
                                   CanardTransferKind      transfer_type,
                                   CanardPortID            port) noexcept
    {
        // Emplace publication record at back of list
        // Do not need to check the success of emplacement as we know we have enough room per the guard above
        // Publication records have an anonymous node ID and unset transfer ID fields upon initialization
        const std::size_t size_before = out_records.size();
        out_records.emplace_back(
            CanardTransferMetadata{priority,             // Transfer priority, passed from on high
                                   transfer_type,        // 'UdpardTransferKindMessage/Request/Response'
                                   port,                 // Subject or service ID
                                   AnonymousNodeID,      // Starts off as anonymous (indicates record is inactive)
                                   InitialTransferID});  // Starts at 0
        if (out_records.size() == size_before + 1)
        {
            return ResultCode::Success;
        }
        else
        {
            // The records list should be declared with exactly enough space, this line should not be reached once the
            // list is at capacity.
            return Status(ResultCode::NotEnough, CauseCode::Resource);
        }
    }

    /// @brief Fetches publication record from the provided records list
    /// @note The record is active (already in use) if its 'remote_node_id' field is set. If the field is unset, it is
    ///       inactive and available.
    /// @param[in] records publication record list
    /// @param[in] port Canard PortID (subjectid/serviceid)
    /// @param[in] node NodeID
    CanardTransferMetadata* getPublicationRecord(PublicationRecordsList& records,
                                                 PortID                  port,
                                                 CanardNodeID            node) noexcept
    {
        // Iterate through publication records
        // Active publication records ('remote_node_id' field is set) are listed before inactive ones ('remote_node_id'
        // field is anonymous) with the same port ID
        // If a record's 'port_id' and 'remote_node_id' match the argument values or, if no active match was found and
        // this is the first inactive record with a matching port ID, return it
        for (CanardTransferMetadata& record : records)
        {
            if ((record.port_id == port) && ((record.remote_node_id == node) || (record.remote_node_id == 0)))
            {
                return &record;
            }
        }

        // No inactive record with a matching port ID was found
        return nullptr;
    }

    /// @brief Publishes a serialized cyphal message to UDP
    /// @param[in] metadata metadata for udpard transfer containing information like transferid, source nodeid, etc.
    /// @param[in] out_message Message to send
    Status publishTransfer(const CanardTransferMetadata& metadata, const Message& message)
    {
        // Push message to Canard TX queue
        CanardMicrosecond no_timeout = 0;  // Transfers are queued and published in-line here, no timeout necessary
        std::int32_t      push_status =
            canardTxPush(&canard_tx_fifo_, &canard_, no_timeout, &metadata, message.size(), message.data());

        Status publish_status = ardStatusToCyphalStatus(static_cast<ArdStatus>(push_status));
        if (publish_status.isSuccess())
        {
            // Pop them from the queue and transmit
            const CanardTxQueueItem* curr_tx_item = canardTxPeek(&canard_tx_fifo_);
            BusStatus                bus_status;
            while (curr_tx_item != nullptr)
            {
                // Transmit frame via driver interfaces
                const CanardFrame& canard_frame = curr_tx_item->frame;
                media::can::extended::Frame
                           libcyphal_frame(media::can::extended::Identifier{libcyphal::media::can::extended::IDMask &
                                                                     canard_frame.extended_can_id},
                                    media::can::nearestDataLengthCode(canard_frame.payload_size),
                                    reinterpret_cast<const std::uint8_t*>(canard_frame.payload));
                TxMetadata tx_metadata{canardToLibcyphalTxMetadata(metadata)};
                // If a driver has failed while in this loop then the transfer will likely be incomplete on that bus.
                // Give up publishing to that bus for this transfer and try again with the next.
                // This also guards against invalid interface pointers
                bus_status.primary = primary_bus_.transmit(tx_metadata, libcyphal_frame);

                if (backup_bus_ && bus_status.backup.isSuccess())
                {
                    bus_status.backup = backup_bus_->transmit(tx_metadata, libcyphal_frame);
                }

                // Pop current item, deallocate, then grab next one

                fn_canard_mem_free_(&canard_, canardTxPop(&canard_tx_fifo_, curr_tx_item));

                curr_tx_item = canardTxPeek(&canard_tx_fifo_);
            }

            publish_status = bus_status.anySuccess();
        }

        return publish_status;
    }

    /// @brief Converts status received from libcanard into a libcypahl Status
    /// @param[in] result libcanard status received
    /// @return Success or Failure Status class
    static Status toCyphalStatus(CanardStatus canard_status) noexcept
    {
        // 0 indicates a transfer in progress or non-failure (let's call it a soft success)
        // Positive values indicate hard success
        if (canard_status >= 0)
        {
            return Status();
        }

        // Negative values indicate errors but the error codes are the positive equivalent for some reason
        // See 'canard.h' line 109
        if (canard_status == (-CANARD_ERROR_INVALID_ARGUMENT))
        {
            return Status(ResultCode::Invalid, CauseCode::Parameter);
        }
        else if (canard_status == (-CANARD_ERROR_OUT_OF_MEMORY))
        {
            return Status(ResultCode::NotEnough, CauseCode::Resource);
        }

        // Else it's an unknown error
        return Status(ResultCode::Failure, CauseCode::Unknown);
    }
};

}  // namespace can
}  // namespace transport
}  // namespace libcyphal

#endif  // LIBCYPHAL_TRANSPORT_CAN_CYPHAL_CAN_TRANSPORT_HPP_INCLUDED