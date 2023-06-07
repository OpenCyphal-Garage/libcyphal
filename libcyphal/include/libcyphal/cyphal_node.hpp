/// @copyright Copyright Amazon.com Inc. and its affiliates. All Rights Reserved.
/// @file
/// The internal implementation of the Node

#ifndef LIBCYPHAL_CYPHAL_NODE_HPP_INCLUDED
#define LIBCYPHAL_CYPHAL_NODE_HPP_INCLUDED

#include "libcyphal/types/time.hpp"

namespace libcyphal
{
/// @todo Implement this
/*
    Node(const time::Timer& timer) noexcept
        : timer_{timer}
        , transport_list_{}
        , publisher_list_{}
        , periodic_list_{}
        , subscriber_list_{}
        , registrar_{}
        , is_ready_for_initialization_{false}
        , is_initialized_{false}
        // Node
        , informant_{nullptr}
        , is_informed_{false}
        , mode_{Node::Mode::kInitialization}
        , health_{Node::Health::kWarning}
        , vssc_{0u}
        , sw_version_{0, 0}
        , hw_version_{0, 0}
        , sw_revision_{0u}
        , crc_64_we_{0u}
        , node_uid_{}
        , node_name_{}
        , certificate_{}
        // Dynamic ID
        , node_id_type_{Discovery::Type::kUnassigned}
        , discovery_{} {}

// Node::Initialize() override
Status Node::Initialize(Informant& informant, Discovery& discovery, serialized::Transport& transport) {
    // called during Application setup states
    // The superloop is available.
    if (not is_registered_) {
        // the sequence isn't being respected somehow
        return make_status<result_e::NOT_READY, cause_e::FINITE_STATE_MACHINE>();
    }
    if (is_ready_for_initialization_ or is_initialized_) {
        // we can't be called twice
        return make_status<result_e::ALREADY_INITIALIZED, cause_e::FINITE_STATE_MACHINE>();
    }
    // Cache reference to the informant and executor implementations
    informant_ = &informant;
    // Cache reference to the discovery implementation
    discovery_ = &discovery;
    // Cache reference to the transport lookup
    transport_ = &transport;
    // enable the small state machine to start asking the informant and the discovery questions.
    is_ready_for_initialization_ = true;
    // Register all of the default messages with this transport
    transport_.RegisterPublication(uavcan_node_Heartbeat_1_0_FIXED_PORT_ID_, kTransferKindMessage);
    transport_.RegisterPublication(TODO, kTransferKindMessage);
    transport_.RegisterSubscription(TODO, kTransferKindRequest);
    transport_.RegisterSubscription(TODO, kTransferKindRequest);
    return Status{};
}

Status Node::SetHealth(Node::Health h) {
    health_ = h;
    // FIXME report!
    return Status{};
}


Status Node::SetTargetMode(Node::Mode mode) {
    if (mode == Node::Mode::kInitialization) {
        // can't go back to initializing
        return make_status<result_e::NOT_ALLOWED, cause_e::FINITE_STATE_MACHINE>();
    } else if (mode_ != Node::Mode::kInitialization) {
        // if we're already in another state we can't switch out, we must reset
        return make_status<result_e::NOT_ALLOWED, cause_e::FINITE_STATE_MACHINE>();
    }
    if (not is_initialized_ or not is_informed_) {
        // stack isn't initialized correctly
        return make_status<result_e::NOT_INITIALIZED, cause_e::RESOURCE>();
    }
    auto status = stack_.CloseRegistration();
    if (status.is_success()) {
        mode_ = mode;
        return Status();
    }
    return status;
}

void Node::PerformDiscovery(void) {
    if (is_ready_for_initialization_ and not is_initialized_) {
        if (not is_informed_ and informant_) {
            // Informant procedure
            Informant& informant = *informant_; // convenience reference
            if (informant.GetStatus().is_success()) {
                hw_version_ = informant.GetHardwareVersion();
                sw_version_ = informant.GetSoftwareVersion();
                sw_revision_ = informant.GetSoftwareRevision();
                crc_64_we_ = informant.GetSoftwareCRC();
                node_name_ = informant.GetName();
                node_uid_ = informant.GetUniqueId();
                certificate_ = informant_->GetCertificateOfAuthority();
                health_ = Node::Health::kNominal; // start nominal, be informed of changes later
                is_informed_ = true;
            }
            // Otherwise we'll come back later
        }
        if (is_informed_ and discovery_ and (serialized::number_of_cyphal_transports > 0)) {
            // Discovery procedure
            Status discovery_status = discovery_->GetStatus();
            if (discovery_status == result_e::NOT_AVAILABLE) {
                // Make sure the discovery process has been started
                discovery_status = make_status<result_e::BUSY, cause_e::SESSION>(); // Report discovery as busy
                Status start_status = discovery_->Start();
                if (start_status.is_success()) {
                    // Good!
                    // Keep BUSY state set above until 'Discovery::GetStatus()' reports SUCCESS
                } else if (start_status.is_not_ready()) {
                    // Cache start status in discovery status to report NOT_READY state and come back later
                    discovery_status = start_status;
                } else { // if (start_status == result_e::ALREADY_INITIALIZED) or error
                    Status::get_log().write(start_status);

                    // Cache start status in discovery status if it is a suspected error
                    if (start_status != result_e::ALREADY_INITIALIZED) {
                        discovery_status = start_status;
                    }

                    // Otherwise keep BUSY state from above until 'Discovery::GetStatus()' reports SUCCESS
                }
            } else if (discovery_status.is_busy()) {
                // Discovery in progress, come back later
            } else {
                // Discovery success or failure
                if (discovery_status.is_failure()) {
                    // Log failure
                    Status::log(discovery_status);
                }
                else {
                    NodeID node_id = discovery_->GetID(transport_name);
                    transport_->SetNodeID(node_id);
                }
            }

            // If the discovery has finished its procedure we may not re-enter this block
            if (!discovery_status.is_busy() && !discovery_status.is_not_ready()) {
                // Set initialized flag based on discovery success
                is_initialized_ = (discovery_status.is_success());
                if (is_initialized) {
                    print("Node: %s up\r\n", node_name_.data());
                }
            }
        }
    }
}

void Node::Execute(void) {
    if (is_initialized_) {
        ProcessIncoming();
        ProcessOutgoing();
    }
    else {
        PerformDiscovery();
    }
}

/// Processes all incoming transport message traffic
void Node::ProcessIncoming(void) {
    // process all incoming messages and distribute them as needed.
    for (auto& listener : listeners_list_) {
        transport_.ProcessIncomingTransfers(listener);
    }
}

// Transport::Listener implementation. Handles default and required Cyphal messages
void Node::OnReceive(RxMetadata rx_metadata, const Message& msg) {
    size_t serialized_size = 0u;
    if (rx_metadata.kind == kTransferKindRequest) {
        // Handle responding to any requests
        switch (rx_metadata.port_id) {
            case uavcan_node_GetInfo_1_0_FIXED_PORT_ID_:
            {
                static uavcan_node_GetInfo_Response_1_0 response;
                // put this out in global memory but don't let anyone see it symbolically
                static uint8_t serialized_blob[uavcan_node_GetInfo_Response_1_0_EXTENT_BYTES_];
                response.protocol_version.major = 1; // FIXME ?
                response.protocol_version.minor = 0; // FIXME ?
                response.hardware_version.major = hw_version_.major;
                response.hardware_version.minor = hw_version_.minor;
                response.software_version.major = sw_version_.major;
                response.software_version.minor = sw_version_.minor;
                response.software_vcs_revision_id = sw_revision_;
                memcpy(response.unique_id, node_uid_.data(), to_underlying(id::Size::UID));
                response.name.count = strlen(node_name_.data());
                strncpy((char *)response.name.elements, node_name_.data(), response.name.count);
                response.software_image_crc.count = 1;
                response.software_image_crc.elements[0] = crc_64_we_;
                response.certificate_of_authenticity.count = certificate_.count(); // FIXME support certificates in the
GetInfo memcpy(response.certificate_of_authenticity.elements, certificate_.data(), certificate_.count());
                serialized_size = uavcan_node_GetInfo_Response_1_0_EXTENT_BYTES_;
                if (0 == uavcan_node_GetInfo_Response_1_0_serialize_(&response, serialized_blob, &serialized_size) and
serialized_size > 0u) { msg.reset(serialized_blob); msg.resize(serialized_size); return Status(); } else { return
make_status_and_log<result_e::NOT_EXPECTED, cause_e::BUFFER>(); // didn't serialize
                }
            }
            case uavcan_node_GetTransportStatistics_0_1_FIXED_PORT_ID_:
            {
                uavcan_node_GetTransportStatistics_Response_0_1 response;
                // put this out in global memory but don't let anyone see it symbolically
                static uint8_t serialized_blob[uavcan_node_GetTransportStatistics_Response_0_1_EXTENT_BYTES_];
                // FIXME AVF-298 query transports for information
                response.transfer_statistics.num_emitted = 0;
                response.transfer_statistics.num_errored = 0;
                response.transfer_statistics.num_received = 0;
                response.network_interface_statistics.count = 0;
                serialized_size = uavcan_node_GetTransportStatistics_Response_0_1_EXTENT_BYTES_;
                if (0 == uavcan_node_GetTransportStatistics_Response_0_1_serialize_(&response, serialized_blob,
&serialized_size) and serialized_size > 0u) { msg.reset(serialized_blob); msg.resize(serialized_size); return Status();
                } else {
                    return make_status_and_log<result_e::NOT_EXPECTED, cause_e::BUFFER>(); // didn't serialize
                }
            }
            default:
                break;
        }
    }
    else if (rx_metadata.kind == kTransferKindResponse) {
        // We don't care about any responses for now
        return Status{};
    }
    else if (rx_metadata.kind == kTransferKindMessage) {
        // Handle receiving broadcasts
        size_t serialized_size = 0u;
        switch(rx_metadata.port_id) {
            default:
                // No default broadcasts to receive
                break;
        }
    }
    return Status{TODO: Unknown transfer kind status};

}

Status Node::ProcessOutgoing(void) {
    cyphal::serialized::Message msg;
    if (TODO: Heartbeat ready to publish) {
        uavcan_node_Heartbeat_1_0 hb;
        // put this out in global memory but don't let anyone see it symbolically
        static uint8_t serialized_blob[uavcan_node_Heartbeat_1_0_EXTENT_BYTES_];
        hb.uptime = timer_.get_time_us().us() / 1'000'000u; // FIXME should there be a get_time_sec()?
        hb.health.value = to_underlying(health_);
        hb.mode.value = to_underlying(mode_);
        hb.vendor_specific_status_code = vssc_;
        serialized_size = uavcan_node_Heartbeat_1_0_EXTENT_BYTES_;
        if (0 == uavcan_node_Heartbeat_1_0_serialize_(&hb, serialized_blob, &serialized_size) and serialized_size > 0u)
{ msg.reset(serialized_blob); msg.resize(serialized_size); return Status{}; } else { return
make_status_and_log<result_e::NOT_EXPECTED, cause_e::BUFFER>(); // didn't serialize
        }
        TxMetadata tx_metadata;
        // TODO: fill in metadata
        Publish(tx_metadata, msg);
    }
    if (TODO: port list ready to publish) {
        // FIXME this is too big to put on the stack!
        static uavcan_node_port_List_0_1 list;
        uavcan_node_port_List_0_1_initialize_(&list);

        // FIXME populate this List with Message types which are NOT in the standard (SubjectID < 1024, ServiceID < 512)
        // FIXME add amazon.avfw.Sync!
        // FIXME add any other custom types!
        // we get to decide if a sparse list is better to send than a bit mask.

        // put this out in global memory but don't let anyone see it symbolically
        static uint8_t serialized_blob[uavcan_node_port_List_0_1_EXTENT_BYTES_];
        // FIXME this should come from the static lists!
        serialized_size = uavcan_node_port_List_0_1_EXTENT_BYTES_;
        if (0 == uavcan_node_port_List_0_1_serialize_(&list, serialized_blob, &serialized_size) and serialized_size >
0u) { msg.reset(serialized_blob); msg.resize(serialized_size); return Status{}; } else { return
make_status_and_log<result_e::NOT_EXPECTED, cause_e::BUFFER>(); // didn't serialize
        }
        TxMetadata tx_metadata;
        // TODO: fill in metadata
        Publish(tx_metadata, msg);
    }
    return Status{};
}

Status Node::Publish(TxMetadata tx_metadata, const Message& msg) {
    transport_.Transmit(tx_metadata, msg);
}

Status Node::AddListener(serialized::Transport::Listener& listener) {
    if (listener_list_.is_full) {
        return Status{TODO: add full status};
    }
    return (listener_list_.emplace_back(listener)) ? Status{} : Status{TODO: add error status};
}

void Node::OnReceiveRequestFromTransport(ServiceID type, const RxMetadata& rx_metadata, const types::NodeID&
source_node_id, const Message& msg) {
    // Check if the request if destined for our NodeID, if not let's figure out where it goes
    if (rx_metadata.remote_node_id == node_id_) {
        // This is our node ID, so check which responder will handle it
        for (auto& responder : responder_list) {
            if (responder.service_id == type) {
                responder.OnReceiveRequest(requester, msg, *this);  // TODO: fix params
                break; // 1 handler to 1 service id
            }
        }
    }
    else {
        // We need to check if the destination node ID is on a different Transport. If so, we need to forward this
        UniqueRoutingId uid;
        uid.source_node_id = rx_metadata.remote_node_id; // TODO: Need func to get source node from frame and add here
        uid.port_id = rx_metadata.port_id;
        uid.is_service_message = true;
        uid.has_source_node = true;
        serialized::Transport::MaskType mask = transport_lookup_.GetDestinationTransports(uid);
    }
}
*/
}  // namespace libcyphal

#endif  // LIBCYPHAL_CYPHAL_NODE_HPP_INCLUDED
