/// @copyright
/// Copyright (C) OpenCyphal Development Team  <opencyphal.org>
/// Copyright Amazon.com Inc. or its affiliates.
/// SPDX-License-Identifier: MIT

#ifndef LIBCYPHAL_PRESENTATION_SUBSCRIBER_IMPL_HPP_INCLUDED
#define LIBCYPHAL_PRESENTATION_SUBSCRIBER_IMPL_HPP_INCLUDED

#include "presentation_delegate.hpp"
#include "shared_object.hpp"

#include "libcyphal/common/cavl/cavl.hpp"
#include "libcyphal/transport/msg_sessions.hpp"
#include "libcyphal/transport/scattered_buffer.hpp"
#include "libcyphal/transport/types.hpp"
#include "libcyphal/types.hpp"

#include <cetl/cetl.hpp>
#include <cetl/pf17/cetlpf.hpp>

#include <cstdint>
#include <memory>
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

// TODO: docs
class SubscriberImpl final : public cavl::Node<SubscriberImpl>, public detail::SharedObject
{
public:
    class CallbackNode : public cavl::Node<CallbackNode>
    {
    public:
        struct Deserializer
        {
            struct Context;
            using FunctionPtr = void (*)(Context&);

            struct Context
            {
                cetl::pmr::memory_resource&         memory;
                const TimePoint                     approx_now;
                const transport::ScatteredBuffer&   buffer;
                const transport::MessageRxMetadata& metadata;
                CallbackNode*&                      next_node;

            };  // Context

            using TypeId = std::uintptr_t;
            template <typename Message>
            static TypeId getTypeId() noexcept
            {
                static const struct
                {
                } placeholder{};
                // No Lint and Sonar cpp:S3630 "reinterpret_cast" should not be used" b/c it's a part of
                // the type id/erasure pattern - we use this cast to be able to compare deserializers.
                // NOLINTNEXTLINE(*-pro-type-reinterpret-cast)
                return reinterpret_cast<TypeId>(&placeholder);  // NOSONAR : cpp:S3630
            }

            // No Sonar `cpp:S5356` and `cpp:S5357` b/c of raw PMR memory allocation,
            // as well as data pointer type mismatches between scattered buffer and `const_bitspan`.
            // TODO: Eliminate PMR allocation - when `deserialize` will support scattered buffers.
            template <typename Message>
            static bool tryDeserializeMessage(Message& message, Context& context)
            {
                // Make a copy of the scattered buffer into a single contiguous temp buffer.
                //
                const std::unique_ptr<cetl::byte, PmrRawBytesDeleter>
                    tmp_buffer{static_cast<cetl::byte*>(  // NOSONAR cpp:S5356 cpp:S5357
                                   context.memory.allocate(context.buffer.size())),
                               {context.buffer.size(), &context.memory}};
                if (!tmp_buffer)
                {
                    return false;
                }
                const auto data_size = context.buffer.copy(0, tmp_buffer.get(), context.buffer.size());

                const auto* const data_raw = static_cast<const void*>(tmp_buffer.get());
                const auto* const data_u8s = static_cast<const std::uint8_t*>(data_raw);  // NOSONAR cpp:S5356 cpp:S5357
                const nunavut::support::const_bitspan bitspan{data_u8s, data_size};

                return deserialize(message, bitspan);
            }

            template <typename Message, typename Subscriber>
            static void deserializeMsgOnceForManySubs(Context& context)
            {
                CETL_DEBUG_ASSERT(context.next_node != nullptr, "");
                constexpr auto this_deserializer_fn = &deserializeMsgOnceForManySubs<Message, Subscriber>;
                CETL_DEBUG_ASSERT(context.next_node->deserializer_.function == this_deserializer_fn, "");

                // Deserialize the message from the buffer - only once!
                //
                Message    message{};
                const bool got_message = tryDeserializeMessage(message, context);

                // Enumerate all nodes with the same deserializer, and deliver the message to them.
                // It's important to do it even in case of deserialization failure -
                // to "consume" nodes and make sure that enumeration is progressing.
                //
                while (auto* const curr_node = context.next_node)
                {
                    if (curr_node->deserializer_.function != this_deserializer_fn)
                    {
                        // We've reached the end of the list of nodes with the same deserializer.
                        // A different deserializer will take care of the next node.
                        break;
                    }

                    // "Consume" current node by preparing the context for the next iteration.
                    // It's important to do it before the callback b/c its activity may modify what is next.
                    context.next_node = curr_node->getNextInOrderNode();

                    // Notify only those subscribers that were created before the message was received.
                    // This is to avoid nondeterministic delivery of messages to subscribers that were created
                    // after the message was sent to one of callbacks.
                    //
                    if (got_message && (context.approx_now > curr_node->creation_time_))
                    {
                        // This is safe downcast b/c we know that the `curr_node` is of type `Subscriber`.
                        // Otherwise, the `deserializer_` would be different from `this_deserializer`.
                        auto* const subscriber = static_cast<Subscriber*>(curr_node);
                        subscriber->onReceiveCallback(context.approx_now, message, context.metadata);

                        // NB! `curr_node` or `subscriber` must not be used anymore b/c they may be invalidated
                        // by a callback activity, f.e. by moving or freeing the `Subscriber` object.
                    }
                }
            }

            template <typename Subscriber>
            static void passRawMessageAsIs(Context& context)
            {
                CETL_DEBUG_ASSERT(context.next_node != nullptr, "");
                constexpr auto this_deserializer_fn = &passRawMessageAsIs<Subscriber>;
                CETL_DEBUG_ASSERT(context.next_node->deserializer_.function == this_deserializer_fn, "");
                (void) this_deserializer_fn;

                // "Consume" current node by preparing the context for the next iteration.
                // It's important to do it before the callback b/c its activity may modify what is next.
                auto* const curr_node = context.next_node;
                context.next_node     = curr_node->getNextInOrderNode();

                // Notify only those subscribers that were created before the message was received.
                // This is to avoid nondeterministic delivery of messages to subscribers that were created
                // after the message was sent to one of callbacks.
                //
                if (context.approx_now > curr_node->creation_time_)
                {
                    // This is safe downcast b/c we know that the `curr_node` is of type `Subscriber`.
                    // Otherwise, the `deserializer_` would be different from `this_deserializer`.
                    auto* const subscriber = static_cast<Subscriber*>(curr_node);
                    subscriber->onReceiveCallback(context.approx_now, context.buffer, context.metadata);

                    // NB! `curr_node` or `subscriber` must not be used anymore b/c they may be invalidated
                    // by a callback activity, f.e. by moving or freeing the `Subscriber` object.
                }
            }

            TypeId      type_id;
            FunctionPtr function;

        };  // Deserializer

        CallbackNode(const TimePoint creation_time, const Deserializer deserializer)
            : creation_time_{creation_time}
            , deserializer_{deserializer}

        {
            CETL_DEBUG_ASSERT(deserializer_.function != nullptr, "");
        }

        CETL_NODISCARD std::int8_t compareByDeserializer(const Deserializer& deserializer) const noexcept
        {
            // Keep callback nodes sorted by address of their `deserializer_` static function.
            // This will ensure that callback nodes with the same deserializer will be grouped together,
            // and so `deserializeMsgOnceForManySubs` strategy could be applied.
            return (deserializer.type_id >= deserializer_.type_id) ? +1 : -1;
        }

    private:
        friend class SubscriberImpl;

        // MARK: Data members:

        TimePoint    creation_time_;
        Deserializer deserializer_;

    };  // CallbackNode

    explicit SubscriberImpl(cetl::pmr::memory_resource&             memory,
                            IPresentationDelegate&                  delegate,
                            ITimeProvider&                          time_provider,
                            UniquePtr<transport::IMessageRxSession> msg_rx_session)
        : memory_{memory}
        , delegate_{delegate}
        , time_provider_{time_provider}
        , msg_rx_session_{std::move(msg_rx_session)}
        , subject_id_{msg_rx_session_->getParams().subject_id}
        , next_cb_node_{nullptr}
    {
        CETL_DEBUG_ASSERT(msg_rx_session_ != nullptr, "");

        msg_rx_session_->setOnReceiveCallback([this](const auto& arg) {
            //
            onMessageRxTransfer(arg);
        });
    }

    void onMessageRxTransfer(const transport::IMessageRxSession::OnReceiveCallback::Arg& arg)
    {
        CETL_DEBUG_ASSERT(next_cb_node_ == nullptr, "");

        if (callback_nodes_.empty())
        {
            return;
        }

        next_cb_node_ = callback_nodes_.min();
        CallbackNode::Deserializer::Context context{memory_,
                                                    time_provider_.now(),
                                                    arg.transfer.payload,
                                                    arg.transfer.metadata,
                                                    next_cb_node_};
        while (next_cb_node_ != nullptr)
        {
            CETL_DEBUG_ASSERT(next_cb_node_->deserializer_.function != nullptr, "");
            next_cb_node_->deserializer_.function(context);
        }
    }

    TimePoint now() const noexcept
    {
        return time_provider_.now();
    }

    CETL_NODISCARD std::int32_t compareBySubjectId(const transport::PortId subject_id) const
    {
        return static_cast<std::int32_t>(subject_id_) - static_cast<std::int32_t>(subject_id);
    }

    void retainCallbackNode(CallbackNode& callback_node) noexcept
    {
        CETL_DEBUG_ASSERT(!callback_node.isLinked(), "");

        SharedObject::retain();

        const auto cb_node_existing = callback_nodes_.search(   //
            [&callback_node](const CallbackNode& other_node) {  // predicate
                //
                return other_node.compareByDeserializer(callback_node.deserializer_);
            },
            [&callback_node]() { return &callback_node; });  // "factory"

        (void) cb_node_existing;
        CETL_DEBUG_ASSERT(!std::get<1>(cb_node_existing), "Unexpected existing callback node.");
        CETL_DEBUG_ASSERT(&callback_node == std::get<0>(cb_node_existing), "Unexpected callback node.");
    }

    void updateCallbackNode(const CallbackNode* const old_cb_node, CallbackNode* const new_cb_node) noexcept
    {
        CETL_DEBUG_ASSERT(old_cb_node != nullptr, "");
        CETL_DEBUG_ASSERT(!old_cb_node->isLinked(), "");
        CETL_DEBUG_ASSERT(new_cb_node != nullptr, "");
        CETL_DEBUG_ASSERT(new_cb_node->isLinked(), "");

        if (old_cb_node == next_cb_node_)
        {
            next_cb_node_ = new_cb_node;
        }
    }

    void releaseCallbackNode(CallbackNode& callback_node) noexcept
    {
        CETL_DEBUG_ASSERT(callback_node.isLinked(), "");

        if (next_cb_node_ == &callback_node)
        {
            next_cb_node_ = next_cb_node_->getNextInOrderNode();
        }
        callback_nodes_.remove(&callback_node);

        SharedObject::release();
        if (getRefCount() == 0)
        {
            delegate_.releaseSubscriber(this);
        }
    }

private:
    // MARK: Data members:

    cetl::pmr::memory_resource&                   memory_;
    IPresentationDelegate&                        delegate_;
    ITimeProvider&                                time_provider_;
    const UniquePtr<transport::IMessageRxSession> msg_rx_session_;
    const transport::PortId                       subject_id_;
    cavl::Tree<CallbackNode>                      callback_nodes_;
    CallbackNode*                                 next_cb_node_;

};  // SubscriberImpl

}  // namespace detail
}  // namespace presentation
}  // namespace libcyphal

#endif  // LIBCYPHAL_PRESENTATION_SUBSCRIBER_IMPL_HPP_INCLUDED
