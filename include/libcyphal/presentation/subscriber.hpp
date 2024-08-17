/// @copyright
/// Copyright (C) OpenCyphal Development Team  <opencyphal.org>
/// Copyright Amazon.com Inc. or its affiliates.
/// SPDX-License-Identifier: MIT

#ifndef LIBCYPHAL_PRESENTATION_SUBSCRIBER_HPP_INCLUDED
#define LIBCYPHAL_PRESENTATION_SUBSCRIBER_HPP_INCLUDED

#include "libcyphal/transport/errors.hpp"
#include "subscriber_impl.hpp"

#include <cetl/cetl.hpp>

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
class SubscriberBase  // NOSONAR cpp:S4963
{
public:
    using Failure = transport::AnyFailure;

    SubscriberBase(const SubscriberBase& other)
        : impl_{other.impl_}
    {
        CETL_DEBUG_ASSERT(impl_ != nullptr, "Not supposed to copy from already moved `other`.");
        impl_->retain();
    }

    SubscriberBase(SubscriberBase&& other) noexcept
        : impl_{std::exchange(other.impl_, nullptr)}
    {
        CETL_DEBUG_ASSERT(impl_ != nullptr, "Not supposed to move from already moved `other`.");
    }

    SubscriberBase& operator=(const SubscriberBase& other)
    {
        if (this != &other)
        {
            CETL_DEBUG_ASSERT(other.impl_ != nullptr, "Not supposed to copy from already moved `other`.");

            if (impl_ != nullptr)
            {
                impl_->release();
            }

            impl_ = other.impl_;
            impl_->retain();
        }
        return *this;
    }

    SubscriberBase& operator=(SubscriberBase&& other) noexcept
    {
        CETL_DEBUG_ASSERT(other.impl_ != nullptr, "Not supposed to move from already moved `other`.");

        impl_ = std::exchange(other.impl_, nullptr);

        return *this;
    }

protected:
    ~SubscriberBase() noexcept
    {
        if (impl_ != nullptr)
        {
            impl_->release();
        }
    }

    explicit SubscriberBase(SubscriberImpl* const impl)
        : impl_{impl}
    {
        CETL_DEBUG_ASSERT(impl != nullptr, "");
        impl->retain();
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
public:
private:
    friend class Presentation;  // NOLINT cppcoreguidelines-virtual-class-destructor

    explicit Subscriber(detail::SubscriberImpl* const impl)
        : SubscriberBase{impl}
    {
    }

};  // Subscriber

}  // namespace presentation
}  // namespace libcyphal

#endif  // LIBCYPHAL_PRESENTATION_SUBSCRIBER_HPP_INCLUDED
