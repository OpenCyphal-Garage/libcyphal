/// @copyright
/// Copyright (C) OpenCyphal Development Team  <opencyphal.org>
/// Copyright Amazon.com Inc. or its affiliates.
/// SPDX-License-Identifier: MIT

#ifndef LIBCYPHAL_PRESENTATION_SUBSCRIBER_HPP_INCLUDED
#define LIBCYPHAL_PRESENTATION_SUBSCRIBER_HPP_INCLUDED

#include "subscriber_impl.hpp"

#include "libcyphal/transport/errors.hpp"
#include "libcyphal/transport/scattered_buffer.hpp"
#include "libcyphal/transport/types.hpp"

#include <cetl/cetl.hpp>
#include <cetl/pmr/function.hpp>

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

/// @brief Defines internal base class for any concrete (final) message subscriber.
///
/// No Sonar cpp:S4963 'The "Rule-of-Zero" should be followed'
/// b/c we do directly handle resources here.
///
class SubscriberBase : public SubscriberImpl::CallbackNode  // NOSONAR cpp:S4963
{
public:
    /// @brief Defines failure type for a base subscriber operations.
    ///
    /// The set of possible failures of the base subscriber includes transport layer failures.
    ///
    using Failure = transport::AnyFailure;

    SubscriberBase(SubscriberBase&& other) noexcept
        : CallbackNode{std::move(static_cast<CallbackNode&&>(other))}
        , impl_{std::exchange(other.impl_, nullptr)}
    {
        CETL_DEBUG_ASSERT(impl_ != nullptr, "Not supposed to move from already moved `other`.");
        impl_->updateCallbackNode(&other, this);
    }

    SubscriberBase& operator=(SubscriberBase&& other) noexcept
    {
        CETL_DEBUG_ASSERT(impl_ != nullptr, "Not supposed to move to already moved `this`.");

        impl_->releaseCallbackNode(*this);

        impl_ = std::exchange(other.impl_, nullptr);

        const CallbackNode* const old_cb_node = &other;
        static_cast<CallbackNode&>(*this)     = std::move(static_cast<CallbackNode&&>(other));

        impl_->updateCallbackNode(old_cb_node, this);

        return *this;
    }

    SubscriberBase(const SubscriberBase& other)            = delete;
    SubscriberBase& operator=(const SubscriberBase& other) = delete;

protected:
    ~SubscriberBase()
    {
        if (impl_ != nullptr)
        {
            impl_->releaseCallbackNode(*this);
        }
    }

    SubscriberBase(SubscriberImpl* const impl, const Deserializer deserializer)
        : CallbackNode{impl->now(), deserializer}
        , impl_{impl}
    {
        CETL_DEBUG_ASSERT(impl != nullptr, "");

        impl_->retainCallbackNode(*this);
    }

private:
    // MARK: Data members:

    SubscriberImpl* impl_;

};  // SubscriberBase

}  // namespace detail

/// @brief Defines a custom strong-typed message subscriber class.
///
/// Although the subscriber class does not specifically require a Nunavut tool generated message type,
/// it follows patterns of the tool (and has dependency on its `SerializeResult` and `bitspan` helper types),
/// so it is highly recommended to use DSDL file and the tool to generate the types.
/// Otherwise, see below requirements for the `Message` type, as well as consult with
/// Nunavut's generated code (f.e. for the signatures of expected `deserialize` function).
///
/// @tparam Message The message type of the subscriber. This type has the following requirements:
///                 - default constructible
///                 - contains `_traits_::ExtentBytes` constant
///                 - has freestanding `deserialize` function under its namespace (so that ADL will find it).
///
template <typename Message>
class Subscriber final : public detail::SubscriberBase
{
public:
    /// @brief Defines the strong-typed message callback (arguments, function).
    ///
    struct OnReceiveCallback
    {
        struct Arg
        {
            TimePoint                    approx_now;
            Message                      message;
            transport::MessageRxMetadata metadata;
        };
        using Function = cetl::pmr::function<void(const Arg&), sizeof(void*) * 4>;
    };

    /// @brief Sets function which will be called on each message reception.
    ///
    /// Note that setting the callback will disable the previous one (if any).
    /// Also, resetting it to `nullptr` does not release internal RX session,
    /// and so incoming messages will still be coming and silently dropped.
    ///
    /// @param on_receive_cb_fn The function which will be called back.
    ///                         Use `nullptr` (or `{}`) to disable the callback.
    ///
    void setOnReceiveCallback(typename OnReceiveCallback::Function&& on_receive_cb_fn)
    {
        on_receive_cb_fn_ = std::move(on_receive_cb_fn);
    }

private:
    friend class Presentation;  // NOLINT cppcoreguidelines-virtual-class-destructor
    friend class detail::SubscriberImpl;

    explicit Subscriber(detail::SubscriberImpl* const impl)
        : SubscriberBase{impl,
                         {Deserializer::getTypeId<Message>(),
                          Deserializer::deserializeMsgOnceForManySubs<Message, Subscriber>}}
    {
    }

    void onReceiveCallback(const TimePoint                     approx_now,
                           const Message&                      message,
                           const transport::MessageRxMetadata& metadata) const
    {
        if (on_receive_cb_fn_)
        {
            on_receive_cb_fn_({approx_now, message, metadata});
        }
    }

    // MARK: Data members:

    typename OnReceiveCallback::Function on_receive_cb_fn_;

};  // Subscriber<Message>

/// @brief Defines a raw (aka untyped) subscriber class.
///
/// The publisher class has no requirements for the message data (neither any Nunavut dependencies).
/// The message data is passed as raw bytes (without any serialization step).
///
template <>
class Subscriber<void> final : public detail::SubscriberBase
{
public:
    /// @brief Defines a raw untyped message callback (arguments, function).
    ///
    struct OnReceiveCallback
    {
        struct Arg
        {
            TimePoint                         approx_now;
            const transport::ScatteredBuffer& raw_message;
            transport::MessageRxMetadata      metadata;
        };
        using Function = cetl::pmr::function<void(const Arg&), sizeof(void*) * 4>;
    };

    /// @brief Sets function which will be called on each message reception.
    ///
    /// Note that setting the callback will disable the previous one (if any).
    /// Also, resetting it to `nullptr` does not release internal RX session,
    /// and so incoming messages will still be coming and silently dropped.
    ///
    /// @param on_receive_cb_fn The function which will be called back.
    ///                         Use `nullptr` (or `{}`) to disable the callback.
    ///
    void setOnReceiveCallback(OnReceiveCallback::Function&& on_receive_cb_fn)
    {
        on_receive_cb_fn_ = std::move(on_receive_cb_fn);
    }

private:
    friend class Presentation;  // NOLINT cppcoreguidelines-virtual-class-destructor
    friend class detail::SubscriberImpl;

    explicit Subscriber(detail::SubscriberImpl* const impl)
        : SubscriberBase{impl, {Deserializer::getTypeId<void>(), Deserializer::passRawMessageAsIs<Subscriber>}}
    {
    }

    void onReceiveCallback(const TimePoint                     approx_now,
                           const transport::ScatteredBuffer&   raw_message,
                           const transport::MessageRxMetadata& metadata) const
    {
        if (on_receive_cb_fn_)
        {
            on_receive_cb_fn_({approx_now, raw_message, metadata});
        }
    }

    // MARK: Data members:

    OnReceiveCallback::Function on_receive_cb_fn_;

};  // Subscriber<void>

}  // namespace presentation
}  // namespace libcyphal

#endif  // LIBCYPHAL_PRESENTATION_SUBSCRIBER_HPP_INCLUDED
