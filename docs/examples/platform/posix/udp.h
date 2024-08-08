///                            ____                   ______            __          __
///                           / __ `____  ___  ____  / ____/_  ______  / /_  ____  / /
///                          / / / / __ `/ _ `/ __ `/ /   / / / / __ `/ __ `/ __ `/ /
///                         / /_/ / /_/ /  __/ / / / /___/ /_/ / /_/ / / / / /_/ / /
///                         `____/ .___/`___/_/ /_/`____/`__, / .___/_/ /_/`__,_/_/
///                             /_/                     /____/_/
///
/// This module implements the platform-specific implementation of the UDP transport. On a conventional POSIX system
/// this would be a thin wrapper around the standard Berkeley sockets API. On a bare-metal system this would be
/// a thin wrapper around the platform-specific network stack, such as LwIP, or a custom solution.
///
/// Having the interface extracted like this helps better illustrate the surface of the networking API required
/// by LibUDPard, which is minimal. This also helps with porting to new platforms.
///
/// All addresses and values used in this API are in the host-native byte order.
/// For example, 127.0.0.1 is represented as 0x7F000001 always.
///
/// This software is distributed under the terms of the MIT License.
/// Copyright (C) OpenCyphal Development Team  <opencyphal.org>
/// Copyright Amazon.com Inc. or its affiliates.
/// SPDX-License-Identifier: MIT
/// Author: Pavel Kirienko <pavel@opencyphal.org>

#pragma once

#include <stdbool.h>
#include <stdint.h>
#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

/// These definitions are highly platform-specific.
/// Note that LibUDPard does not require the same socket to be usable for both transmission and reception.
typedef struct
{
    int fd;
} UDPTxHandle;
typedef struct
{
    int fd;
} UDPRxHandle;

/// Initialize a TX socket for use with LibUDPard.
/// The local iface address is used to specify the egress interface for multicast traffic.
/// Per LibUDPard design, there is one TX socket per redundant interface, so the application needs to invoke
/// this function once per interface.
/// On error returns a negative error code.
int16_t udpTxInit(UDPTxHandle* const self, const uint32_t local_iface_address);

/// Send a datagram to the specified endpoint without blocking using the specified IP DSCP field value.
/// A real-time embedded system should normally accept a transmission deadline here for the networking stack.
/// Returns 1 on success, 0 if the socket is not ready for sending, or a negative error code.
int16_t udpTxSend(UDPTxHandle* const self,
                  const uint32_t     remote_address,
                  const uint16_t     remote_port,
                  const uint8_t      dscp,
                  const size_t       payload_size,
                  const void* const  payload);

/// No effect if the argument is invalid.
/// This function is guaranteed to invalidate the handle.
void udpTxClose(UDPTxHandle* const self);

/// Initialize an RX socket for use with LibUDPard, for subscription to subjects or for RPC traffic.
/// The socket will be bound to the specified multicast group and port.
/// Most socket APIs, in particular the Berkeley sockets, require the local iface address to be known,
/// because it is used to decide which egress port to send IGMP membership reports over.
/// On error returns a negative error code.
int16_t udpRxInit(UDPRxHandle* const self,
                  const uint32_t     local_iface_address,
                  const uint32_t     multicast_group,
                  const uint16_t     remote_port);

/// Read one datagram from the socket without blocking.
/// The size of the destination buffer is specified in inout_payload_size; it is updated to the actual size of the
/// received datagram upon return.
/// Returns 1 on success, 0 if the socket is not ready for reading, or a negative error code.
int16_t udpRxReceive(UDPRxHandle* const self, size_t* const inout_payload_size, void* const out_payload);

/// No effect if the argument is invalid.
/// This function is guaranteed to invalidate the handle.
void udpRxClose(UDPRxHandle* const self);

/// Auxiliary types for use with the I/O multiplexing function.
/// The "ready" flag is updated to indicate whether the handle is ready for I/O.
/// The "user_*" fields can be used for user-defined purposes.
typedef struct
{
    UDPTxHandle* handle;
    bool         ready;
    void*        user_reference;
} UDPTxAwaitable;
typedef struct
{
    UDPRxHandle* handle;
    bool         ready;
    void*        user_reference;
} UDPRxAwaitable;

/// Suspend execution until the expiration of the timeout (in microseconds) or until any of the specified handles
/// become ready for reading (the RX group) or writing (the TX group).
/// The function may return earlier than the timeout even if no handles are ready.
/// On error returns a negative error code.
int16_t udpWait(const uint64_t        timeout_usec,
                const size_t          tx_count,
                UDPTxAwaitable* const tx,
                const size_t          rx_count,
                UDPRxAwaitable* const rx);

/// Convert an interface address from string to binary representation; e.g., "127.0.0.1" --> 0x7F000001.
/// Returns zero if the address is not recognized.
uint32_t udpParseIfaceAddress(const char* const address);

int udpGetSocketOptionInt(const int fd, const int option);
int udpSetSocketOptionInt(const int fd, const int option, const int value);

#ifdef __cplusplus
}
#endif
