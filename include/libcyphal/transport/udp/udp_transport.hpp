/// @copyright
/// Copyright (C) OpenCyphal Development Team  <opencyphal.org>
/// Copyright Amazon.com Inc. or its affiliates.
/// SPDX-License-Identifier: MIT

#ifndef LIBCYPHAL_TRANSPORT_UDP_TRANSPORT_HPP_INCLUDED
#define LIBCYPHAL_TRANSPORT_UDP_TRANSPORT_HPP_INCLUDED

#include "libcyphal/transport/errors.hpp"
#include "libcyphal/transport/transport.hpp"
#include "media.hpp"

#include <cetl/pf17/cetlpf.hpp>
#include <cetl/pmr/function.hpp>
#include <udpard.h>

#include <cstdint>

namespace libcyphal
{
namespace transport
{
namespace udp
{

/// @brief Defines interface of UDP transport layer.
///
class IUdpTransport : public ITransport
{
public:
    /// Defines structure for reporting transient transport errors to the user's handler.
    ///
    /// In addition to the error itself, it provides:
    /// - Index of media interface related to this error.
    ///   This index is the same as the index of the (not `nullptr`!) media interface
    ///   pointer in the `media` span argument used at the `makeTransport()` factory method.
    /// - A reference to the entity that has caused this error.
    ///
    struct TransientErrorReport
    {
        /// @brief Error report about publishing a message to a TX session.
        struct UdpardTxPublish
        {
            AnyError     error;
            std::uint8_t media_index;
            UdpardTx&    culprit;
        };

        /// @brief Error report about pushing a service request to a TX session.
        struct UdpardTxRequest
        {
            AnyError     error;
            std::uint8_t media_index;
            UdpardTx&    culprit;
        };

        /// @brief Error report about pushing a service respond to a TX session.
        struct UdpardTxRespond
        {
            AnyError     error;
            std::uint8_t media_index;
            UdpardTx&    culprit;
        };

        /// @brief Error report about making TX socket by the media interface.
        struct MediaMakeTxSocket
        {
            AnyError     error;
            std::uint8_t media_index;
            IMedia&      culprit;
        };

        /// Defines variant of all possible transient error reports.
        ///
        using Variant = cetl::variant<UdpardTxPublish, UdpardTxRequest, UdpardTxRespond, MediaMakeTxSocket>;

    };  // TransientErrorReport

    /// @brief Defines signature of a transient error handler.
    ///
    /// If set, this handler is called by the transport layer when a transient media related error occurs during
    /// transport's (or any of its sessions) `run` method. A TX session `send` method may also trigger this handler.
    ///
    /// Note that there is a limited set of things that can be done within this handler, f.e.:
    /// - it's not allowed to call transport's (or its session's) `run` method from within this handler;
    /// - it's not allowed to call a TX session `send` or RX session `receive` methods from within this handler;
    /// - main purpose of the handler:
    ///   - is to log/report/stat the error;
    ///   - potentially modify state of some "culprit" media related component;
    ///   - return an optional (maybe different) error back to the transport.
    /// - result error from the handler affects:
    ///   - whether or not other redundant media of this transport will continue to be processed
    ///     as part of this current "problematic" run (see description of return below),
    ///   - propagation of the error up to the original user's call (result of the `run` or `send` methods).
    ///
    /// @param report The error report to be handled. It's made as non-const ref to allow the handler modify it,
    ///               and f.e. reuse original `.error` field value by moving it as is to return result.
    /// @return An optional (maybe different) error back to the transport.
    ///         - If `cetl::nullopt` is returned, the original error (in the `report`) is considered as handled
    ///           and insignificant for the transport. Transport will continue its current process (effectively
    ///           either ignoring such transient failure, or retrying the process later on its next run).
    ///         - If an error is returned, the transport will immediately stop current process, won't process any
    ///           other media (if any), and propagate the returned error to the user (as result of `run` or etc).
    ///
    using TransientErrorHandler =
        cetl::pmr::function<cetl::optional<AnyError>(TransientErrorReport::Variant& report_var), sizeof(void*) * 3>;

    IUdpTransport(const IUdpTransport&)                = delete;
    IUdpTransport(IUdpTransport&&) noexcept            = delete;
    IUdpTransport& operator=(const IUdpTransport&)     = delete;
    IUdpTransport& operator=(IUdpTransport&&) noexcept = delete;

    /// Sets new transient error handler.
    ///
    /// - If the handler is set, it will be called by the transport layer when a transient media related error occurs,
    ///   and it's up to the handler to decide what to do with the error, and whether to continue or stop the process.
    /// - If the handler is not set (default mode), the transport will treat this transient error as "serious" one,
    ///   and immediately stop its current process (its `run` or TX session's `send` method) and propagate the error.
    /// See \ref TransientErrorHandler for more details.
    ///
    virtual void setTransientErrorHandler(TransientErrorHandler handler) = 0;

protected:
    IUdpTransport()  = default;
    ~IUdpTransport() = default;

};  // IUdpTransport

/// @brief Specifies set of memory resources used by the UDP transport.
///
struct MemoryResourcesSpec
{
    /// The general purpose memory resource is used to provide memory for the libcyphal library.
    /// It is NOT used for any Udpard TX or RX transfers, payload (de)fragmentation or transient handles,
    /// but only for the libcyphal internal needs (like `make*[Rx|Tx]Session` factory calls).
    cetl::pmr::memory_resource& general;

    /// The session memory resource is used to provide memory for the Udpard session instances.
    /// Each instance is fixed-size, so a trivial zero-fragmentation block allocator is sufficient.
    /// If `nullptr` then the `.general` memory resource will be used instead.
    cetl::pmr::memory_resource* session{nullptr};

    /// The fragment handles are allocated per payload fragment; each handle contains a pointer to its fragment.
    /// Each instance is of a very small fixed size, so a trivial zero-fragmentation block allocator is sufficient.
    /// If `nullptr` then the `.general` memory resource will be used instead.
    cetl::pmr::memory_resource* fragment{nullptr};

    /// The library never allocates payload buffers itself, as they are handed over by the application via
    /// receive calls. Once a buffer is handed over, the library may choose to keep it if it is deemed to be
    /// necessary to complete a transfer reassembly, or to discard it if it is deemed to be unnecessary.
    /// Discarded payload buffers are freed using this memory resource.
    /// If `nullptr` then the `.general` memory resource will be used instead.
    cetl::pmr::memory_resource* payload{nullptr};

};  // MemoryResourcesSpec

}  // namespace udp
}  // namespace transport
}  // namespace libcyphal

#endif  // LIBCYPHAL_TRANSPORT_UDP_TRANSPORT_HPP_INCLUDED
