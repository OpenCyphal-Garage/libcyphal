/// @copyright
/// Copyright (C) OpenCyphal Development Team  <opencyphal.org>
/// Copyright Amazon.com Inc. or its affiliates.
/// SPDX-License-Identifier: MIT

#ifndef LIBCYPHAL_PRESENTATION_CLIENT_HPP_INCLUDED
#define LIBCYPHAL_PRESENTATION_CLIENT_HPP_INCLUDED

#include "client_impl.hpp"

#include "libcyphal/transport/errors.hpp"

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

/// @brief Defines internal base class for any concrete (final) service client.
///
/// No Sonar cpp:S4963 "The "Rule-of-Zero" should be followed"
/// b/c we do directly handle resources here.
///
class ClientBase  // NOSONAR cpp:S4963
{
public:
    /// @brief Defines failure type for a base client operations.
    ///
    /// The set of possible failures of the base client includes transport layer failures.
    ///
    using Failure = transport::AnyFailure;

    ClientBase(const ClientBase& other)
        : impl_{other.impl_}
    {
        CETL_DEBUG_ASSERT(other.impl_ != nullptr, "Not supposed to copy construct from already moved `other`.");

        impl_->retain();
    }

    ClientBase(ClientBase&& other) noexcept
        : impl_{std::exchange(other.impl_, nullptr)}
    {
        CETL_DEBUG_ASSERT(impl_ != nullptr, "Not supposed to move construct from already moved `other`.");
        // No need to retain the moved object, as it is already retained.
    }

    ClientBase& operator=(const ClientBase& other)
    {
        CETL_DEBUG_ASSERT(impl_ != nullptr, "Not supposed to copy assign to already moved `this`.");
        CETL_DEBUG_ASSERT(other.impl_ != nullptr, "Not supposed to copy assign from already moved `other`.");

        if (this != &other)
        {
            impl_->release();
            impl_ = other.impl_;
            impl_->retain();
        }
        return *this;
    }

    ClientBase& operator=(ClientBase&& other) noexcept
    {
        CETL_DEBUG_ASSERT(impl_ != nullptr, "Not supposed to move construct to already moved `this`.");
        CETL_DEBUG_ASSERT(other.impl_ != nullptr, "Not supposed to move construct from already moved `other`.");

        impl_->release();
        impl_ = std::exchange(other.impl_, nullptr);
        // No need to retain the moved object, as it is already retained.

        return *this;
    }

protected:
    ~ClientBase() noexcept
    {
        if (impl_ != nullptr)
        {
            impl_->release();
        }
    }

    explicit ClientBase(ClientImpl* const impl)
        : impl_{impl}
    {
        CETL_DEBUG_ASSERT(impl_ != nullptr, "");

        impl_->retain();
    }

private:
    // MARK: Data members:

    ClientImpl* impl_;

};  // ClientBase

}  // namespace detail

/// @brief Defines a raw (aka untyped) RPC client class.
///
/// The client class has no requirements for the request and response data (neither any Nunavut dependencies).
/// The request/response data is passed as raw bytes (without any serialization/deserialization steps).
///
class RawServiceClient final : public detail::ClientBase
{
public:
private:
    friend class Presentation;  // NOLINT cppcoreguidelines-virtual-class-destructor

    explicit RawServiceClient(detail::ClientImpl* const impl)
        : ClientBase{impl}
    {
    }

    // MARK: Data members:

};  // RawServiceClient

}  // namespace presentation
}  // namespace libcyphal

#endif  // LIBCYPHAL_PRESENTATION_CLIENT_HPP_INCLUDED
