/// @copyright
/// Copyright (C) OpenCyphal Development Team  <opencyphal.org>
/// Copyright Amazon.com Inc. or its affiliates.
/// SPDX-License-Identifier: MIT

#ifndef LIBCYPHAL_CONFIG_HPP_INCLUDED
#define LIBCYPHAL_CONFIG_HPP_INCLUDED

#include <cstddef>

namespace libcyphal
{

// All below NOSONAR cpp:S799 "Rename this identifier to be shorter or equal to 31 characters."
// are intentional and are used to keep the names consistent with the rest of the codebase.
// F.e. `IExecutor_Callback_FunctionMaxSize` is consistent with `IExecutor::Callback::FunctionMaxSize`.

/// Defines various configuration parameters of libcyphal.
///
/// All methods are `static constexpr` - they are evaluated at compile time.
///
/// The Curiously Recurring Template Pattern (CRTP)
/// is used to allow the derived class to override the configuration parameters
/// while keeping the default values in the base class.
///
/// @tparam Derived The derived class that could override some of the configuration parameters.
///
/// Nolint b/c this is the main purpose of this file - define various constants.
/// NOLINTBEGIN(cppcoreguidelines-avoid-magic-numbers, readability-magic-numbers)
///
template <typename Derived>
class Config
{
public:
    /// Defines max footprint of a callback function in use by the executor.
    ///
    static constexpr std::size_t IExecutor_Callback_FunctionMaxSize()  // NOSONAR cpp:S799
    {
        return Derived::IExecutor_Callback_FunctionMaxSize_Impl();
    }

    /// Defines footprint size reserved for a callback implementation.
    /// The actual max footprint for the callback implementation is `sizeof(IExecutor::Function)` bigger,
    /// and it depends on `Cfg_IExecutor_Callback_FunctionMaxSize`.
    ///
    static constexpr std::size_t IExecutor_Callback_ReserveSize()  // NOSONAR cpp:S799
    {
        return Derived::IExecutor_Callback_ReserveSize_Impl();
    }

    /// Defines various configuration parameters for the application layer.
    ///
    class Application
    {
    public:
        class Node
        {
        public:
            /// Defines max footprint of a callback function in use by the heartbeat producer.
            ///
            static constexpr std::size_t HeartbeatProducer_UpdateCallback_FunctionSize()  // NOSONAR cpp:S799
            {
                return Derived::Application::Node::HeartbeatProducer_UpdateCallback_FunctionSize_Impl();
            }

        protected:
            static constexpr std::size_t HeartbeatProducer_UpdateCallback_FunctionSize_Impl()  // NOSONAR cpp:S799
            {
                /// Size is chosen arbitrary, but it should be enough to store any lambda or function pointer.
                return sizeof(void*) * 4;
            }

        };  // Node

    };  // Application

    /// Defines various configuration parameters for the presentation layer.
    ///
    class Presentation
    {
    public:
        /// Defines max footprint of a callback function in use by the RPC client response promise.
        ///
        static constexpr std::size_t ResponsePromiseBase_Callback_FunctionSize()  // NOSONAR cpp:S799
        {
            return Derived::Presentation::ResponsePromiseBase_Callback_FunctionSize_Impl();
        }

        /// Defines max footprint of a callback function in use by the RPC server response continuation.
        ///
        static constexpr std::size_t ServerBase_ContinuationImpl_FunctionMaxSize()  // NOSONAR cpp:S799
        {
            return Derived::Presentation::ServerBase_ContinuationImpl_FunctionMaxSize_Impl();
        }

        /// Defines max footprint of a callback function in use by the RPC server request notification.
        ///
        static constexpr std::size_t ServerBase_OnRequestCallback_FunctionMaxSize()  // NOSONAR cpp:S799
        {
            return Derived::Presentation::ServerBase_OnRequestCallback_FunctionMaxSize_Impl();
        }

        /// Defines the size serialization/deserialization payload buffer which is considered as a small one,
        /// and therefore could be used with stack buffer. Any payload larger than this size will be PMR allocated.
        ///
        /// Setting it to 0 will force all payload buffers to be PMR allocated (except zero-sized).
        ///
        static constexpr std::size_t SmallPayloadSize()
        {
            return Derived::Presentation::SmallPayloadSize_Impl();
        }

        /// Defines max footprint of a callback function in use by the message subscriber receive notification.
        ///
        static constexpr std::size_t Subscriber_OnReceiveCallback_FunctionMaxSize()  // NOSONAR cpp:S799
        {
            return Derived::Presentation::Subscriber_OnReceiveCallback_FunctionMaxSize_Impl();
        }

    protected:
        static constexpr std::size_t ResponsePromiseBase_Callback_FunctionSize_Impl()  // NOSONAR cpp:S799
        {
            /// Size is chosen arbitrary, but it should be enough to store any lambda or function pointer.
            return sizeof(void*) * 4;
        }

        static constexpr std::size_t ServerBase_ContinuationImpl_FunctionMaxSize_Impl()  // NOSONAR cpp:S799
        {
            /// Size is chosen arbitrary, but it should be enough to store any lambda or function pointer.
            return sizeof(void*) * 5;
        }

        static constexpr std::size_t ServerBase_OnRequestCallback_FunctionMaxSize_Impl()  // NOSONAR cpp:S799
        {
            /// Size is chosen arbitrary, but it should be enough to store any lambda or function pointer.
            return sizeof(void*) * 5;
        }

        static constexpr std::size_t SmallPayloadSize_Impl()
        {
            /// Size is chosen arbitrary - as compromise between stack and PMR allocation.
            return 256;
        }

        static constexpr std::size_t Subscriber_OnReceiveCallback_FunctionMaxSize_Impl()  // NOSONAR cpp:S799
        {
            /// Size is chosen arbitrary, but it should be enough to store any lambda or function pointer.
            return sizeof(void*) * 4;
        }
    };

    /// Defines various configuration parameters for the transport layer.
    ///
    class Transport
    {
    public:
        /// Defines max footprint of a callback function in use by the message RX session notification.
        /// Size is chosen arbitrary, but it should be enough to store any lambda or function pointer.
        ///
        static constexpr std::size_t IMessageRxSession_OnReceiveCallback_FunctionMaxSize()  // NOSONAR cpp:S799
        {
            return Derived::Transport::IMessageRxSession_OnReceiveCallback_FunctionMaxSize_Impl();
        }

        /// Defines max footprint of a callback function in use by the service RX session notification.
        /// Size is chosen arbitrary, but it should be enough to store any lambda or function pointer.
        ///
        static constexpr std::size_t ISvcRxSession_OnReceiveCallback_FunctionMaxSize()  // NOSONAR cpp:S799
        {
            return Derived::Transport::ISvcRxSession_OnReceiveCallback_FunctionMaxSize_Impl();
        }

        /// Defines max footprint of a platform-specific error implementation.
        ///
        static constexpr std::size_t PlatformErrorMaxSize()
        {
            return Derived::Transport::PlatformErrorMaxSize_Impl();
        }

        /// Defines max footprint of a storage implementation used by the scattered buffer.
        ///
        static constexpr std::size_t ScatteredBuffer_StorageVariantFootprint()  // NOSONAR cpp:S799
        {
            return Derived::Transport::ScatteredBuffer_StorageVariantFootprint_Impl();
        }

        /// Defines various configuration parameters for the CAN transport sublayer.
        ///
        class Can
        {
        public:
            /// Defines max footprint of a callback function in use by the CAN transport transient error handler.
            ///
            static constexpr std::size_t ICanTransport_TransientErrorHandlerMaxSize()  // NOSONAR cpp:S799
            {
                return Derived::Transport::Can::ICanTransport_TransientErrorHandlerMaxSize_Impl();
            }

        protected:
            static constexpr std::size_t ICanTransport_TransientErrorHandlerMaxSize_Impl()  // NOSONAR cpp:S799
            {
                /// Size is chosen arbitrary, but it should be enough to store simple lambda or function pointer.
                return sizeof(void*) * 3;
            }

        };  // Can

        /// Defines various configuration parameters for the UDO transport sublayer.
        ///
        class Udp
        {
        public:
            /// Defines max footprint of a callback function in use by the UDP transport transient error handler.
            ///
            static constexpr std::size_t IUdpTransport_TransientErrorHandlerMaxSize()  // NOSONAR cpp:S799
            {
                return Derived::Transport::Udp::IUdpTransport_TransientErrorHandlerMaxSize_Impl();
            }

        protected:
            static constexpr std::size_t IUdpTransport_TransientErrorHandlerMaxSize_Impl()  // NOSONAR cpp:S799
            {
                /// Size is chosen arbitrary, but it should be enough to store simple lambda or function pointer.
                return sizeof(void*) * 3;
            }

        };  // Udp

    protected:
        static constexpr std::size_t IMessageRxSession_OnReceiveCallback_FunctionMaxSize_Impl()  // NOSONAR cpp:S799
        {
            /// Size is chosen arbitrary, but it should be enough to store any lambda or function pointer.
            return sizeof(void*) * 4;
        }

        static constexpr std::size_t ISvcRxSession_OnReceiveCallback_FunctionMaxSize_Impl()  // NOSONAR cpp:S799
        {
            /// Size is chosen arbitrary, but it should be enough to store any lambda or function pointer.
            return sizeof(void*) * 4;
        }

        static constexpr std::size_t PlatformErrorMaxSize_Impl()
        {
            /// Size is chosen arbitrary, but it should be enough to store simple implementation.
            return sizeof(void*) * 3;
        }

        static constexpr std::size_t ScatteredBuffer_StorageVariantFootprint_Impl()  // NOSONAR cpp:S799
        {
            /// Size is chosen arbitrary, but it should be enough to store any implementation.
            return sizeof(void*) * 8;
        }

    };  // Transport

protected:
    static constexpr std::size_t IExecutor_Callback_FunctionMaxSize_Impl()  // NOSONAR cpp:S799
    {
        /// Size is chosen arbitrary, but it should be enough to store any lambda or function pointer.
        return sizeof(void*) * 8;
    }

    static constexpr std::size_t IExecutor_Callback_ReserveSize_Impl()  // NOSONAR cpp:S799
    {
        /// Size is chosen arbitrary, but it should be enough to store any lambda or function pointer.
        return sizeof(void*) * 16;
    }

};  // Config

// NOLINTEND(cppcoreguidelines-avoid-magic-numbers, readability-magic-numbers)

class DefaultConfig final : public Config<DefaultConfig>
{};

#ifdef LIBCYPHAL_CONFIG
using config = LIBCYPHAL_CONFIG;
#else
using config = DefaultConfig;
#endif

}  // namespace libcyphal

#endif  // LIBCYPHAL_CONFIG_HPP_INCLUDED
