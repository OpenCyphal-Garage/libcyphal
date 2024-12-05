///                            ____                   ______            __          __
///                           / __ `____  ___  ____  / ____/_  ______  / /_  ____  / /
///                          / / / / __ `/ _ `/ __ `/ /   / / / / __ `/ __ `/ __ `/ /
///                         / /_/ / /_/ /  __/ / / / /___/ /_/ / /_/ / / / / /_/ / /
///                         `____/ .___/`___/_/ /_/`____/`__, / .___/_/ /_/`__,_/_/
///                             /_/                     /____/_/
///
/// This is a basic adapter library that bridges Libcanard with SocketCAN.
/// Read the API documentation for usage information.
///
/// To integrate the library into your application, just copy-paste the c/h files into your project tree.
///
/// --------------------------------------------------------------------------------------------------------------------
/// Changelog
///
/// v3.0 - Update for compatibility with Libcanard v3.
///
/// v2.0 - Added loop-back functionality.
///        API change in socketcanPop(): loopback flag added.
///      - Changed to kernel-based time-stamping for received frames for improved accuracy.
///        API change in socketcanPop(): time stamp clock source is now CLOCK_REALTIME, vs CLOCK_TAI before.
///
/// v1.0 - Initial release
/// --------------------------------------------------------------------------------------------------------------------
///
/// This software is distributed under the terms of the MIT License.
/// Copyright (c) 2020 OpenCyphal
/// Author: Pavel Kirienko <pavel.kirienko@zubax.com>

#ifndef SOCKETCAN_H_INCLUDED
#define SOCKETCAN_H_INCLUDED

#include "canard.h"

#include <stdbool.h>
#include <stdint.h>
#include <stdlib.h>

#ifdef __cplusplus
extern "C" {
#endif

/// File descriptor alias.
typedef int SocketCANFD;

/// Initialize a new non-blocking (sic!) SocketCAN socket and return its handle on success.
/// On failure, a negated errno is returned.
/// To discard the socket just call close() on it; no additional de-initialization activities are required.
/// The argument can_fd enables support for CAN FD frames.
SocketCANFD socketcanOpen(const char* const iface_name, const bool can_fd);

/// Enqueue a new extended CAN data frame for transmission.
/// Block until the frame is enqueued or until the timeout is expired.
/// Zero timeout makes the operation non-blocking.
/// Returns 1 on success, 0 on timeout, negated errno on error.
int16_t socketcanPush(const SocketCANFD fd, const struct CanardFrame* const frame, const CanardMicrosecond timeout_usec);

/// Fetch a new extended CAN data frame from the RX queue.
/// If the received frame is not an extended-ID data frame, it will be dropped and the function will return early.
/// The payload pointer of the returned frame will point to the payload_buffer. It can be a stack-allocated array.
/// The payload_buffer_size shall be large enough (64 bytes is enough for CAN FD), otherwise an error is returned.
/// The received frame timestamp will be set to CLOCK_REALTIME by the kernel, sampled near the moment of its arrival.
/// The loopback flag pointer is used to both indicate and control behavior when a looped-back message is received.
/// If the flag pointer is NULL, loopback frames are silently dropped; if not NULL, they are accepted and indicated
/// using this flag.
/// The function will block until a frame is received or until the timeout is expired. It may return early.
/// Zero timeout makes the operation non-blocking.
/// Returns 1 on success, 0 on timeout, negated errno on error.
int16_t socketcanPop(const SocketCANFD        fd,
                     struct CanardFrame* const       out_frame,
                     CanardMicrosecond* const out_timestamp_usec,
                     const size_t             payload_buffer_size,
                     void* const              payload_buffer,
                     const CanardMicrosecond  timeout_usec,
                     bool* const              loopback);

/// Apply the specified acceptance filter configuration.
/// Note that it is only possible to accept extended-format data frames.
/// The default configuration is to accept everything.
/// Returns 0 on success, negated errno on error.
int16_t socketcanFilter(const SocketCANFD fd, const size_t num_configs, const struct CanardFilter* const configs);

#ifdef __cplusplus
}
#endif

#endif
