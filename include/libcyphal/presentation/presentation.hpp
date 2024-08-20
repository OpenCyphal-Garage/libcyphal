/// @copyright
/// Copyright (C) OpenCyphal Development Team  <opencyphal.org>
/// Copyright Amazon.com Inc. or its affiliates.
/// SPDX-License-Identifier: MIT

#ifndef LIBCYPHAL_PRESENTATION_PRESENTATION_HPP_INCLUDED
#define LIBCYPHAL_PRESENTATION_PRESENTATION_HPP_INCLUDED

#include "presentation_delegate.hpp"
#include "publisher.hpp"
#include "publisher_impl.hpp"
#include "subscriber.hpp"
#include "subscriber_impl.hpp"

#include "libcyphal/common/cavl/cavl.hpp"
#include "libcyphal/transport/errors.hpp"
#include "libcyphal/transport/msg_sessions.hpp"
#include "libcyphal/transport/transport.hpp"
#include "libcyphal/transport/types.hpp"
#include "libcyphal/types.hpp"

#include <cetl/cetl.hpp>
#include <cetl/pf17/cetlpf.hpp>

#include <utility>

namespace libcyphal
{
namespace presentation
{

// TODO: docs
class Presentation final : private detail::IPresentationDelegate
{
public:
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
    Expected<Publisher<Message>, transport::AnyFailure> makePublisher(const transport::PortId subject_id)
    {
        cetl::optional<transport::AnyFailure> out_failure;

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

    /// @brief Makes a message subscriber.
    ///
    /// The subscriber must never outlive this presentation object.
    ///
    /// @tparam Message DSDL compiled (aka Nunavut generated) type of the message to subscribe.
    /// @param subject_id The subject ID to subscribe the message on.
    ///
    template <typename Message>
    Expected<Subscriber<Message>, transport::AnyFailure> makeSubscriber(const transport::PortId subject_id)
    {
        cetl::optional<transport::AnyFailure> out_failure;

        const auto subscriber_existing = subscriber_impl_nodes_.search(
            [subject_id](const detail::SubscriberImpl& other_sub) {  // predicate
                //
                return other_sub.compareBySubjectId(subject_id);
            },
            [this, subject_id, &out_failure]() -> detail::SubscriberImpl* {  // factory
                //
                return makeSubscriberImpl({Message::_traits_::ExtentBytes, subject_id}, out_failure);
            });
        if (out_failure)
        {
            return std::move(*out_failure);
        }

        auto* const subscriber_impl = std::get<0>(subscriber_existing);
        CETL_DEBUG_ASSERT(subscriber_impl != nullptr, "");

        return Subscriber<Message>{subscriber_impl};
    }

private:
    template <typename T>
    using PmrAllocator        = libcyphal::detail::PmrAllocator<T>;
    using MessageRxSessionPtr = UniquePtr<transport::IMessageRxSession>;
    using MessageTxSessionPtr = UniquePtr<transport::IMessageTxSession>;

    CETL_NODISCARD detail::PublisherImpl* makePublisherImpl(const transport::MessageTxParams       params,
                                                            cetl::optional<transport::AnyFailure>& out_failure)
    {
        auto maybe_session = transport_.makeMessageTxSession(params);
        if (auto* const failure = cetl::get_if<transport::AnyFailure>(&maybe_session))
        {
            out_failure = std::move(*failure);
            return nullptr;
        }

        detail::PublisherImpl* publisher_impl{nullptr};
        if (auto msg_tx_session = cetl::get<MessageTxSessionPtr>(std::move(maybe_session)))
        {
            PmrAllocator<detail::PublisherImpl> allocator{&memory_};
            publisher_impl = allocator.allocate(1);
            if (publisher_impl != nullptr)
            {
                allocator.construct(publisher_impl,
                                    static_cast<detail::IPresentationDelegate&>(*this),
                                    std::move(msg_tx_session));
            }
        }
        if (publisher_impl == nullptr)
        {
            out_failure = transport::MemoryError{};
        }
        return publisher_impl;
    }

    CETL_NODISCARD detail::SubscriberImpl* makeSubscriberImpl(const transport::MessageRxParams       params,
                                                              cetl::optional<transport::AnyFailure>& out_failure)
    {
        auto maybe_session = transport_.makeMessageRxSession(params);
        if (auto* const failure = cetl::get_if<transport::AnyFailure>(&maybe_session))
        {
            out_failure = std::move(*failure);
            return nullptr;
        }

        detail::SubscriberImpl* subscriber_impl{nullptr};
        if (auto msg_rx_session = cetl::get<MessageRxSessionPtr>(std::move(maybe_session)))
        {
            PmrAllocator<detail::SubscriberImpl> allocator{&memory_};
            subscriber_impl = allocator.allocate(1);
            if (subscriber_impl != nullptr)
            {
                allocator.construct(subscriber_impl,
                                    memory_,
                                    static_cast<detail::IPresentationDelegate&>(*this),
                                    executor_,
                                    std::move(msg_rx_session));
            }
        }
        if (subscriber_impl == nullptr)
        {
            out_failure = transport::MemoryError{};
        }
        return subscriber_impl;
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

    void releasePublisher(detail::PublisherImpl* const publisher_impl) noexcept override
    {
        releaseAnyImplNode(publisher_impl, publisher_impl_nodes_);
    }

    void releaseSubscriber(detail::SubscriberImpl* const subscriber_impl) noexcept override
    {
        releaseAnyImplNode(subscriber_impl, subscriber_impl_nodes_);
    }

    // MARK: Data members:

    cetl::pmr::memory_resource&        memory_;
    IExecutor&                         executor_;
    transport::ITransport&             transport_;
    cavl::Tree<detail::PublisherImpl>  publisher_impl_nodes_;
    cavl::Tree<detail::SubscriberImpl> subscriber_impl_nodes_;

};  // Presentation

}  // namespace presentation
}  // namespace libcyphal

#endif  // LIBCYPHAL_PRESENTATION_PRESENTATION_HPP_INCLUDED
