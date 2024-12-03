/// @copyright
/// Copyright (C) OpenCyphal Development Team  <opencyphal.org>
/// Copyright Amazon.com Inc. or its affiliates.
/// SPDX-License-Identifier: MIT

#ifndef LIBCYPHAL_APPLICATION_NODE_REGISTRY_PROVIDER_HPP_INCLUDED
#define LIBCYPHAL_APPLICATION_NODE_REGISTRY_PROVIDER_HPP_INCLUDED

#include "libcyphal/application/registry/registry.hpp"
#include "libcyphal/presentation/presentation.hpp"
#include "libcyphal/presentation/server.hpp"
#include "libcyphal/types.hpp"

#include <cetl/pf17/cetlpf.hpp>

#include <uavcan/_register/Access_1_0.hpp>
#include <uavcan/_register/List_1_0.hpp>

#include <chrono>
#include <utility>

namespace libcyphal
{
namespace application
{
namespace node
{

/// @brief Defines 'Registry' provider component for the application node.
///
/// Internally, it uses the registry 'List' and 'Access' service servers to handle incoming requests.
///
class RegistryProvider final
{
    using ListService   = uavcan::_register::List_1_0;
    using AccessService = uavcan::_register::Access_1_0;

public:
    /// @brief Factory method to create a RegistryProvider instance.
    ///
    /// @param presentation The presentation layer instance. In use to create 'List' and 'Access' service servers.
    /// @param registry Interface to the registry to be exposed by this provider.
    /// @return The RegistryProvider instance or a failure.
    ///
    static auto make(presentation::Presentation& presentation, registry::IIntrospectableRegistry& registry)
        -> Expected<RegistryProvider, presentation::Presentation::MakeFailure>
    {
        auto maybe_list_srv = presentation.makeServer<ListService>();
        if (auto* const failure = cetl::get_if<presentation::Presentation::MakeFailure>(&maybe_list_srv))
        {
            return std::move(*failure);
        }
        auto maybe_access_srv = presentation.makeServer<AccessService>();
        if (auto* const failure = cetl::get_if<presentation::Presentation::MakeFailure>(&maybe_access_srv))
        {
            return std::move(*failure);
        }

        return RegistryProvider{presentation,
                                registry,
                                cetl::get<ListServer>(std::move(maybe_list_srv)),
                                cetl::get<AccessServer>(std::move(maybe_access_srv))};
    }

    RegistryProvider(RegistryProvider&& other) noexcept
        : presentation_{other.presentation_}
        , registry_{other.registry_}
        , list_srv_{std::move(other.list_srv_)}
        , access_srv_{std::move(other.access_srv_)}
        , response_timeout_{other.response_timeout_}
        , pmr_alloc_{other.pmr_alloc_}
    {
        setupOnRequestCallbacks();
    }

    ~RegistryProvider() = default;

    RegistryProvider(const RegistryProvider&)                = delete;
    RegistryProvider& operator=(const RegistryProvider&)     = delete;
    RegistryProvider& operator=(RegistryProvider&&) noexcept = delete;

    /// @brief Sets the response transmission timeout (default is 1s).
    ///
    /// @param timeout Duration of the response transmission timeout. Applied for the next response transmission.
    ///
    void setResponseTimeout(const Duration& timeout) noexcept
    {
        response_timeout_ = timeout;
    }

private:
    using Name         = registry::IRegister::Name;
    using ListServer   = presentation::ServiceServer<ListService>;
    using AccessServer = presentation::ServiceServer<AccessService>;

    RegistryProvider(presentation::Presentation&        presentation,
                     registry::IIntrospectableRegistry& registry,
                     ListServer&&                       list_srv,
                     AccessServer&&                     access_srv)
        : presentation_{presentation}
        , registry_{registry}
        , list_srv_{std::move(list_srv)}
        , access_srv_{std::move(access_srv)}
        , response_timeout_{std::chrono::seconds{1}}
        , pmr_alloc_{&presentation.memory()}
    {
        // We have to set up request callback again (b/c it captures its own `this` pointer),
        setupOnRequestCallbacks();
    }

    void setupOnRequestCallbacks()
    {
        list_srv_.setOnRequestCallback([this](const auto& arg, auto continuation) {
            //
            const auto reg_name = registry::makeRegisterName(pmr_alloc_, registry_.index(arg.request.index));
            const ListService::Response response{reg_name, pmr_alloc_};

            // There is nothing we can do about possible continuation failures - we just ignore them.
            // TODO: Introduce error handler at the node level.
            (void) continuation(arg.approx_now + response_timeout_, response);
        });
        access_srv_.setOnRequestCallback([this](const auto& arg, auto continuation) {
            //
            const auto name = registry::makeStringView(arg.request.name.name);
            if (!arg.request.value.is_empty())
            {
                (void) registry_.set(name, arg.request.value);
            }

            AccessService::Response response{pmr_alloc_};
            if (auto value_and_flags = registry_.get(name))
            {
                response.value      = std::move(value_and_flags->value);
                response._mutable   = value_and_flags->flags._mutable;
                response.persistent = value_and_flags->flags.persistent;
            }

            // There is nothing we can do about possible continuation failures - we just ignore them.
            // TODO: Introduce error handler at the node level.
            (void) continuation(arg.approx_now + response_timeout_, response);
        });
    }

    // MARK: Data members:

    presentation::Presentation&            presentation_;
    registry::IIntrospectableRegistry&     registry_;
    ListServer                             list_srv_;
    AccessServer                           access_srv_;
    Duration                               response_timeout_;
    cetl::pmr::polymorphic_allocator<void> pmr_alloc_;

};  // RegistryProvider

}  // namespace node
}  // namespace application
}  // namespace libcyphal

#endif  // LIBCYPHAL_APPLICATION_NODE_REGISTRY_PROVIDER_HPP_INCLUDED
