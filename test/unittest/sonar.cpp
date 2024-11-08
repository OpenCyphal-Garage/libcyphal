/// @copyright
/// Copyright (C) OpenCyphal Development Team  <opencyphal.org>
/// Copyright Amazon.com Inc. or its affiliates.
/// SPDX-License-Identifier: MIT

#include <libcyphal/application/node.hpp>
#include "libcyphal/application/node/get_info_provider.hpp"
#include "libcyphal/application/node/heartbeat_producer.hpp"
#include "libcyphal/application/node/registry_provider.hpp"
#include "libcyphal/application/registry/register.hpp"
#include "libcyphal/application/registry/register_impl.hpp"
#include "libcyphal/application/registry/registry.hpp"
#include "libcyphal/application/registry/registry_impl.hpp"
#include <libcyphal/common/cavl/cavl.hpp>
#include <libcyphal/common/crc.hpp>
#include <libcyphal/errors.hpp>
#include <libcyphal/executor.hpp>
#include <libcyphal/platform/single_threaded_executor.hpp>
#include <libcyphal/presentation/client.hpp>
#include <libcyphal/presentation/client_impl.hpp>
#include <libcyphal/presentation/common_helpers.hpp>
#include <libcyphal/presentation/presentation.hpp>
#include <libcyphal/presentation/presentation_delegate.hpp>
#include <libcyphal/presentation/publisher.hpp>
#include <libcyphal/presentation/publisher_impl.hpp>
#include <libcyphal/presentation/response_promise.hpp>
#include <libcyphal/presentation/server.hpp>
#include <libcyphal/presentation/server_impl.hpp>
#include <libcyphal/presentation/shared_object.hpp>
#include <libcyphal/presentation/subscriber.hpp>
#include <libcyphal/presentation/subscriber_impl.hpp>
#include <libcyphal/time_provider.hpp>
#include <libcyphal/transport/can/can_transport.hpp>
#include <libcyphal/transport/can/can_transport_impl.hpp>
#include <libcyphal/transport/can/delegate.hpp>
#include <libcyphal/transport/can/media.hpp>
#include <libcyphal/transport/can/msg_rx_session.hpp>
#include <libcyphal/transport/can/msg_tx_session.hpp>
#include <libcyphal/transport/can/svc_rx_sessions.hpp>
#include <libcyphal/transport/can/svc_tx_sessions.hpp>
#include <libcyphal/transport/contiguous_payload.hpp>
#include <libcyphal/transport/errors.hpp>
#include <libcyphal/transport/msg_sessions.hpp>
#include <libcyphal/transport/scattered_buffer.hpp>
#include <libcyphal/transport/session.hpp>
#include <libcyphal/transport/svc_sessions.hpp>
#include <libcyphal/transport/transfer_id_generators.hpp>
#include <libcyphal/transport/transport.hpp>
#include <libcyphal/transport/types.hpp>
#include <libcyphal/transport/udp/delegate.hpp>
#include <libcyphal/transport/udp/media.hpp>
#include <libcyphal/transport/udp/msg_rx_session.hpp>
#include <libcyphal/transport/udp/msg_tx_session.hpp>
#include <libcyphal/transport/udp/session_tree.hpp>
#include <libcyphal/transport/udp/svc_rx_sessions.hpp>
#include <libcyphal/transport/udp/svc_tx_sessions.hpp>
#include <libcyphal/transport/udp/tx_rx_sockets.hpp>
#include <libcyphal/transport/udp/udp_transport.hpp>
#include <libcyphal/transport/udp/udp_transport_impl.hpp>
#include <libcyphal/types.hpp>

int main()
{
    return 0;
}
