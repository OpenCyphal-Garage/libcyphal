/// @copyright
/// Copyright (C) OpenCyphal Development Team  <opencyphal.org>
/// Copyright Amazon.com Inc. or its affiliates.
/// SPDX-License-Identifier: MIT

#ifndef LIBCYPHAL_PRESENTATION_PUBLISHER_HPP_INCLUDED
#define LIBCYPHAL_PRESENTATION_PUBLISHER_HPP_INCLUDED

#include "publisher_impl.hpp"

#include "libcyphal/transport/errors.hpp"
#include "libcyphal/transport/types.hpp"
#include "libcyphal/types.hpp"

#include <cetl/cetl.hpp>
#include <cetl/pf17/cetlpf.hpp>
#include <cetl/pf20/cetlpf.hpp>
#include <nunavut/support/serialization.hpp>

#include <array>
#include <cstdint>
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

/// @brief Defines internal base class for any concrete (final) message publisher.
///
/// No Sonar cpp:S4963 'The "Rule-of-Zero" should be followed'
/// b/c we do directly handle resources here.
///
class PublisherBase  // NOSONAR cpp:S4963
{
public:
    /// @brief Defines failure type for a base publisher operations.
    ///
    /// The set of possible failures of the base publisher includes transport layer failures.
    /// A strong-typed publisher extends this type with its own error types (serialization-related).
    ///
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
        // No need to retain the moved object, as it is already retained.
    }

    PublisherBase& operator=(const PublisherBase& other)
    {
        if (this != &other)
        {
            CETL_DEBUG_ASSERT(impl_ != nullptr, "Not supposed to copy to already moved `this`.");
            CETL_DEBUG_ASSERT(other.impl_ != nullptr, "Not supposed to copy from already moved `other`.");

            (void) impl_->release();

            impl_     = other.impl_;
            priority_ = other.priority_;

            impl_->retain();
        }
        return *this;
    }

    PublisherBase& operator=(PublisherBase&& other) noexcept
    {
        CETL_DEBUG_ASSERT(impl_ != nullptr, "Not supposed to move to already moved `this`.");
        CETL_DEBUG_ASSERT(other.impl_ != nullptr, "Not supposed to move from already moved `other`.");

        (void) impl_->release();

        impl_     = std::exchange(other.impl_, nullptr);
        priority_ = other.priority_;

        // No need to retain the moved object, as it is already retained.
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
    ~PublisherBase()
    {
        if (impl_ != nullptr)
        {
            (void) impl_->release();
        }
    }

    explicit PublisherBase(PublisherImpl* const impl)
        : impl_{impl}
        , priority_{transport::Priority::Nominal}
    {
        CETL_DEBUG_ASSERT(impl_ != nullptr, "");

        impl_->retain();
    }

    cetl::optional<Failure> publishRawData(const TimePoint                   deadline,
                                           const transport::PayloadFragments payload_fragments) const
    {
        CETL_DEBUG_ASSERT(impl_ != nullptr, "");

        return impl_->publishRawData(deadline, priority_, payload_fragments);
    }

private:
    // MARK: Data members:

    PublisherImpl*      impl_;
    transport::Priority priority_;

};  // PublisherBase

}  // namespace detail

/// @brief Defines a custom strong-typed message publisher class.
///
/// Although the publisher class does not specifically require a Nunavut tool generated message type,
/// it follows patterns of the tool (and has dependency on its `SerializeResult` and `bitspan` helper types),
/// so it is highly recommended to use DSDL file and the tool to generate the types.
/// Otherwise, see below requirements for the `Message` type, as well as consult with
/// Nunavut's generated code (f.e. for the signatures of expected `serialize` function).
///
/// @tparam Message The message type of the publisher. This type has the following requirements:
///                 - contains `_traits_::SerializationBufferSizeBytes` constant
///                 - has freestanding `serialize` function under its namespace (so that ADL will find it)
///
template <typename Message>
class Publisher final : public detail::PublisherBase
{
public:
    /// @brief Defines failure type for a strong-typed publisher operations.
    ///
    /// The set of possible failures includes transport layer failures (inherited from the base publisher),
    /// as well as serialization-related ones.
    ///
    using Failure = libcyphal::detail::AppendType<Failure, nunavut::support::Error>::Result;

    /// Publishes the message on libcyphal network.
    ///
    /// @param deadline The latest time to send the message. Will be dropped if exceeded.
    /// @param message The message to serialize and then send.
    ///
    cetl::optional<Failure> publish(const TimePoint deadline, const Message& message) const
    {
        // Try to serialize the message to raw payload buffer.
        //
        // Next nolint b/c we use a buffer to serialize the message, so no need to zero it (and performance better).
        // NOLINTNEXTLINE(cppcoreguidelines-pro-type-member-init,hicpp-member-init)
        std::array<std::uint8_t, Message::_traits_::SerializationBufferSizeBytes> buffer;
        const auto result_size = serialize(message, buffer);
        if (!result_size)
        {
            return result_size.error();
        }

        // Next nolint & NOSONAR are currently unavoidable.
        // TODO: Eliminate `reinterpret_cast` when Nunavut supports `cetl::byte` at its `serialize`.
        // NOLINTNEXTLINE(cppcoreguidelines-pro-type-reinterpret-cast)
        const cetl::span<const cetl::byte> data{reinterpret_cast<cetl::byte*>(buffer.data()),  // NOSONAR cpp:S3630
                                                result_size.value()};
        const std::array<const cetl::span<const cetl::byte>, 1> payload_fragments{data};
        if (auto failure = publishRawData(deadline, payload_fragments))
        {
            return libcyphal::detail::upcastVariant<Failure>(std::move(*failure));
        }

        return cetl::nullopt;
    }

private:
    friend class Presentation;  // NOLINT cppcoreguidelines-virtual-class-destructor

    explicit Publisher(detail::PublisherImpl* const impl)
        : PublisherBase{impl}
    {
    }

};  // Publisher<Message>

/// @brief Defines a raw (aka untyped) publisher class.
///
/// The publisher class has no requirements for the message data (neither any Nunavut dependencies).
/// The message data is passed as raw bytes (without any serialization step).
///
template <>
class Publisher<void> final : public detail::PublisherBase
{
public:
    /// Publishes the raw message on libcyphal network.
    ///
    /// @param deadline The latest time to send the message. Will be dropped if exceeded.
    /// @param payload_fragments The message data to publish.
    ///
    cetl::optional<Failure> publish(const TimePoint deadline, const transport::PayloadFragments payload_fragments) const
    {
        return publishRawData(deadline, payload_fragments);
    }

private:
    friend class Presentation;

    explicit Publisher(detail::PublisherImpl* const impl)
        : PublisherBase{impl}
    {
    }

};  // Publisher<void>

}  // namespace presentation
}  // namespace libcyphal

#endif  // LIBCYPHAL_PRESENTATION_PUBLISHER_HPP_INCLUDED
