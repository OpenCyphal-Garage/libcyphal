/// @copyright
/// Copyright (C) OpenCyphal Development Team  <opencyphal.org>
/// Copyright Amazon.com Inc. or its affiliates.
/// SPDX-License-Identifier: MIT

#ifndef LIBCYPHAL_APPLICATION_NODE_GETINFO_HPP_INCLUDED
#define LIBCYPHAL_APPLICATION_NODE_GETINFO_HPP_INCLUDED

#include "libcyphal/presentation/presentation.hpp"
#include "libcyphal/presentation/server.hpp"
#include "libcyphal/types.hpp"

#include <cetl/pf17/cetlpf.hpp>

#include <uavcan/node/GetInfo_1_0.hpp>

#include <chrono>
#include <utility>

namespace libcyphal
{
namespace application
{
namespace node
{

/// @brief Defines 'GetInfo' component for the application node.
///
class GetInfo final
{
    using Service = uavcan::node::GetInfo_1_0;

public:
    /// @brief Defines the response type for the GetInfo.
    ///
    using Response = Service::Response;

    /// @brief Factory method to create a GetInfo instance.
    ///
    /// @param presentation The presentation layer instance. In use to create 'GetInfo' service server.
    /// @return The GetInfo instance or a failure.
    ///
    static auto make(presentation::Presentation& presentation)
        -> Expected<GetInfo, presentation::Presentation::MakeFailure>
    {
        auto maybe_get_info_srv = presentation.makeServer<Service>();
        if (auto* const failure = cetl::get_if<presentation::Presentation::MakeFailure>(&maybe_get_info_srv))
        {
            return std::move(*failure);
        }

        return GetInfo{presentation, cetl::get<Server>(std::move(maybe_get_info_srv))};
    }

    GetInfo(GetInfo&& other) noexcept
        : presentation_{other.presentation_}
        , server_{std::move(other.server_)}
        , response_{std::move(other.response_)}
        , response_timeout_{other.response_timeout_}
    {
        // We have to set up request callback again (b/c it captures its own `this` pointer),
        setupOnRequestCallback();
    }

    ~GetInfo() = default;

    GetInfo(const GetInfo&)                = delete;
    GetInfo& operator=(const GetInfo&)     = delete;
    GetInfo& operator=(GetInfo&&) noexcept = delete;

    /// @brief Gets reference to the GetInfo response instance.
    ///
    /// Could be used to setup the response data. Initially, the response is empty (has default values)
    /// except for the protocol version, which is set to '1.0'.
    ///
    Response& response() noexcept
    {
        return response_;
    }

    /// @brief Sets the response transmission timeout (default is 1s).
    ///
    /// @param timeout Duration of the response transmission timeout. Applied for the next response transmission.
    ///
    void setResponseTimeout(const Duration& timeout) noexcept
    {
        response_timeout_ = timeout;
    }

private:
    using Server = presentation::ServiceServer<Service>;

    GetInfo(presentation::Presentation& presentation, Server&& server)
        : presentation_{presentation}
        , server_{std::move(server)}
        , response_{Response::allocator_type{&presentation.memory()}}
        , response_timeout_{std::chrono::seconds{1}}
    {
        response_.protocol_version.major = 1;
        setupOnRequestCallback();
    }

    void setupOnRequestCallback()
    {
        server_.setOnRequestCallback([this](const auto& arg, auto continuation) {
            //
            continuation(arg.approx_now + response_timeout_, response_);
        });
    }

    // MARK: Data members:

    presentation::Presentation& presentation_;
    Server                      server_;
    Response                    response_;
    Duration                    response_timeout_;

};  // GetInfo

}  // namespace node
}  // namespace application
}  // namespace libcyphal

#endif  // LIBCYPHAL_APPLICATION_NODE_GETINFO_HPP_INCLUDED
