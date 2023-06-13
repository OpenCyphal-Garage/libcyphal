/// @copyright Copyright Amazon.com Inc. and its affiliates. All Rights Reserved.
/// @file
/// Cyphal Transport Layer implementation of UDP

#ifndef LIBCYPHAL_TRANSPORT_UDP_CYPHAL_UDP_TRANSPORT_HPP_INCLUDED
#define LIBCYPHAL_TRANSPORT_UDP_CYPHAL_UDP_TRANSPORT_HPP_INCLUDED

#include <cstdint>
#include <cstring>
#include <array>
#include <udpard.h>
#include "libcyphal/build_config.hpp"
#include "libcyphal/media/udp/frame.hpp"
#include "libcyphal/transport.hpp"
#include "libcyphal/transport/listener.hpp"
#include "libcyphal/transport/message.hpp"
#include "libcyphal/transport/metadata.hpp"
#include "libcyphal/transport/ip/v4/address.hpp"
#include "libcyphal/transport/udp/interface.hpp"
#include "libcyphal/transport/udp/transport.hpp"
#include "libcyphal/types/status.hpp"
#include "libcyphal/types/time.hpp"

#include "cetl/variable_length_array.hpp"
#include "cetl/pf17/memory_resource.hpp"

namespace libcyphal
{
namespace transport
{
namespace udp
{

/// Sets the maximum number of possible transfers that an instance of CyphalUDPransport can manage
/// @todo Determine a proper static size for maximum number of broadcasts/subscriptions
static constexpr std::size_t MaxNumberOfBroadcasts{
    LIBCYPHAL_TRANSPORT_MAX_BROADCASTS};  ///<! Max number of broadcast message types
static constexpr std::size_t MaxNumberOfSubscriptions{
    LIBCYPHAL_TRANSPORT_MAX_SUBSCRIPTIONS};  ///<! Max number of broadcast subscriptions
static constexpr std::size_t MaxNumberOfResponses{
    LIBCYPHAL_TRANSPORT_MAX_RESPONSES};  ///!< Max number of response transfer types
static constexpr std::size_t MaxNumberOfRequests{
    LIBCYPHAL_TRANSPORT_MAX_REQUESTS};  ///!< Max number of request transfer types

static constexpr UdpardNodeID     AnonymousNodeID{UDPARD_NODE_ID_UNSET};
static constexpr UdpardTransferID InitialTransferID{0};  ///<! Transfer IDs for new transactions start at 0
/// Maximum number of subscription records that an instance can manage, cannot be 0
static constexpr std::size_t MaxNumberOfSubscriptionRecords{MaxNumberOfSubscriptions + MaxNumberOfResponses + MaxNumberOfRequests};
static_assert(MaxNumberOfSubscriptionRecords, "MaxNumberOfSubscriptions, Responses, or Requests must be nonzero");

/// Maximum number of publication records that an instance can manage, cannot be 0
static constexpr std::size_t MaxNumberOfPublicationRecords{MaxNumberOfBroadcasts + MaxNumberOfResponses + MaxNumberOfRequests};
static_assert(MaxNumberOfPublicationRecords, "MaxNumberOfBroadcasts, Responses, or Requests must be nonzero");

/// Cyphal transport layer implementation for UDP
class CyphalUDPTransport final : public Transport, public Interface::Receiver
{
public:
    /// CyphalUDPTransport constructor
    /// @note Users of this class will need to run "initialize()" after constructing and make sure there are no errors
    ///       before proceeding to use this class.
    /// @param[in] primary_interface the UDP Transport interface (UdpTransport) defined by the user
    /// @param[in] backup_interface optional. Can be null
    /// @param[in] node_id The Node ID for the UDP interface
    /// @param[in] timer An OS specific, or generic implementation of timer
    /// @param[in] message_buffer   Buffer memory. Don't ask. This is sorta hacky.
    /// @param[in] allocator function that allocates memory off the given memory resource
    /// @param[in] releaser function that release memory off the given memory resource
    CyphalUDPTransport(Interface&                        primary_interface,
                       Interface*                        backup_interface,
                       NodeID                            node_id,
                       const time::Timer&                timer,
                       cetl::pf17::pmr::memory_resource* message_buffer,
                       UdpardMemoryAllocate              allocator,
                       UdpardMemoryFree                  releaser)
        : primary_bus_{primary_interface}
        , backup_bus_{backup_interface}
        , timer_{timer}
        , resource_{message_buffer}
        , fn_udpard_mem_allocate_{allocator}
        , fn_udpard_mem_free_{releaser}
        , udpard_{udpardInit(allocator, releaser)}
        , udpard_tx_fifo_{udpardTxInit(TxFIFOSize, MTUSize)}
    {
        udpard_.node_id = node_id;
        //   See 'CyphalUDPTransport::UdpardMemAllocate()' or 'CyphalUDPTransport::UdpardMemFree()' for usage
        udpard_.user_reference = resource_;
    }

    /// CyphalUDPTransport constructor with anonymous NodeID
    /// @note Anonymous NodeID can only be used to listen to broadcasts. Set NodeID later to also transmit.
    /// @note Users of this class will need to run "initialize()" after constructing and make sure there are no errors
    ///       before proceeding to use this class.
    /// @param[in] primary_interface the UDP Transport interface (UdpTransport) defined by the user
    /// @param[in] backup_interface optional. Can be null
    /// @param[in] timer An OS specific, or generic implementation of timer
    /// @param[in] message_buffer It's a thing.
    /// @param[in] allocator function that allocates memory off the given memory resource
    /// @param[in] releaser function that release memory off the given memory resource
    CyphalUDPTransport(Interface&                        primary_interface,
                       Interface*                        backup_interface,
                       const time::Timer&                timer,
                       cetl::pf17::pmr::memory_resource* message_buffer,
                       UdpardMemoryAllocate              allocator,
                       UdpardMemoryFree                  releaser)
        : CyphalUDPTransport(primary_interface,
                             backup_interface,
                             NodeID(static_cast<std::uint16_t>(AnonymousNodeID)),
                             timer,
                             message_buffer,
                             allocator,
                             releaser)
    {
    }

    CyphalUDPTransport(const CyphalUDPTransport&)            = delete;
    CyphalUDPTransport(CyphalUDPTransport&&)                 = delete;
    CyphalUDPTransport& operator=(const CyphalUDPTransport&) = delete;
    CyphalUDPTransport& operator=(CyphalUDPTransport&&)      = delete;

    /// @brief Transport destructor
    virtual ~CyphalUDPTransport() noexcept {}

    /// @brief Unsubscribes for active records, removes from TX queue, and deallocates
    Status cleanup() override
    {
        Status ret{Status()};
        if (!cleanup_initiated)
        {
            // Unsubscribe from all subscription records
            for (std::size_t i = 0; i < MaxNumberOfSubscriptionRecords; i++)
            {
                UdpardRxSubscription& record = subscription_records_[i];

                // Check activation status of the subscription record via the user reference
                if (record.user_reference)
                {
                    // Record is active, unsubscribe. Transfer type enum value is cached in the record's
                    // 'user_reference'
                    //   field, which is void*, so we need to reinterpret_cast() that to a uintptr_t before it can be
                    //   static cast to a uint
                    std::uintptr_t     transfer_kind_ref = reinterpret_cast<std::uintptr_t>(record.user_reference);
                    UdpardTransferKind transfer_kind     = static_cast<UdpardTransferKind>(transfer_kind_ref);
                    std::int8_t unsubscribe_status       = udpardRxUnsubscribe(&udpard_, transfer_kind, record.port_id);
                    // Clear subscription record to deactivate
                    std::memset(&record, 0, sizeof(UdpardRxSubscription));
                    if (unsubscribe_status < 0)
                    {
                        ret.setResultAndCause(ResultCode::Invalid, CauseCode::Parameter);
                    }
                }
                else
                {
                    // First inactive record, the rest are as well. Break early
                    break;
                }
            }

            // Pop all transfers from the TX queue and deallocate
            const UdpardTxQueueItem* curr_tx_item = udpardTxPeek(&udpard_tx_fifo_);
            while (curr_tx_item != nullptr)
            {
                fn_udpard_mem_free_(&udpard_, udpardTxPop(&udpard_tx_fifo_, curr_tx_item));

                curr_tx_item = udpardTxPeek(&udpard_tx_fifo_);
            }
            cleanup_initiated = true;
        }
        return ret;
    }

    /// @todo Remove this and replace with common *ard metadata types
    static UdpardTransferKind libcyphalToUdpardTransferKind(TransferKind kind)
    {
        return static_cast<UdpardTransferKind>(kind);
    }

    /// @todo Remove this and replace with common *ard metadata types
    static UdpardPriority libcyphalToUdpardPriority(TransferPriority priority)
    {
        return static_cast<UdpardPriority>(priority);
    }

    /// @brief Sets the node ID for this transport
    /// @param[in] node_id The new node ID
    inline void setNodeID(NodeID node_id) noexcept
    {
        udpard_.node_id = node_id;
    }

    /// @brief Initializes and verifies all input variables
    /// @return Status of proper initialization
    Status initialize() override
    {
        ///<! both buses are nullptr
        if ((nullptr == fn_udpard_mem_allocate_) || (nullptr == fn_udpard_mem_free_))
        {
            return Status(ResultCode::Invalid, CauseCode::Parameter);
        }
        return ResultCode::Success;
    }

    /// @brief Allows a transport to transmit a serialized payload
    /// @param[in] tx_metadata The metadata of the payload
    /// @param[in] payload The read only reference to the payload information
    /// @retval Success - Payload transmitted
    /// @retval Invalid - No publication record found or trying to publish anonymously
    /// @retval Failure - Could not transmit the payload.
    Status transmit(const TxMetadata& tx_metadata, const Message& payload) override
    {
        // Cannot publish when NodeID is not set (anonymous node)
        if (udpard_.node_id == AnonymousNodeID)
        {
            return Status(ResultCode::Invalid, CauseCode::Parameter);
        }

        // Broadcast message records do not utilize the remote node ID field in the publication records list, leave it unset
        UdpardNodeID remote_node_id = AnonymousNodeID;
        if (tx_metadata.kind == TransferKindRequest || tx_metadata.kind == TransferKindResponse)
        {
            remote_node_id = tx_metadata.remote_node_id;
        }

        UdpardTransferMetadata* const record = getPublicationRecord(
                publication_records_,
                libcyphalToUdpardTransferKind(tx_metadata.kind),
                tx_metadata.port_id,
                remote_node_id);

        if (record == nullptr)
        {
            // Should not be here, a lack of records for this transfer means that the transport was not informed of
            // this transfer via 'registerPublication()'
            return Status(ResultCode::NotInitialized, CauseCode::Session);
        }

        if ((tx_metadata.kind == TransferKindResponse) && (record->remote_node_id == AnonymousNodeID))
        {
            // An anonymous 'remote_node_id' field for this response record means that the transport was informed of
            // this response, but the predicating request has not been received yet. Thus, the response record is
            // still inactive and the transfer will likely be ignored by the other devices on the bus.
            return Status(ResultCode::NotReady, CauseCode::Resource);
        }

        if (tx_metadata.kind == TransferKindRequest)
        {
            record->remote_node_id = tx_metadata.remote_node_id;
        }

        Status publication_status = publishTransfer(*record, payload);
        if (publication_status.isFailure()) {
            return publication_status;
        }

        // If the publication was a success, increment the transfer ID for the next broadcast or request.
        // Ensure it stays within the accepted range. We do not increment the transfer ID for responses
        // because it should match the transfer ID of the request.
        if ((tx_metadata.kind == TransferKindMessage) || (tx_metadata.kind == TransferKindRequest))
        {
            record->transfer_id = (record->transfer_id + 1) % UDPARD_TRANSFER_ID_MAX;
        }

        return publication_status;
    }

    /// @brief Transmit a serialized message with the subject ID
    /// @param[in] subject_id The subject ID of the message
    /// @param[in] message The read only reference to the payload information.
    /// @retval Success - Message transmitted
    /// @retval Invalid - No record found or trying to broadcast anonymously
    /// @retval Failure - Could not transmit the message.
    Status broadcast(PortID subject_id, const Message& message)
    {
        TxMetadata metadata{};
        metadata.port_id        = subject_id;
        metadata.kind           = TransferKindMessage;
        metadata.priority       = PriorityNominal;
        metadata.remote_node_id = AnonymousNodeID;
        return transmit(metadata, message);
    }

    /// @brief Transmit a serialized request with the specified service ID
    /// @param[in] service_id The service ID of the request
    /// @param[in] remote_node_id The Node ID to whom the request will be sent
    /// @param[in] request The read only reference to the payload information
    /// @retval Success - Request transmitted
    /// @retval Invalid - No record found for request or trying to publish anonymously
    /// @retval Failure - Could not transmit the request.
    Status sendRequest(PortID service_id, NodeID remote_node_id, const Message& request)
    {
        TxMetadata metadata{};
        metadata.port_id        = service_id;
        metadata.kind           = TransferKindRequest;
        metadata.priority       = PriorityNominal;
        metadata.remote_node_id = remote_node_id;
        return transmit(metadata, request);
    }

    /// @brief Transmit a serialized response with the specified service ID
    /// @param[in] service_id The service ID of the response
    /// @param[in] remote_node_id The Node ID to whom the response will be sent
    /// @param[in] response The read only reference to the payload information.
    /// @retval Success - Response transmitted
    /// @retval Invalid - No record found for response or trying to publish anonymously
    /// @retval Failure - Could not transmit the response.
    Status sendResponse(PortID service_id, NodeID remote_node_id, const Message& response)
    {
        TxMetadata metadata{};
        metadata.port_id        = service_id;
        metadata.kind           = TransferKindResponse;
        metadata.priority       = PriorityNominal;
        metadata.remote_node_id = remote_node_id;
        return transmit(metadata, response);
    }

    /// @brief Called by the Interface when an UDP Frame is available
    /// @note Implements libcyphal::transport::Receiver::onReceive
    /// @param[in] frame the UDP frame
    void onReceive(const media::udp::Frame& frame) noexcept override
    {
        UdpardRxTransfer             received;                // Incoming transfer
        UdpardRxSubscription** const subscription = nullptr;  // Optional, unused reference to the subscription
        // Timer_.setCurrentTime();
        time::Monotonic now = timer_.getTimeInUs();
        UdpardFrame     udpard_frame;
        udpard_frame.payload           = &frame.data_[0];
        udpard_frame.payload_size      = frame.data_length_;
        udpard_frame.udp_cyphal_header = frame.header_;

        std::int8_t accept_status = udpardRxAccept(&udpard_,
                                                   static_cast<UdpardMicrosecond>(now.toMicrosecond()),
                                                   &udpard_frame,
                                                   current_rx_bus_index_,
                                                   &received,
                                                   subscription);

        // If `accept_status` is 1, a new transfer is available for processing.
        // If it is 0, a transfer may still be in progress or the frame was discarded 
        // by Udpard. It is not a failure or success.
        // If it is negative, an error occurred while accepting the new frame
        if (accept_status == 1)
        {
            // Frame has been accepted, new transfer available
            // Store transfer payload into serialized buffer
            Message payload(static_cast<std::uint8_t*>(received.payload), received.payload_size);

            // Determine if transfer is a message or request/response, then call the appropriate callback
            UdpardPortID port_id  = received.metadata.port_id;
            RxMetadata   metadata = udpardToLibcyphalRxMetadata(received.metadata);
            if (received.metadata.transfer_kind == UdpardTransferKindRequest)
            {
                // Incoming transfer is a service request
                // Update response record metadata associated with this request
                UdpardNodeID            node_id = received.metadata.remote_node_id;
                UdpardTransferMetadata* record  = getPublicationRecord(
                    publication_records_,
                    UdpardTransferKindResponse,
                    port_id,
                    node_id);

                if (record != nullptr)
                {
                    // Cache the request's transfer and node ID within the response record

                    // Save off the transfer ID of the request in the response publication record because we need
                    // to publish the response using the same transfer ID that was in the request, as required
                    // by the Cyphal specification
                    record->transfer_id    = received.metadata.transfer_id;

                    // Activates record if this is the first accepted request and is otherwise just writing the same
                    // value to its 'remote_node_id' field
                    record->remote_node_id = node_id;
                }
            }

            current_listener_->onReceive(metadata, payload);

            // Deallocate transfer payload, nullptr is handled gracefully
            fn_udpard_mem_free_(&udpard_, received.payload);
        }
    }

    /// Called by clients in order to processes incoming UDP Frames
    /// @param[in] listener Object that provides callbacks to the application layer to trigger from the transport
    /// @note The implementation will invoke the listener with the appropriately typed Frames.
    ///     1. The user defines a Listener by implementing the Listener APIs. For example, if the user wants custom
    ///        behavior after receiving a broadcast message, onReceive could perhaps deserialize and print
    ///        the message as an example
    ///     2. The user defines UDP Interfaces by implementing libcyphal::transport::udp::transport.hpp. This is
    ///        considered the primary/secondary "buses".
    ///     3. The user application or libcyphal Application layer calls processIncomingTransfers(<Listener>)
    ///     4. CyphaUDPTransport triggers a UDP Interface call to the primary/secondary bus and calls
    ///        processIncomingFrames
    ///     5. This calls whatever OS level APIs are available to receive UDP packets
    ///     6. After the transfer is received and udpard is notified, the listener's onReceive* API is called
    /// @note The lifecycle of the Listener is maintained by the application/application layer and not this class
    /// @note Multiple calls to this API are needed for large payloads until the EOT flag in the header is set
    ///       indicating the transfer is complete and thus sending the buffer back to the user. Use caution as very
    ///       large payloads can take a while before downloading the full buffer. It is up to the user whether to
    ///       block (for example looping back to back waiting for the buffer) or download frame by frame per loop cycle.
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
        bus_status.primary        = primary_bus_.processIncomingFrames(*this);

        if (backup_bus_ != nullptr)
        {
            current_rx_bus_index_ = Backup;
            bus_status.backup         = backup_bus_->processIncomingFrames(*this);
        }
        else
        {
            bus_status.backup = Status(ResultCode::NotConfigured, CauseCode::Resource);
        }

        // Clear current listener to make it available for the next call to this method
        current_listener_ = nullptr;

        // Compare and return driver bus_statuses
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
        // Create a publication record to hold the metadata associated with this individual port ID
        return createPublicationRecord(
                publication_records_,
                UdpardPriorityNominal,  // FIXME Eventually priorities should be assigned per node and port ID pair
                libcyphalToUdpardTransferKind(transfer_kind),
                port_id);
    }

    /// @brief Registers interest in a specific port ID from this transport.
    /// This allows transfers to be delivered to @ref Listener::Receive
    /// @param[in] port_id PortID of the transfer (subject id / service id)
    /// @param[in] transfer_kind Type of transfer (Message / service request/response)
    /// @return status of udpard subscribing
    Status registerSubscription(PortID port_id, TransferKind transfer_kind) noexcept override
    {
        if (is_registration_closed_)
        {
            return Status(ResultCode::NotAllowed, CauseCode::FiniteStateMachine);
        }
        Status subscription_status = udpardSubscribe(port_id, libcyphalToUdpardTransferKind(transfer_kind));

        return subscription_status;
    }

    /// @brief Disallow any further subscriptions to be added
    /// @return Whether or not closing the registration was successful
    Status closeRegistration() noexcept override
    {
        is_registration_closed_ = true;
        return Status(ResultCode::Success);
    }

private:
    static constexpr time::Monotonic::MicrosecondType DefaultSubscriptionTimeoutUs =
        UDPARD_DEFAULT_TRANSFER_ID_TIMEOUT_USEC;

    static constexpr std::size_t MTUSize = UDPARD_MTU_UDP_IPV4;
    // FIXME pick a non-arbitrary value. this is the number of frames that can be held in the TX FIFO at once
    static constexpr std::size_t TxFIFOSize = LIBCYPHAL_TRANSPORT_MAX_FIFO_QUEUE_SIZE;

    enum BusIndex : std::uint8_t
    {
        Primary     = 0,
        Backup      = 1,
        MaxBusIndex = 2
    };

    BusIndex current_rx_bus_index_{Primary};

    // Status fields for the primary and backup conveniently grouped together
    struct BusStatus
    {
        Status primary;
        Status backup;
    };

    bool cleanup_initiated{false};

    Interface&         primary_bus_; //!< Primary CAN bus. Reference type because primary bus is required.
    Interface*         backup_bus_;  //!< Backup CAN bus for fully redundant transports. Pointer type
                                     //!< because backup bus is optional.
    const time::Timer& timer_;       //!< For timing transfers

    // Subscription records, initialized and used by Udpard but managed by this class
    // Each represents an instance of one of three types of subscription:
    //  1. Message - Accepts multi-cast transfers of a specific subject ID
    //  2. Request - Accepts request transfers from a specific port and node ID pair
    //  3. Response - Accepts response transfers from a specific port and node ID pair
    UdpardRxSubscription subscription_records_[MaxNumberOfSubscriptionRecords]{};
    std::size_t          current_sub_index_{0};

    // The following are cached during 'ProcessIncomingFrames()' and not used for transmit operations
    Listener*                         current_listener_{nullptr};  // The current listener to received frames
    cetl::pf17::pmr::memory_resource* resource_;                   // memory resource for buffering messages.
    UdpardMemoryAllocate              fn_udpard_mem_allocate_;
    UdpardMemoryFree                  fn_udpard_mem_free_;
    UdpardInstance                    udpard_;                         // Udpard handler instance
    UdpardTxQueue                     udpard_tx_fifo_;                 // Primary UDP bus TX frame queue
    bool                              is_registration_closed_{false};  // Indicates if registration has been closed.

    /// A publication record is the metadata associated with the latest transfer for a node and port ID pair
    using PublicationRecordsList =
        cetl::VariableLengthArray<UdpardTransferMetadata,
                                  cetl::pf17::pmr::polymorphic_allocator<UdpardTransferMetadata>>;

    /// Publication records. Each entry caches the transfer metadata for the next transfer of its
    /// respective type to be published.
    std::array<UdpardTransferMetadata, MaxNumberOfPublicationRecords>
        publication_record_storage_{};  //!< Records of all publications from this transport

    cetl::pf17::pmr::deviant::basic_monotonic_buffer_resource
        publication_records_resource_{publication_record_storage_.data(), publication_record_storage_.size()};
    PublicationRecordsList publication_records_{&publication_records_resource_};

    /// @brief Converts udpard Metadata type to libcyphal metadata type
    /// @param[in] metadata the udpard metadata type to convert
    /// @todo Have common metadata between udpard/canard and libcyphal
    RxMetadata udpardToLibcyphalRxMetadata(const UdpardTransferMetadata& metadata) noexcept
    {
        RxMetadata converted{};
        converted.kind           = static_cast<TransferKind>(metadata.transfer_kind);
        converted.port_id        = static_cast<PortID>(metadata.port_id);
        converted.priority       = static_cast<TransferPriority>(metadata.priority);
        converted.remote_node_id = static_cast<NodeID>(metadata.remote_node_id);
        converted.transfer_id    = static_cast<TransferID>(metadata.transfer_id);
        converted.timestamp_us   = timer_.getTimeInUs().toMicrosecond();
        return converted;
    }

    /// @brief Converts udpard Metadata type to libcyphal metadata type
    /// @param[in] metadata the udpard metadata type to convert
    /// @todo Have common metadata between udpard/canard and libcyphal
    TxMetadata udpardToLibcyphalTxMetadata(const UdpardTransferMetadata& metadata) noexcept
    {
        TxMetadata converted{};
        converted.kind           = static_cast<TransferKind>(metadata.transfer_kind);
        converted.port_id        = static_cast<PortID>(metadata.port_id);
        converted.priority       = static_cast<TransferPriority>(metadata.priority);
        converted.remote_node_id = static_cast<NodeID>(metadata.remote_node_id);
        return converted;
    }

    /// Registers a new subscription with Udpard
    /// @param[in] port Subject or service ID, "port ID" is the ambiguous term
    /// @param[in] transfer_type Transfer type (Message, Request, Response)
    Status udpardSubscribe(UdpardPortID port, UdpardTransferKind transfer_type) noexcept
    {
        // The subscription records list should be declared with exactly enough space, this line should not be reached
        // once the list is at capacity.
        if (current_sub_index_ >= MaxNumberOfSubscriptionRecords)
        {
            return Status(ResultCode::NotEnough, CauseCode::Resource);
        }

        // Get reference to the next empty subscription record and pass it to 'udpardRxSubscribe' to be populated
        UdpardRxSubscription& new_sub       = subscription_records_[current_sub_index_];
        std::int8_t           udpard_status = udpardRxSubscribe(&udpard_,
                                                      transfer_type,
                                                      port,
                                                      MaxMessageSize,
                                                      UDPARD_DEFAULT_TRANSFER_ID_TIMEOUT_USEC,
                                                      &new_sub);

        // If the subscription is successful, the record will be activated by assigning the transfer type
        // (broadcast, request, response) to its 'user_reference' field. The current subscription index is then
        // iterated forward to the next empty record.
        Status subscription_status = ardStatusToCyphalStatus(static_cast<ArdStatus>(udpard_status));
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
    /// @param[out] out_records Publication record list to write to
    /// @param[in] priority Priority of the transfer
    /// @param[in] transfer_type Transfer type (service or message)
    /// @param[in] port UDPard PortID (subjectid/serviceid)
    static Status createPublicationRecord(PublicationRecordsList& out_records,
                                          UdpardPriority          priority,
                                          UdpardTransferKind      transfer_type,
                                          UdpardPortID            port) noexcept
    {
        // Emplace publication record at back of list
        // Do not need to check the success of emplacement as we know we have enough room per the guard above
        // Publication records have an anonymous node ID and unset transfer ID fields upon initialization
        const std::size_t size_before = out_records.size();
        out_records.emplace_back(
            UdpardTransferMetadata{priority,             // Transfer priority, passed from on high
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
    /// @param[in] port UDPard PortID (subjectid/serviceid)
    /// @param[in] node NodeID
    static UdpardTransferMetadata* getPublicationRecord(PublicationRecordsList& records,
                                                        UdpardTransferKind      transfer_type,
                                                        UdpardPortID            port,
                                                        UdpardNodeID            node) noexcept
    {
        // Iterate through publication records
        // Active publication records ('remote_node_id' field is set) are listed before inactive ones ('remote_node_id'
        // field is anonymous) with the same port ID
        // If a record's 'port_id' and 'remote_node_id' match the argument values or, if no active match was found and
        // this is the first inactive record with a matching port ID, return it
        for (UdpardTransferMetadata& record : records)
        {
            if ((record.port_id == port) &&
                (record.transfer_kind == transfer_type) &&
                ((record.remote_node_id == node) || (record.remote_node_id == AnonymousNodeID)))
            {
                return &record;
            }
        }

        // No inactive record with a matching port ID was found
        return nullptr;
    }

    /// @brief Publishes a serialized cyphal transfer to UDP
    /// @param[in] metadata Metadata for udpard transfer containing information like transfer ID, source node ID, etc.
    /// @param[out] out_transfer Payload span to store data into
    Status publishTransfer(const UdpardTransferMetadata& metadata, const Message& out_transfer)
    {
        // Push transfer to Udpard TX queue
        UdpardMicrosecond no_timeout = 0;  // Transfers are queued and published in-line here, no timeout necessary
        std::int32_t      push_status =
            udpardTxPush(&udpard_tx_fifo_, &udpard_, no_timeout, &metadata, out_transfer.size(), out_transfer.data());
        Status publish_status = ardStatusToCyphalStatus(static_cast<ArdStatus>(push_status));
        if (publish_status.isSuccess())
        {
            // Pop them from the queue and transmit
            const UdpardTxQueueItem* curr_tx_item = udpardTxPeek(&udpard_tx_fifo_);
            BusStatus                bus_status;
            while (curr_tx_item != nullptr)
            {
                // Transmit frame via driver interfaces
                const media::udp::Frame udpard_frame =
                    media::udp::Frame(static_cast<volatile const std::uint8_t*>(curr_tx_item->frame.payload),
                                      curr_tx_item->frame.payload_size);

                TxMetadata tx_metadata{udpardToLibcyphalTxMetadata(metadata)};
                bus_status.primary = primary_bus_.transmit(tx_metadata, udpard_frame);
                if (backup_bus_ && bus_status.backup.isSuccess())
                {
                    bus_status.backup = backup_bus_->transmit(tx_metadata, udpard_frame);
                }

                // Pop current item, deallocate, then grab next one

                fn_udpard_mem_free_(&udpard_, udpardTxPop(&udpard_tx_fifo_, curr_tx_item));

                curr_tx_item = udpardTxPeek(&udpard_tx_fifo_);
            }
            publish_status.setResult((bus_status.primary.isSuccess() || bus_status.backup.isSuccess())
                                         ? ResultCode::Success
                                         : ResultCode::Failure);
        }

        return publish_status;
    }
};

}  // namespace udp
}  // namespace transport
}  // namespace libcyphal

#endif  // LIBCYPHAL_TRANSPORT_UDP_CYPHAL_UDP_TRANSPORT_HPP_INCLUDED
