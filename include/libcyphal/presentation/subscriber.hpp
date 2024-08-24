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
#include <nunavut/support/serialization.hpp>

#include <tuple>
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
/// No Sonar cpp:S4963 "The "Rule-of-Zero" should be followed"
/// b/c we do directly handle resources here.
class SubscriberBase : public SubscriberImpl::CallbackNode  // NOSONAR cpp:S4963
{
public:
    using Failure = transport::AnyFailure;

    SubscriberBase(SubscriberBase&& other) noexcept
        : CallbackNode{std::move(static_cast<CallbackNode&>(other))}
        , impl_{std::exchange(other.impl_, nullptr)}
    {
        CETL_DEBUG_ASSERT(impl_ != nullptr, "Not supposed to move from already moved `other`.");
        impl_->updateCallbackNode(&other, this);
    }

    SubscriberBase& operator=(SubscriberBase&& other) noexcept
    {
        CETL_DEBUG_ASSERT(impl_ != nullptr, "Not supposed to move to already moved `this`.");

        impl_->releaseCallbackNode(*this);

        static_cast<CallbackNode&>(*this) = std::move(static_cast<CallbackNode&>(other));

        impl_ = std::exchange(other.impl_, nullptr);
        impl_->updateCallbackNode(&other, this);

        return *this;
    }

    SubscriberBase(const SubscriberBase& other)            = delete;
    SubscriberBase& operator=(const SubscriberBase& other) = delete;

protected:
    ~SubscriberBase() noexcept
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

// TODO: docs
template <typename Message>
class Subscriber final : public detail::SubscriberBase
{
    static_assert(!Message::_traits_::IsServiceType, "Service types are not supported by the Subscriber.");

public:
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
                           const transport::MessageRxMetadata& metadata)
    {
        if (on_receive_cb_fn_)
        {
            on_receive_cb_fn_({approx_now, message, metadata});
        }
    }

    // MARK: Data members:

    typename OnReceiveCallback::Function on_receive_cb_fn_;

};  // Subscriber

}  // namespace presentation
}  // namespace libcyphal

#endif  // LIBCYPHAL_PRESENTATION_SUBSCRIBER_HPP_INCLUDED
