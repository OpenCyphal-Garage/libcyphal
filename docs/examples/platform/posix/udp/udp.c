/// This software is distributed under the terms of the MIT License.
/// Copyright (C) OpenCyphal Development Team  <opencyphal.org>
/// Copyright Amazon.com Inc. or its affiliates.
/// SPDX-License-Identifier: MIT
/// Author: Pavel Kirienko <pavel@opencyphal.org>

#include "udp.h"

/// Enable SO_REUSEPORT.
#ifndef _DEFAULT_SOURCE
#    define _DEFAULT_SOURCE  // NOLINT(bugprone-reserved-identifier,cert-dcl37-c,cert-dcl51-cpp)
#endif

#include <fcntl.h>
#include <arpa/inet.h>
#include <sys/socket.h>
#include <unistd.h>
#include <poll.h>
#include <errno.h>
#include <limits.h>

/// This is the value recommended by the Cyphal/UDP specification.
#define OVERRIDE_TTL 16

/// RFC 2474.
#define DSCP_MAX 63

static bool isMulticast(const uint32_t address)
{
    return (address & 0xF0000000UL) == 0xE0000000UL;  // NOLINT(*-magic-numbers)
}

int16_t udpTxInit(UDPTxHandle* const self, const uint32_t local_iface_address)
{
    int16_t res = -EINVAL;
    if ((self != NULL) && (local_iface_address > 0))
    {
        self->fd                 = socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP);
        uint32_t  local_iface_be = htonl(local_iface_address);
        const int ttl            = OVERRIDE_TTL;
        bool      ok             = self->fd >= 0;
        //
        ok = ok && bind(self->fd,
                        (struct sockaddr*) &(struct sockaddr_in){
                            .sin_family = AF_INET,
                            .sin_addr   = {local_iface_be},
                            .sin_port   = 0,
                        },
                        sizeof(struct sockaddr_in)) == 0;
        ok = ok && fcntl(self->fd, F_SETFL, O_NONBLOCK) == 0;
        ok = ok && setsockopt(self->fd, IPPROTO_IP, IP_MULTICAST_TTL, &ttl, sizeof(ttl)) == 0;
        // Specify the egress interface for multicast traffic.
        ok = ok && setsockopt(self->fd, IPPROTO_IP, IP_MULTICAST_IF, &local_iface_be, sizeof(local_iface_be)) == 0;
        if (ok)
        {
            res = 0;
        }
        else
        {
            res = (int16_t) -errno;
            (void) close(self->fd);
            self->fd = -1;
        }
    }
    return res;
}

int16_t udpTxSend(UDPTxHandle* const self,
                  const uint32_t     remote_address,
                  const uint16_t     remote_port,
                  const uint8_t      dscp,
                  const size_t       payload_size,
                  const void* const  payload)
{
    int16_t res = -EINVAL;
    if ((self != NULL) && (self->fd >= 0) && (remote_address > 0) && (remote_port > 0) && (payload != NULL) &&
        (dscp <= DSCP_MAX))
    {
        const int dscp_int = dscp << 2U;  // The 2 least significant bits are used for the ECN field.
        (void) setsockopt(self->fd, IPPROTO_IP, IP_TOS, &dscp_int, sizeof(dscp_int));  // Best effort.
        const ssize_t send_result =
            sendto(self->fd,
                   payload,
                   payload_size,
                   MSG_DONTWAIT,
                   (struct sockaddr*) &(struct sockaddr_in){.sin_family = AF_INET,
                                                            .sin_addr   = {.s_addr = htonl(remote_address)},
                                                            .sin_port   = htons(remote_port)},
                   sizeof(struct sockaddr_in));
        if (send_result == (ssize_t) payload_size)
        {
            res = 1;
        }
        else if ((errno == EAGAIN) || (errno == EWOULDBLOCK))
        {
            res = 0;
        }
        else
        {
            res = (int16_t) -errno;
        }
    }
    return res;
}

void udpTxClose(UDPTxHandle* const self)
{
    if ((self != NULL) && (self->fd >= 0))
    {
        (void) close(self->fd);
        self->fd = -1;
    }
}

int16_t udpRxInit(UDPRxHandle* const self,
                  const uint32_t     local_iface_address,
                  const uint32_t     multicast_group,
                  const uint16_t     remote_port)
{
    int16_t res = -EINVAL;
    if ((self != NULL) && (local_iface_address > 0) && isMulticast(multicast_group) && (remote_port > 0))
    {
        const int reuse = 1;
        self->fd        = socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP);
        bool ok         = self->fd >= 0;
        // Set non-blocking mode.
        ok = ok && (fcntl(self->fd, F_SETFL, O_NONBLOCK) == 0);
        // Allow other applications to use the same Cyphal port as well. This must be done before binding.
        // Failure to do so will make it impossible to run more than one Cyphal/UDP node on the same host.
        ok = ok && (setsockopt(self->fd, SOL_SOCKET, SO_REUSEADDR, &reuse, sizeof(reuse)) == 0);
#ifdef SO_REUSEPORT  // Linux
        ok = ok && (setsockopt(self->fd, SOL_SOCKET, SO_REUSEPORT, &reuse, sizeof(reuse)) == 0);
#endif
        // Binding to the multicast group address is necessary on GNU/Linux: https://habr.com/ru/post/141021/
        // Binding to a multicast address is not allowed on Windows, and it is not necessary there;
        // instead, one should bind to INADDR_ANY with the specific port.
        const struct sockaddr_in bind_addr = {
            .sin_family = AF_INET,
#ifdef _WIN32
            .sin_addr = {.s_addr = INADDR_ANY},
#else
            .sin_addr = {.s_addr = htonl(multicast_group)},
#endif
            .sin_port = htons(remote_port),
        };
        ok = ok && (bind(self->fd, (struct sockaddr*) &bind_addr, sizeof(bind_addr)) == 0);
        // INADDR_ANY in IP_ADD_MEMBERSHIP doesn't actually mean "any", it means "choose one automatically";
        // see https://tldp.org/HOWTO/Multicast-HOWTO-6.html. This is why we have to specify the interface explicitly.
        // This is needed to inform the networking stack of which local interface to use for IGMP membership reports.
        const struct in_addr tuple[2] = {{.s_addr = htonl(multicast_group)}, {.s_addr = htonl(local_iface_address)}};
        ok = ok && (setsockopt(self->fd, IPPROTO_IP, IP_ADD_MEMBERSHIP, &tuple[0], sizeof(tuple)) == 0);
        if (ok)
        {
            res = 0;
        }
        else
        {
            res = (int16_t) -errno;
            (void) close(self->fd);
            self->fd = -1;
        }
    }
    return res;
}

int16_t udpRxReceive(UDPRxHandle* const self, size_t* const inout_payload_size, void* const out_payload)
{
    int16_t res = -EINVAL;
    if ((self != NULL) && (self->fd >= 0) && (inout_payload_size != NULL) && (out_payload != NULL))
    {
        const ssize_t recv_result = recv(self->fd, out_payload, *inout_payload_size, MSG_DONTWAIT);
        if (recv_result >= 0)
        {
            *inout_payload_size = (size_t) recv_result;
            res                 = 1;
        }
        else if ((errno == EAGAIN) || (errno == EWOULDBLOCK))
        {
            res = 0;
        }
        else
        {
            res = (int16_t) -errno;
        }
    }
    return res;
}

void udpRxClose(UDPRxHandle* const self)
{
    if ((self != NULL) && (self->fd >= 0))
    {
        (void) close(self->fd);
        self->fd = -1;
    }
}

int16_t udpWait(const uint64_t        timeout_usec,
                const size_t          tx_count,
                UDPTxAwaitable* const tx,
                const size_t          rx_count,
                UDPRxAwaitable* const rx)
{
    int16_t       res         = -EINVAL;
    const size_t  total_count = tx_count + rx_count;
    struct pollfd fds[total_count];  // Violates MISRA-C:2012 Rule 18.8; replace with a fixed limit.
    // IEEE Std 1003.1 requires:
    //
    //  The implementation shall support one or more programming environments in which the width of nfds_t is
    //  no greater than the width of type long.
    //
    // Per C99, the minimum size of "long" is 32 bits, hence we compare against INT32_MAX.
    // OPEN_MAX is not used because it is not guaranteed to be defined nor the limit has to be static.
    if ((tx != NULL) && (rx != NULL) && (total_count > 0) && (total_count <= INT32_MAX))
    {
        {
            size_t idx = 0;
            for (; idx < tx_count; idx++)
            {
                fds[idx].fd     = tx[idx].handle->fd;
                fds[idx].events = POLLOUT;
            }
            for (; idx < tx_count + rx_count; idx++)
            {
                fds[idx].fd     = rx[idx - tx_count].handle->fd;
                fds[idx].events = POLLIN;
            }
        }
        const uint64_t timeout_ms = timeout_usec / 1000U;
        const int poll_result = poll(fds, (nfds_t) total_count, (int) ((timeout_ms > INT_MAX) ? INT_MAX : timeout_ms));
        if (poll_result >= 0)
        {
            res        = 0;
            size_t idx = 0;
            for (; idx < tx_count; idx++)
            {
                tx[idx].ready = (fds[idx].revents & POLLOUT) != 0;  // NOLINT(*-signed-bitwise)
            }
            for (; idx < tx_count + rx_count; idx++)
            {
                rx[idx - tx_count].ready = (fds[idx].revents & POLLIN) != 0;  // NOLINT(*-signed-bitwise)
            }
        }
        else
        {
            res = (int16_t) -errno;
        }
    }
    return res;
}

uint32_t udpParseIfaceAddress(const char* const address)
{
    uint32_t out = 0;
    if (address != NULL)
    {
        struct in_addr addr;
        if (inet_pton(AF_INET, address, &addr) == 1)
        {
            out = ntohl(addr.s_addr);
        }
    }
    return out;
}
