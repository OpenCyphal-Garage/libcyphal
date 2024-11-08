/// @copyright
/// Copyright (C) OpenCyphal Development Team  <opencyphal.org>
/// Copyright Amazon.com Inc. or its affiliates.
/// SPDX-License-Identifier: MIT

#ifndef LIBCYPHAL_APPLICATION_NODE_GETINFO_PROVIDER_HPP_INCLUDED
#define LIBCYPHAL_APPLICATION_NODE_GETINFO_PROVIDER_HPP_INCLUDED

#include "libcyphal/presentation/presentation.hpp"
#include "libcyphal/presentation/server.hpp"
#include "libcyphal/types.hpp"

#include <cetl/pf17/cetlpf.hpp>
#include <cetl/pf20/cetlpf.hpp>

#include <uavcan/node/GetInfo_1_0.hpp>

#include <algorithm>
#include <chrono>
#include <cstdint>
#include <iterator>
#include <utility>

namespace libcyphal
{
namespace application
{
namespace node
{

/// @brief Defines 'GetInfo' provider component for the application node.
///
/// Internally, it uses the 'GetInfo' service server to handle incoming requests.
///
/// No Sonar cpp:S4963 'The "Rule-of-Zero" should be followed'
/// b/c we do directly handle resources here (namely capturing of `this` in the request callback).
///
class GetInfoProvider final  // NOSONAR cpp:S4963
{
    using Service = uavcan::node::GetInfo_1_0;

public:
    /// @brief Defines the response type for the GetInfo provider.
    ///
    using Response = Service::Response;

    /// @brief Factory method to create a GetInfo instance.
    ///
    /// @param presentation The presentation layer instance. In use to create 'GetInfo' service server.
    /// @return The GetInfo provider instance or a failure.
    ///
    static auto make(presentation::Presentation& presentation)
        -> Expected<GetInfoProvider, presentation::Presentation::MakeFailure>
    {
        auto maybe_get_info_srv = presentation.makeServer<Service>();
        if (auto* const failure = cetl::get_if<presentation::Presentation::MakeFailure>(&maybe_get_info_srv))
        {
            return std::move(*failure);
        }

        return GetInfoProvider{presentation, cetl::get<Server>(std::move(maybe_get_info_srv))};
    }

    GetInfoProvider(GetInfoProvider&& other) noexcept
        : presentation_{other.presentation_}
        , server_{std::move(other.server_)}
        , response_{std::move(other.response_)}
        , response_timeout_{other.response_timeout_}
    {
        // We have to set up request callback again (b/c it captures its own `this` pointer),
        setupOnRequestCallback();
    }

    ~GetInfoProvider() = default;

    GetInfoProvider(const GetInfoProvider&)                = delete;
    GetInfoProvider& operator=(const GetInfoProvider&)     = delete;
    GetInfoProvider& operator=(GetInfoProvider&&) noexcept = delete;

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
    /// @return Reference to self for method chaining.
    ///
    GetInfoProvider& setResponseTimeout(const Duration& timeout) noexcept
    {
        response_timeout_ = timeout;
        return *this;
    }

    /// @brief Sets the node unique 128-bit id in the GetInfo response.
    ///
    /// Default is all zeros.
    /// Truncates the id if it exceeds 16 bytes capacity of the response field.
    ///
    /// @return Reference to self for method chaining.
    ///
    GetInfoProvider& setUniqueId(const cetl::span<const std::uint8_t> id) noexcept
    {
        response_.unique_id = {};
        std::copy_n(id.data(), std::min(id.size(), response_.unique_id.size()), response_.unique_id.begin());
        return *this;
    }

    // using software_vcs_revision_id = std::uint64_t;
    // using software_image_crc = cetl::VariableLengthArray<std::uint64_t,
    // std::allocator_traits<allocator_type>::rebind_alloc<std::uint64_t>>;

    /// @brief Sets the node protocol version in the GetInfo response.
    ///
    /// Default is '1.0'.
    ///
    /// @return Reference to self for method chaining.
    ///
    GetInfoProvider& setProtocolVersion(const std::uint8_t major, const std::uint8_t minor) noexcept  // NOLINT
    {
        response_.protocol_version.major = major;
        response_.protocol_version.minor = minor;
        return *this;
    }

    /// @brief Sets the node hardware version in the GetInfo response.
    ///
    /// Default is '0.0'.
    ///
    /// @return Reference to self for method chaining.
    ///
    GetInfoProvider& setHardwareVersion(const std::uint8_t major, const std::uint8_t minor) noexcept  // NOLINT
    {
        response_.hardware_version.major = major;
        response_.hardware_version.minor = minor;
        return *this;
    }

    /// @brief Sets the node software version in the GetInfo response.
    ///
    /// Default is '0.0'.
    ///
    /// @return Reference to self for method chaining.
    ///
    GetInfoProvider& setSoftwareVersion(const std::uint8_t major, const std::uint8_t minor) noexcept  // NOLINT
    {
        response_.software_version.major = major;
        response_.software_version.minor = minor;
        return *this;
    }

    /// @brief Sets the node name in the GetInfo response.
    ///
    /// Default is ''.
    /// Truncates the name if it exceeds the capacity of the response field.
    ///
    /// @return Reference to self for method chaining.
    ///
    GetInfoProvider& setName(const cetl::string_view name)
    {
        return setStringField<ArrayCapacity::name>(response_.name, name);
    }

    /// @brief Sets the node certificate of authenticity in the GetInfo response.
    ///
    /// Default is ''.
    /// Truncates the certificate if it exceeds the capacity of the response field.
    ///
    /// @return Reference to self for method chaining.
    ///
    GetInfoProvider& setCertificateOfAuthenticity(const cetl::string_view certificate)
    {
        return setStringField<ArrayCapacity::certificate_of_authenticity>(response_.certificate_of_authenticity,
                                                                          certificate);
    }

private:
    using ArrayCapacity = Response::_traits_::ArrayCapacity;
    using Server        = presentation::ServiceServer<Service>;

    GetInfoProvider(presentation::Presentation& presentation, Server&& server)
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
            // There is nothing we can do about possible continuation failures - we just ignore them.
            // TODO: Introduce error handler at the node level.
            (void) continuation(arg.approx_now + response_timeout_, response_);
        });
    }

    template <std::size_t Capacity, typename Field>
    GetInfoProvider& setStringField(Field& field, const cetl::string_view value)
    {
        const auto dst_size = std::min(value.size(), Capacity);
        field.clear();
        field.reserve(dst_size);
        std::copy_n(value.begin(), dst_size, std::back_inserter(field));
        return *this;
    }

    // MARK: Data members:

    presentation::Presentation& presentation_;
    Server                      server_;
    Response                    response_;
    Duration                    response_timeout_;

};  // GetInfoProvider

}  // namespace node
}  // namespace application
}  // namespace libcyphal

#endif  // LIBCYPHAL_APPLICATION_NODE_GETINFO_PROVIDER_HPP_INCLUDED
