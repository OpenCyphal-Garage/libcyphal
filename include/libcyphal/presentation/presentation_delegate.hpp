/// @copyright
/// Copyright (C) OpenCyphal Development Team  <opencyphal.org>
/// Copyright Amazon.com Inc. or its affiliates.
/// SPDX-License-Identifier: MIT

#ifndef LIBCYPHAL_PRESENTATION_DELEGATE_HPP_INCLUDED
#define LIBCYPHAL_PRESENTATION_DELEGATE_HPP_INCLUDED

#include "libcyphal/transport/transfer_id_map.hpp"
#include "shared_object.hpp"

#include <cetl/pf17/cetlpf.hpp>

#include <type_traits>

namespace libcyphal
{
namespace presentation
{

/// Internal implementation details of the Presentation layer.
/// Not supposed to be used directly by the users of the library.
///
namespace detail
{

/// Trait which determines whether the given type has `T::_traits_::IsService` field.
///
/// No Sonar cpp:S872 "Reconsider this operator for `bool` operand'
/// b/c we do need to check the existence of the field with help of `decltype` and `,` (comma) operator.
///
template <typename T>
auto HasIsServiceTrait(bool dummy) -> decltype(T::_traits_::IsService, std::true_type{});  // NOSONAR cpp:S872
template <typename>
std::false_type HasIsServiceTrait(...);

/// Trait which determines whether the given (supposed to be a Service) type
/// has nested `T::Request` type, constructible with PMR allocator.
///
template <typename Service>
auto HasServiceRequest(bool dummy)
    -> decltype(typename Service::Request{typename Service::Request::allocator_type{nullptr}}, std::true_type{});
template <typename>
std::false_type HasServiceRequest(...);

/// Trait which determines whether the given (supposed to be a Service) type
/// has nested `T::Response` type, constructible with PMR allocator.
///
template <typename Service>
auto HasServiceResponse(bool dummy)
    -> decltype(typename Service::Response{typename Service::Response::allocator_type{nullptr}}, std::true_type{});
template <typename>
std::false_type HasServiceResponse(...);

/// Trait which determines whether the given type is a service one.
///
/// A service type is expected to have `T::Request` and `T::Response` nested types,
/// as well as `Service::_traits_::IsService` boolean constant equal `true`.
///
template <typename T,
          bool = decltype(HasServiceRequest<T>(true))::value && decltype(HasServiceResponse<T>(true))::value &&
                 decltype(HasIsServiceTrait<T>(true))::value>
struct IsServiceTrait
{
    static constexpr bool value = false;
};
template <typename T>
struct IsServiceTrait<T, true>
{
    static constexpr bool value = T::_traits_::IsService;
};

// Forward declaration.
class SharedClient;
class PublisherImpl;
class SubscriberImpl;

/// @brief Defines internal interface for the Presentation layer delegate.
///
class IPresentationDelegate
{
public:
    IPresentationDelegate(const IPresentationDelegate&)                = delete;
    IPresentationDelegate(IPresentationDelegate&&) noexcept            = delete;
    IPresentationDelegate& operator=(const IPresentationDelegate&)     = delete;
    IPresentationDelegate& operator=(IPresentationDelegate&&) noexcept = delete;

    virtual cetl::pmr::memory_resource&       memory() const noexcept           = 0;
    virtual cetl::optional<transport::NodeId> getLocalNodeId() const noexcept   = 0;
    virtual transport::ITransferIdMap*        getTransferIdMap() const noexcept = 0;

    virtual void markSharedObjAsUnreferenced(SharedObject& shared_obj) noexcept = 0;
    virtual void forgetSharedClient(SharedClient& shared_client) noexcept       = 0;
    virtual void forgetPublisherImpl(PublisherImpl& publisher_impl) noexcept    = 0;
    virtual void forgetSubscriberImpl(SubscriberImpl& subscriber_impl) noexcept = 0;

protected:
    IPresentationDelegate()  = default;
    ~IPresentationDelegate() = default;

};  // IPresentationDelegate

}  // namespace detail
}  // namespace presentation
}  // namespace libcyphal

#endif  // LIBCYPHAL_PRESENTATION_DELEGATE_HPP_INCLUDED
