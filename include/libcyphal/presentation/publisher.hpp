/// @copyright
/// Copyright (C) OpenCyphal Development Team  <opencyphal.org>
/// Copyright Amazon.com Inc. or its affiliates.
/// SPDX-License-Identifier: MIT

#ifndef LIBCYPHAL_PRESENTATION_PUBLISHER_HPP_INCLUDED
#define LIBCYPHAL_PRESENTATION_PUBLISHER_HPP_INCLUDED

#include "libcyphal/transport/msg_sessions.hpp"
#include "publisher_impl.hpp"

#include <cetl/pf17/cetlpf.hpp>
#include <nunavut/support/serialization.hpp>

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
class PublisherBase
{
public:
    using Failure = transport::AnyFailure;

    PublisherBase(const PublisherBase& other)
        : impl_{other.impl_}
        , priority_{other.priority_}
    {
        CETL_DEBUG_ASSERT(impl_ != nullptr, "Not supposed to copy from already moved `other`.");
        impl_->retain();
    }

    PublisherBase(PublisherBase&& other) noexcept
        : impl_{std::exchange(other.impl_, nullptr)}
        , priority_{other.priority_}
    {
        CETL_DEBUG_ASSERT(impl_ != nullptr, "Not supposed to move from already moved `other`.");
    }

    PublisherBase& operator=(const PublisherBase& other)
    {
        if (this != &other)
        {
            CETL_DEBUG_ASSERT(other.impl_ != nullptr, "Not supposed to copy from already moved `other`.");

            if (impl_ != nullptr)
            {
                impl_->release();
            }

            impl_     = other.impl_;
            priority_ = other.priority_;

            impl_->retain();
        }
        return *this;
    }

    PublisherBase& operator=(PublisherBase&& other) noexcept
    {
        CETL_DEBUG_ASSERT(other.impl_ != nullptr, "Not supposed to move from already moved `other`.");

        impl_     = std::exchange(other.impl_, nullptr);
        priority_ = other.priority_;

        return *this;
    }

    transport::Priority getPriority() const noexcept
    {
        return priority_;
    }
    void setPriority(const transport::Priority priority) noexcept
    {
        priority_ = priority;
    }

protected:
    ~PublisherBase() noexcept
    {
        if (impl_ != nullptr)
        {
            impl_->release();
        }
    }

    explicit PublisherBase(PublisherImpl* impl)
        : impl_{impl}
        , priority_{transport::Priority::Nominal}
    {
        CETL_DEBUG_ASSERT(impl != nullptr, "");
        impl->retain();
    }

    cetl::optional<Failure> publish(const TimePoint now, const cetl::span<const cetl::byte> data) const
    {
        CETL_DEBUG_ASSERT(impl_ != nullptr, "");
        return impl_->publish(now, priority_, data);
    }

private:
    // MARK: Data members:

    PublisherImpl*      impl_;
    transport::Priority priority_;

};  // PublisherBase

}  // namespace detail

// TODO: docs
template <typename Message>
class Publisher final : public detail::PublisherBase
{
public:
    using Failure = libcyphal::detail::AppendType<transport::AnyFailure, nunavut::support::Error>::Result;

    cetl::optional<Failure> publish(const TimePoint now, const Message& message) const
    {
        std::array<std::uint8_t, Message::_traits_::SerializationBufferSizeBytes> buffer;

        const auto result = serialize(message, buffer);
        if (!result)
        {
            return result.error();
        }

        // NOLINTNEXTLINE
        const cetl::span<const cetl::byte> data{reinterpret_cast<cetl::byte*>(buffer.data()), result.value()};
        if (auto failure = PublisherBase::publish(now, data))
        {
            return failureFromVariant(std::move(*failure));
        }

        return cetl::nullopt;
    }

private:
    friend class Presentation;

    explicit Publisher(detail::PublisherImpl* impl)
        : PublisherBase{impl}
    {
    }

    template <typename FailureVar>
    CETL_NODISCARD static Failure failureFromVariant(FailureVar&& failure_var)
    {
        return cetl::visit(
            [](auto&& failure) -> Failure {
                //
                return std::forward<decltype(failure)>(failure);
            },
            std::forward<FailureVar>(failure_var));
    }
};
//
template <>
class Publisher<void> final : public detail::PublisherBase
{
public:
    using PublisherBase::publish;

private:
    friend class Presentation;

    explicit Publisher(detail::PublisherImpl* impl)
        : PublisherBase{impl}
    {
    }

};  // Publisher

}  // namespace presentation
}  // namespace libcyphal

#endif  // LIBCYPHAL_PRESENTATION_PUBLISHER_HPP_INCLUDED
