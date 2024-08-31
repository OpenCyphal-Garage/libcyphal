/// @copyright
/// Copyright (C) OpenCyphal Development Team  <opencyphal.org>
/// Copyright Amazon.com Inc. or its affiliates.
/// SPDX-License-Identifier: MIT

#ifndef LIBCYPHAL_PRESENTATION_PRESENTATION_HPP_INCLUDED
#define LIBCYPHAL_PRESENTATION_PRESENTATION_HPP_INCLUDED

#include "client.hpp"
#include "client_impl.hpp"
#include "presentation_delegate.hpp"
#include "publisher.hpp"
#include "publisher_impl.hpp"
#include "server.hpp"
#include "server_impl.hpp"
#include "subscriber.hpp"
#include "subscriber_impl.hpp"

#include "libcyphal/common/cavl/cavl.hpp"
#include "libcyphal/transport/errors.hpp"
#include "libcyphal/transport/msg_sessions.hpp"
#include "libcyphal/transport/svc_sessions.hpp"
#include "libcyphal/transport/transport.hpp"
#include "libcyphal/transport/types.hpp"
#include "libcyphal/types.hpp"

#include <cetl/cetl.hpp>
#include <cetl/pf17/cetlpf.hpp>

#include <cstddef>
#include <type_traits>
#include <utility>

namespace libcyphal
{
namespace presentation
{

/// Internal implementation details of the Presentation layer.
/// Not supposed to be used directly by the users of the library.
///
namespace detail
{

/// Trait which determines whether the given type has `T::_traits_::HasFixedPortID` field.
///
template <typename T>
auto HasFixedPortIdTrait(bool dummy) -> decltype(std::decay_t<T>::_traits_::HasFixedPortID, std::true_type{});
template <typename>
std::false_type HasFixedPortIdTrait(...);

/// Trait which determines whether the given type is a service one with a fixed port id assigned.
///
/// The service type is expected to have `T::_traits_::HasFixedPortID` boolean constant equal `true`.
/// In use to enable `makeServer` method for service types with a standard fixed port id.
///
template <typename T,
          bool = IsServiceTrait<T>::value && decltype(HasFixedPortIdTrait<typename T::Request>(true))::value>
struct IsFixedPortIdServiceTrait
{
    static constexpr bool value = false;
};
template <typename T>
struct IsFixedPortIdServiceTrait<T, true>
{
    static constexpr bool value = T::Request::_traits_::HasFixedPortID;
};

/// Trait which determines whether the given type is a message one with a fixed port id assigned.
///
/// The message type is expected to have `T::_traits_::HasFixedPortID` boolean constant equal `true`.
/// In use to enable `makePublisher` and `makeSubscriber` methods for message types with a standard fixed port id.
///
template <typename T, bool = decltype(HasFixedPortIdTrait<T>(true))::value>
struct IsFixedPortIdMessageTrait
{
    static constexpr bool value = false;
};
template <typename T>
struct IsFixedPortIdMessageTrait<T, true>
{
    static constexpr bool value = T::_traits_::HasFixedPortID;
};

}  // namespace detail

/// @brief Defines the main presentation layer class.
///
/// Instance of this class is supposed to be created once per transport instance (or even per application).
/// Main purpose of the presentation object is to create publishers, subscribers, and RPC clients and servers.
///
class Presentation final : private detail::IPresentationDelegate
{
public:
    /// @brief Defines failure type of various `make...` methods of the presentation layer.
    ///
    /// The set of possible make errors includes transport layer failures.
    ///
    using MakeFailure = transport::AnyFailure;

    /// @brief Constructs the presentation layer object.
    ///
    Presentation(cetl::pmr::memory_resource& memory, IExecutor& executor, transport::ITransport& transport) noexcept
        : memory_{memory}
        , executor_{executor}
        , transport_{transport}
    {
    }

    /// @brief Makes a message publisher.
    ///
    /// The publisher must never outlive this presentation object.
    ///
    /// @tparam Message DSDL compiled (aka Nunavut generated) type of the message to publish.
    /// @param subject_id The subject ID to publish the message on.
    ///
    template <typename Message>
    auto makePublisher(const transport::PortId subject_id) -> Expected<Publisher<Message>, MakeFailure>
    {
        cetl::optional<MakeFailure> out_failure;

        // Create a shared publisher implementation node or find an existing one.
        //
        const auto publisher_existing = publisher_impl_nodes_.search(
            [subject_id](const detail::PublisherImpl& other_pub) {  // predicate
                //
                return other_pub.compareBySubjectId(subject_id);
            },
            [this, subject_id, &out_failure]() -> detail::PublisherImpl* {  // factory
                //
                return makePublisherImpl({subject_id}, out_failure);
            });
        if (out_failure)
        {
            return std::move(*out_failure);
        }

        auto* const publisher_impl = std::get<0>(publisher_existing);
        CETL_DEBUG_ASSERT(publisher_impl != nullptr, "");

        return Publisher<Message>{publisher_impl};
    }

    /// @brief Makes a typed publisher bound to its standard (aka fixed) subject id.
    ///
    /// @tparam Message The message type generated by DSDL tool. The type expected to have a fixed port ID.
    ///
    template <typename Message, typename Result = Expected<Publisher<Message>, MakeFailure>>
    auto makePublisher() -> std::enable_if_t<detail::IsFixedPortIdMessageTrait<Message>::value, Result>
    {
        return makePublisher<Message>(Message::_traits_::FixedPortId);
    }

    /// @brief Makes a typed message subscriber.
    ///
    /// The subscriber must never outlive this presentation object.
    ///
    /// Internally, multiple subscribers to the same subject id are using the same instance of shared RX session.
    /// Such sharing is transparent to the user of the library, but has implications on the extent bytes parameter -
    /// the very first subscriber to a subject id defines the extent bytes for all subscribers to that subject id.
    /// So implicit extent bytes might be ignored if the subscriber is the second or later one to the subject id. This
    /// behavior may be improved in a future version such that the largest extent of all existing subscribers is used.
    ///
    /// @tparam Message DSDL compiled (aka Nunavut generated) type of the message to subscribe. Size of the transfer
    ///                 payload memory buffer (the `extent_bytes`) is automatically determined from the message type
    ///                 by applying `Message::_traits_::ExtentBytes` (generated by the Nunavut tool).
    /// @param subject_id The subject ID to subscribe the message on.
    /// @param on_receive_cb_fn Optional callback function to be called when a message is received.
    ///                         Can be assigned (or reset) later via `Subscriber::setOnReceiveCallback`.
    ///
    template <typename Message, typename Result = Expected<Subscriber<Message>, MakeFailure>>
    auto makeSubscriber(const transport::PortId                                     subject_id,
                        typename Subscriber<Message>::OnReceiveCallback::Function&& on_receive_cb_fn = {})
        -> std::enable_if_t<!std::is_void<Message>::value, Result>
    {
        cetl::optional<MakeFailure> out_failure;
        const std::size_t           extent_bytes    = Message::_traits_::ExtentBytes;
        auto* const                 subscriber_impl = createSharedSubscriberImpl(subject_id, extent_bytes, out_failure);
        if (out_failure)
        {
            return std::move(*out_failure);
        }

        Subscriber<Message> subscriber{subscriber_impl};

        if (on_receive_cb_fn)
        {
            subscriber.setOnReceiveCallback(std::move(on_receive_cb_fn));
        }

        return subscriber;
    }

    /// @brief Makes a typed subscriber bound to its standard (aka fixed) subject id.
    ///
    /// @tparam Message The message type generated by DSDL tool. The type expected to have a fixed port ID.
    /// @param on_receive_cb_fn Optional callback function to be called when a message is received.
    ///                         Can be assigned (or reset) later via `Subscriber::setOnReceiveCallback`.
    ///
    template <typename Message, typename Result = Expected<Subscriber<Message>, MakeFailure>>
    auto makeSubscriber(typename Subscriber<Message>::OnReceiveCallback::Function&& on_receive_cb_fn = {})
        -> std::enable_if_t<detail::IsFixedPortIdMessageTrait<Message>::value, Result>
    {
        return makeSubscriber<Message>(Message::_traits_::FixedPortId, std::move(on_receive_cb_fn));
    }

    /// @brief Makes a raw message subscriber.
    ///
    /// The subscriber must never outlive this presentation object.
    ///
    /// Internally, multiple subscribers to the same subject id are using the same instance of shared RX session.
    /// Such sharing is transparent to the user of the library, but has implications on the extent bytes parameter -
    /// the very first subscriber to a subject id defines the extent bytes for all subscribers to that subject id.
    /// So explicit `extent_bytes` might be ignored if the subscriber is the second or later one to the subject id. This
    /// behavior may be improved in a future version such that the largest extent of all existing subscribers is used.
    ///
    /// @param subject_id The subject ID to subscribe the message on.
    /// @param extent_bytes Defines the size of the transfer payload memory buffer;
    ///                     or, in other words, the maximum possible size of received objects,
    ///                     considering also possible future versions with new fields.
    /// @param on_receive_cb_fn Optional callback function to be called when a message is received.
    ///                         Can be assigned (or reset) later via `Subscriber::setOnReceiveCallback`.
    ///
    auto makeSubscriber(const transport::PortId                         subject_id,
                        const std::size_t                               extent_bytes,
                        Subscriber<void>::OnReceiveCallback::Function&& on_receive_cb_fn = {})
        -> Expected<Subscriber<void>, MakeFailure>
    {
        cetl::optional<MakeFailure> out_failure;
        auto* const                 subscriber_impl = createSharedSubscriberImpl(subject_id, extent_bytes, out_failure);
        if (out_failure)
        {
            return std::move(*out_failure);
        }

        Subscriber<void> subscriber{subscriber_impl};

        if (on_receive_cb_fn)
        {
            subscriber.setOnReceiveCallback(std::move(on_receive_cb_fn));
        }

        return subscriber;
    }

    /// @brief Makes a custom typed RPC server bound to a specific service id.
    ///
    /// @tparam Request The request type of the server. See `Server<Request, ...>` for more details.
    /// @tparam Response The response type of the server. See `Server<..., Response>` for more details.
    /// @param service_id The service ID of the server.
    /// @param on_request_cb_fn Optional callback function to be called when a request is received.
    ///                         Can be assigned (or reset) later via `Server::setOnRequestCallback`.
    ///
    template <typename Request, typename Response>
    auto makeServer(const transport::PortId                                           service_id,
                    typename Server<Request, Response>::OnRequestCallback::Function&& on_request_cb_fn = {})
        -> Expected<Server<Request, Response>, MakeFailure>
    {
        auto maybe_server_impl = makeServerImpl({Request::_traits_::ExtentBytes, service_id});
        if (auto* const failure = cetl::get_if<MakeFailure>(&maybe_server_impl))
        {
            return std::move(*failure);
        }

        Server<Request, Response> server{cetl::get<detail::ServerImpl>(std::move(maybe_server_impl))};

        if (on_request_cb_fn)
        {
            server.setOnRequestCallback(std::move(on_request_cb_fn));
        }

        return server;
    }

    /// @brief Makes a service typed RPC server bound to a specific service id.
    ///
    /// @tparam Service The service type generated by DSDL tool. See `ServiceServer<Service>` for more details.
    /// @param service_id The service ID of the server.
    /// @param on_request_cb_fn Optional callback function to be called when a request is received.
    ///                         Can be assigned (or reset) later via `Server::setOnRequestCallback`.
    ///
    template <typename Service, typename Result = Expected<ServiceServer<Service>, MakeFailure>>
    auto makeServer(const transport::PortId                                        service_id,
                    typename ServiceServer<Service>::OnRequestCallback::Function&& on_request_cb_fn = {})
        -> std::enable_if_t<detail::IsServiceTrait<Service>::value, Result>
    {
        return makeServer<typename Service::Request, typename Service::Response>(service_id,
                                                                                 std::move(on_request_cb_fn));
    }

    /// @brief Makes a typed RPC server bound to its standard (aka fixed) service id.
    ///
    /// @tparam Service The service type generated by DSDL tool. The type expected to have a fixed port ID.
    /// @param on_request_cb_fn Optional callback function to be called when a request is received.
    ///                         Can be assigned (or reset) later via `Server::setOnRequestCallback`.
    ///
    template <typename Service, typename Result = Expected<ServiceServer<Service>, MakeFailure>>
    auto makeServer(typename ServiceServer<Service>::OnRequestCallback::Function&& on_request_cb_fn = {})
        -> std::enable_if_t<detail::IsFixedPortIdServiceTrait<Service>::value, Result>
    {
        return makeServer<Service>(Service::Request::_traits_::FixedPortId, std::move(on_request_cb_fn));
    }

    /// @brief Makes a raw (aka untyped) RPC server bound to a specific service id.
    ///
    /// @param service_id The service ID to bind the server on.
    /// @param extent_bytes Defines the size of the transfer payload memory buffer;
    ///                     or, in other words, the maximum possible size of received objects,
    ///                     considering also possible future versions with new fields.
    /// @param on_request_cb_fn Optional callback function to be called when a request is received.
    ///                         Can be assigned (or reset) later via `Server::setOnRequestCallback`.
    ///
    auto makeServer(const transport::PortId                         service_id,
                    const std::size_t                               extent_bytes,
                    RawServiceServer::OnRequestCallback::Function&& on_request_cb_fn = {})
        -> Expected<RawServiceServer, MakeFailure>
    {
        auto maybe_server_impl = makeServerImpl({extent_bytes, service_id});
        if (auto* const failure = cetl::get_if<MakeFailure>(&maybe_server_impl))
        {
            return std::move(*failure);
        }

        RawServiceServer server{cetl::get<detail::ServerImpl>(std::move(maybe_server_impl))};

        if (on_request_cb_fn)
        {
            server.setOnRequestCallback(std::move(on_request_cb_fn));
        }

        return server;
    }

    /// @brief Makes a raw (aka untyped) RPC client bound to a specific server node and service ids.
    ///
    /// @param server_node_id The server node ID to bind the client on.
    /// @param service_id The service ID to bind the client on.
    /// @param extent_bytes Defines the size of the transfer payload memory buffer;
    ///                     or, in other words, the maximum possible size of received objects,
    ///                     considering also possible future versions with new fields.
    ///
    auto makeClient(const transport::NodeId server_node_id,
                    const transport::PortId service_id,
                    const std::size_t       extent_bytes) -> Expected<RawServiceClient, MakeFailure>
    {
        cetl::optional<MakeFailure> out_failure;
        auto* const client_impl = createSharedClientImpl({extent_bytes, service_id, server_node_id}, out_failure);
        if (out_failure)
        {
            return std::move(*out_failure);
        }

        return RawServiceClient{client_impl};
    }

private:
    template <typename T>
    using PmrAllocator = libcyphal::detail::PmrAllocator<T>;

    IPresentationDelegate& asDelegate() noexcept
    {
        return static_cast<IPresentationDelegate&>(*this);
    }

    template <typename Session>
    CETL_NODISCARD static UniquePtr<Session> getIfSession(
        Expected<UniquePtr<Session>, transport::AnyFailure> maybe_session,
        cetl::optional<MakeFailure>&                        out_failure)
    {
        if (auto* const failure = cetl::get_if<transport::AnyFailure>(&maybe_session))
        {
            out_failure = std::move(*failure);
            return nullptr;
        }

        auto session = cetl::get<UniquePtr<Session>>(std::move(maybe_session));
        if (session == nullptr)
        {
            out_failure = transport::MemoryError{};
        }

        return session;
    }

    CETL_NODISCARD detail::PublisherImpl* makePublisherImpl(const transport::MessageTxParams params,
                                                            cetl::optional<MakeFailure>&     out_failure)
    {
        if (auto tx_session = getIfSession(transport_.makeMessageTxSession(params), out_failure))
        {
            PmrAllocator<detail::PublisherImpl> allocator{&memory_};
            detail::PublisherImpl*              publisher_impl = allocator.allocate(1);
            if (publisher_impl == nullptr)
            {
                out_failure = transport::MemoryError{};
                return nullptr;
            }

            allocator.construct(publisher_impl, asDelegate(), std::move(tx_session));
            return publisher_impl;
        }
        CETL_DEBUG_ASSERT(out_failure, "");
        return nullptr;
    }

    detail::SubscriberImpl* createSharedSubscriberImpl(const transport::PortId      subject_id,
                                                       const std::size_t            extent_bytes,
                                                       cetl::optional<MakeFailure>& out_failure)
    {
        // Create a shared subscriber implementation node; or find the existing one.
        //
        const auto subscriber_existing = subscriber_impl_nodes_.search(
            [subject_id](const detail::SubscriberImpl& other_sub) {  // predicate
                //
                return other_sub.compareBySubjectId(subject_id);
            },
            [this, subject_id, extent_bytes, &out_failure]() -> detail::SubscriberImpl* {  // factory
                //
                return makeSubscriberImpl({extent_bytes, subject_id}, out_failure);
            });
        if (out_failure)
        {
            return nullptr;
        }

        auto* const subscriber_impl = std::get<0>(subscriber_existing);
        CETL_DEBUG_ASSERT(subscriber_impl != nullptr, "");
        return subscriber_impl;
    }

    CETL_NODISCARD detail::SubscriberImpl* makeSubscriberImpl(const transport::MessageRxParams params,
                                                              cetl::optional<MakeFailure>&     out_failure)
    {
        if (auto rx_session = getIfSession(transport_.makeMessageRxSession(params), out_failure))
        {
            PmrAllocator<detail::SubscriberImpl> allocator{&memory_};
            detail::SubscriberImpl*              subscriber_impl = allocator.allocate(1);
            if (subscriber_impl == nullptr)
            {
                out_failure = transport::MemoryError{};
                return nullptr;
            }

            allocator.construct(subscriber_impl, memory_, asDelegate(), executor_, std::move(rx_session));
            return subscriber_impl;
        }
        CETL_DEBUG_ASSERT(out_failure, "");
        return nullptr;
    }

    CETL_NODISCARD Expected<detail::ServerImpl, MakeFailure> makeServerImpl(const transport::RequestRxParams params)
    {
        cetl::optional<MakeFailure> out_failure;
        if (auto rx_session = getIfSession(transport_.makeRequestRxSession(params), out_failure))
        {
            const transport::ResponseTxParams tx_params{params.service_id};
            if (auto tx_session = getIfSession(transport_.makeResponseTxSession(tx_params), out_failure))
            {
                return detail::ServerImpl{memory_, executor_, std::move(rx_session), std::move(tx_session)};
            }
        }
        CETL_DEBUG_ASSERT(out_failure, "");
        return *out_failure;
    }

    detail::ClientImpl* createSharedClientImpl(const transport::ResponseRxParams& rx_params,
                                               cetl::optional<MakeFailure>&       out_failure)
    {
        // Create a shared client implementation node; or find the existing one.
        //
        const auto client_existing = client_impl_nodes_.search(
            [&rx_params](const detail::ClientImpl& other_client) {  // predicate
                //
                return other_client.compareByNodeAndServiceIds(rx_params);
            },
            [this, &rx_params, &out_failure]() -> detail::ClientImpl* {  // factory
                //
                return makeClientImpl(rx_params, out_failure);
            });
        if (out_failure)
        {
            return nullptr;
        }

        auto* const client_impl = std::get<0>(client_existing);
        CETL_DEBUG_ASSERT(client_impl != nullptr, "");
        return client_impl;
    }

    CETL_NODISCARD detail::ClientImpl* makeClientImpl(const transport::ResponseRxParams rx_params,
                                                      cetl::optional<MakeFailure>&      out_failure)
    {
        const transport::RequestTxParams tx_params{rx_params.service_id, rx_params.server_node_id};
        if (auto tx_session = getIfSession(transport_.makeRequestTxSession(tx_params), out_failure))
        {
            if (auto rx_session = getIfSession(transport_.makeResponseRxSession(rx_params), out_failure))
            {
                PmrAllocator<detail::ClientImpl> allocator{&memory_};
                detail::ClientImpl*              client_impl = allocator.allocate(1);
                if (client_impl == nullptr)
                {
                    out_failure = transport::MemoryError{};
                    return nullptr;
                }

                allocator.construct(client_impl,
                                    memory_,
                                    asDelegate(),
                                    executor_,
                                    std::move(tx_session),
                                    std::move(rx_session));
                return client_impl;
            }
        }
        CETL_DEBUG_ASSERT(out_failure, "");
        return nullptr;
    }

    template <typename ImplNode>
    void releaseAnyImplNode(ImplNode* const impl, cavl::Tree<ImplNode>& tree) noexcept
    {
        CETL_DEBUG_ASSERT(impl, "");

        // TODO: make it async (deferred to "on idle" callback).
        tree.remove(impl);
        // No Sonar
        // - cpp:S3432   "Destructors should not be called explicitly"
        // - cpp:M23_329 "Advanced memory management" shall not be used"
        // b/c we do our own low-level PMR management here.
        impl->~ImplNode();  // NOSONAR cpp:S3432 cpp:M23_329
        PmrAllocator<ImplNode> allocator{&memory_};
        allocator.deallocate(impl, 1);
    }

    // MARK: IPresentationDelegate

    void releaseClientImpl(detail::ClientImpl* const client_impl) noexcept override
    {
        releaseAnyImplNode(client_impl, client_impl_nodes_);
    }

    void releasePublisherImpl(detail::PublisherImpl* const publisher_impl) noexcept override
    {
        releaseAnyImplNode(publisher_impl, publisher_impl_nodes_);
    }

    void releaseSubscriberImpl(detail::SubscriberImpl* const subscriber_impl) noexcept override
    {
        releaseAnyImplNode(subscriber_impl, subscriber_impl_nodes_);
    }

    // MARK: Data members:

    cetl::pmr::memory_resource&        memory_;
    IExecutor&                         executor_;
    transport::ITransport&             transport_;
    cavl::Tree<detail::ClientImpl>     client_impl_nodes_;
    cavl::Tree<detail::PublisherImpl>  publisher_impl_nodes_;
    cavl::Tree<detail::SubscriberImpl> subscriber_impl_nodes_;

};  // Presentation

}  // namespace presentation
}  // namespace libcyphal

#endif  // LIBCYPHAL_PRESENTATION_PRESENTATION_HPP_INCLUDED
