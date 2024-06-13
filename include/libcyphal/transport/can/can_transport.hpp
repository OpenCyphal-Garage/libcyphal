/// @copyright
/// Copyright (C) OpenCyphal Development Team  <opencyphal.org>
/// Copyright Amazon.com Inc. or its affiliates.
/// SPDX-License-Identifier: MIT

#ifndef LIBCYPHAL_TRANSPORT_CAN_TRANSPORT_HPP_INCLUDED
#define LIBCYPHAL_TRANSPORT_CAN_TRANSPORT_HPP_INCLUDED

#include "libcyphal/transport/can/media.hpp"
#include "libcyphal/transport/errors.hpp"
#include "libcyphal/transport/transport.hpp"

#include <canard.h>
#include <cetl/pf17/cetlpf.hpp>
#include <cetl/pmr/function.hpp>

#include <cstdint>

namespace libcyphal
{
namespace transport
{
namespace can
{

/// @brief Defines interface of CAN transport layer.
///
class ICanTransport : public ITransport
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
        /// @brief Error report about pushing a message to a TX session.
        struct CanardTxPush
        {
            AnyError        error;
            std::uint8_t    media_index;
            CanardInstance& culprit;
        };

        /// @brief Error report about accepting a frame for an RX session.
        struct CanardRxAccept
        {
            AnyError        error;
            std::uint8_t    media_index;
            CanardInstance& culprit;
        };

        /// @brief Error report about receiving frame from the media interface.
        struct MediaPop
        {
            AnyError     error;
            std::uint8_t media_index;
            IMedia&      culprit;
        };

        /// @brief Error report about pushing a frame to the media interface.
        struct MediaPush
        {
            AnyError     error;
            std::uint8_t media_index;
            IMedia&      culprit;
        };

        /// @brief Error report about configuring the media interface (f.e. applying filters).
        struct MediaConfig
        {
            AnyError     error;
            std::uint8_t media_index;
            IMedia&      culprit;
        };

        /// Defines variant of all possible transient error reports.
        ///
        using Variant = cetl::variant<CanardTxPush, CanardRxAccept, MediaPop, MediaPush, MediaConfig>;

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
    ///   - potentially modify state of some "culprit" media related component (f.e. reset HW CAN controller);
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

    ICanTransport(const ICanTransport&)                = delete;
    ICanTransport(ICanTransport&&) noexcept            = delete;
    ICanTransport& operator=(const ICanTransport&)     = delete;
    ICanTransport& operator=(ICanTransport&&) noexcept = delete;

    /// Sets new transient error handler.
    ///
    /// If the handler is set, it will be called by the transport layer when a transient media related error occurs.
    /// If the handler is not set (default mode), the transport will ignore such errors and continue
    /// its current process in a "best-effort" manner, namely in assumption that
    /// either other redundant media (if any) will deliver what is needed,
    /// or later retry (aka next `run`) of the operation will resolve the issue.
    /// See \ref TransientErrorHandler for more details.
    ///
    virtual void setTransientErrorHandler(TransientErrorHandler handler) = 0;

protected:
    ICanTransport()  = default;
    ~ICanTransport() = default;
};

}  // namespace can
}  // namespace transport
}  // namespace libcyphal

#endif  // LIBCYPHAL_TRANSPORT_CAN_TRANSPORT_HPP_INCLUDED
