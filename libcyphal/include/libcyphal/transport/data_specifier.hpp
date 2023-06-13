/// @file
/// Data specifier objects for Transports.
///
/// @copyright
/// Copyright (C) OpenCyphal Development Team  <opencyphal.org>
/// Copyright Amazon.com Inc. or its affiliates.
/// SPDX-License-Identifier: MIT
///

#ifndef LIBCYPHAL_TRANSPORT_DATA_SPECIFIER_HPP_INCLUDED
#define LIBCYPHAL_TRANSPORT_DATA_SPECIFIER_HPP_INCLUDED

#include <cstdint>

#include "libcyphal/libcyphal.hpp"
#include "libcyphal/network/ip/udp.hpp"

namespace libcyphal
{
namespace transport
{

class DataSpecifier final
{
public:
    static janky::optional<DataSpecifier> serviceProvider(std::uint16_t service_id) noexcept
    {
        const std::uint16_t mask = getMask(Role::ServiceProvider);
        if (service_id > mask)
        {
            return janky::nullopt;
        }
        else
        {
            return janky::make_optional<DataSpecifier>(static_cast<std::uint16_t>(service_id & mask), Role::ServiceProvider);
        }
    }
    static janky::optional<DataSpecifier> serviceConsumer(std::uint16_t service_id) noexcept
    {
        const std::uint16_t mask = getMask(Role::ServiceConsumer);
        if (service_id > mask)
        {
            return janky::nullopt;
        }
        else
        {
            return janky::make_optional<DataSpecifier>(static_cast<std::uint16_t>(service_id & mask), Role::ServiceConsumer);
        }
    }
    static janky::optional<DataSpecifier> message(std::uint16_t subject_id) noexcept
    {
        const std::uint16_t mask = getMask(Role::Message);
        if (subject_id > mask)
        {
            return janky::nullopt;
        }
        else
        {
            return janky::make_optional<DataSpecifier>(static_cast<std::uint16_t>(subject_id & mask), Role::Message);
        }
    }
    /// The role the specifier is for. This role will modify the specifier's identifier.
    enum class Role : std::uint16_t
    {
        Message = 0, ///< Multicast message role.
        ServiceProvider = 1, ///< Request output role is for clients. Request input role is for servers.
        ServiceConsumer = 2  ///< Response output role is for servers. Response input role is for clients.
    };

    DataSpecifier(std::uint16_t id, Role role)
        : id_{id}
        , role_{role}
    {
    }
    ~DataSpecifier() noexcept = default;
    DataSpecifier(const DataSpecifier& rhs) noexcept
        : id_{rhs.id_}
        , role_{rhs.role_}
    {
    }

    DataSpecifier(DataSpecifier&& rhs)
        : id_{rhs.id_}
        , role_{rhs.role_}
    {}

    DataSpecifier& operator=(DataSpecifier&&)      = delete;
    DataSpecifier& operator=(const DataSpecifier&) = delete;

    constexpr std::uint16_t getId() const noexcept
    {
        return id_;
    }

    constexpr Role getRole() const noexcept
    {
        return role_;
    }

    constexpr bool isService() const noexcept
    {
        return role_ != Role::Message;
    }

    std::size_t hash() const noexcept
    {
        return std::hash<std::uint16_t>{}(id_) ^ (std::hash<Role>{}(role_) << 1);
    }

    bool operator==(const DataSpecifier& rhs) const noexcept
    {
        return (id_ == rhs.id_) && (role_ == rhs.role_);
    }
private:
    static constexpr std::uint16_t getMask(Role role) noexcept
    {
        return (role == Role::Message) ? network::ip::udp::SubjectIdMask : network::ip::udp::ServiceIdMask;
    }

    const std::uint16_t id_;
    const Role          role_;
};

}  // namespace transport
}  // namespace libcyphal
#endif  // LIBCYPHAL_TRANSPORT_DATA_SPECIFIER_HPP_INCLUDED
