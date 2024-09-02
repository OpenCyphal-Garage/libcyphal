/// @copyright
/// Copyright (C) OpenCyphal Development Team  <opencyphal.org>
/// Copyright Amazon.com Inc. or its affiliates.
/// SPDX-License-Identifier: MIT

#ifndef LIBCYPHAL_PRESENTATION_CLIENT_HPP_INCLUDED
#define LIBCYPHAL_PRESENTATION_CLIENT_HPP_INCLUDED

#include "client_impl.hpp"
#include "presentation_delegate.hpp"
#include "response_promise.hpp"

#include "libcyphal/transport/errors.hpp"
#include "libcyphal/transport/types.hpp"
#include "libcyphal/types.hpp"

#include <cetl/cetl.hpp>
#include <nunavut/support/serialization.hpp>

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
        , priority_{other.priority_}
    {
        CETL_DEBUG_ASSERT(other.impl_ != nullptr, "Not supposed to copy construct from already moved `other`.");

        impl_->retain();
    }

    ClientBase(ClientBase&& other) noexcept
        : impl_{std::exchange(other.impl_, nullptr)}
        , priority_{other.priority_}
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

            impl_     = other.impl_;
            priority_ = other.priority_;

            impl_->retain();
        }
        return *this;
    }

    ClientBase& operator=(ClientBase&& other) noexcept
    {
        CETL_DEBUG_ASSERT(impl_ != nullptr, "Not supposed to move construct to already moved `this`.");
        CETL_DEBUG_ASSERT(other.impl_ != nullptr, "Not supposed to move construct from already moved `other`.");

        impl_->release();

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
    ~ClientBase()
    {
        if (impl_ != nullptr)
        {
            impl_->release();
        }
    }

    explicit ClientBase(ClientImpl* const impl)
        : impl_{impl}
        , priority_{transport::Priority::Nominal}
    {
        CETL_DEBUG_ASSERT(impl_ != nullptr, "");

        impl_->retain();
    }

private:
    // MARK: Data members:

    ClientImpl*         impl_;
    transport::Priority priority_;

};  // ClientBase

}  // namespace detail

/// @brief Defines a custom strong-typed RPC client class.
///
/// Although the client class is not requiring specifically Nunavut tool generated request/response types,
/// it follows patterns of the tool (and has dependency on its `SerializeResult` and `bitspan` helper types),
/// so it is highly recommended to use DSDL file and the tool to generate the types.
/// Otherwise, see below requirements for the `Request` and `Response` types, as well as consult with
/// Nunavut's generated code (f.e. for the signatures of expected `serialize` and `deserialize` functions).
///
/// @tparam Request The request type of the client. This type has the following requirements:
///                 - contains `_traits_::SerializationBufferSizeBytes` constant
///                 - has freestanding `serialize` function under its namespace (so that ADL will find it)
/// @tparam Response The response type of the client. This type has the following requirements:
///                 - default constructible
///                 - contains `_traits_::ExtentBytes` constant
///                 - has freestanding `deserialize` function under its namespace (so that ADL will find it).
///
template <typename Request, typename Response>
class Client final : public detail::ClientBase
{
public:
    /// @brief Defines failure type for a strong-typed client operations.
    ///
    /// The set of possible failures includes transport layer failures (inherited from the base client),
    /// as well as serialization-related ones.
    ///
    using Failure = libcyphal::detail::AppendType<Failure, nunavut::support::Error>::Result;

private:
    friend class Presentation;  // NOLINT cppcoreguidelines-virtual-class-destructor

    explicit Client(detail::ClientImpl* const impl)
        : ClientBase{impl}
    {
    }

};  // Client<Request, Response>

/// @brief Defines a service typed RPC client class.
///
/// Although the client class is not requiring specifically Nunavut tool generated service type, it follows patterns
/// of the tool, so it is highly recommended to use DSDL file and the tool to generate the client type.
/// Otherwise, see below requirements for the `Service` type, and also `Client<Request, Response>` for details.
///
/// @tparam Service The service type of the client. This type has the following requirements:
///                 - Has `_traits_::IsService` boolean constant equal `true`.
///                 - Has nested `Request` type. See `Client<Request, ...>` for details.
///                 - Has nested `Response` type. See `Client<..., Response>` for details.
///
template <typename Service, typename = std::enable_if_t<detail::IsServiceTrait<Service>::value>>
using ServiceClient = Client<typename Service::Request, typename Service::Response>;

/// @brief Defines a raw (aka untyped) RPC client class.
///
/// The client class has no requirements for the request and response data (neither any Nunavut dependencies).
/// The request/response data is passed as raw bytes (without any serialization/deserialization steps).
///
class RawServiceClient final : public detail::ClientBase
{
public:
private:
    friend class Presentation;

    explicit RawServiceClient(detail::ClientImpl* const impl)
        : ClientBase{impl}
    {
    }

};  // RawServiceClient

}  // namespace presentation
}  // namespace libcyphal

#endif  // LIBCYPHAL_PRESENTATION_CLIENT_HPP_INCLUDED
