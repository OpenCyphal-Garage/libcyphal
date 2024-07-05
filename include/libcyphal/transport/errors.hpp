/// @copyright
/// Copyright (C) OpenCyphal Development Team  <opencyphal.org>
/// Copyright Amazon.com Inc. or its affiliates.
/// SPDX-License-Identifier: MIT

#ifndef LIBCYPHAL_TRANSPORT_ERRORS_HPP_INCLUDED
#define LIBCYPHAL_TRANSPORT_ERRORS_HPP_INCLUDED

#include "libcyphal/types.hpp"

#include <cetl/pf17/cetlpf.hpp>
#include <cetl/rtti.hpp>
#include <cetl/unbounded_variant.hpp>

#include <cstdint>

namespace libcyphal
{
namespace transport
{

// TODO: Add docs about taxonomy of results, successes, failures, and primitive errors.

struct StateError final
{};

struct AnonymousError final
{};

struct ArgumentError final
{};

struct MemoryError final
{};

struct CapacityError final
{};

/// @brief Defines platform-specific error.
///
class IPlatformError
{
public:
    /// Gets platform-specific error code.
    ///
    virtual std::uint32_t code() const noexcept = 0;

protected:
    IPlatformError()                                     = default;
    ~IPlatformError()                                    = default;
    IPlatformError(const IPlatformError&)                = default;
    IPlatformError(IPlatformError&&) noexcept            = default;
    IPlatformError& operator=(const IPlatformError&)     = default;
    IPlatformError& operator=(IPlatformError&&) noexcept = default;
};
using PlatformError = ImplementationCell<IPlatformError, cetl::unbounded_variant<sizeof(void*) * 3>>;

struct AlreadyExistsError final
{};

// TODO: Delete it when everything is implemented.
struct NotImplementedError final
{};

/// @brief Defines any possible error at Cyphal transport layer.
///
using AnyError = cetl::variant<StateError,
                               AnonymousError,
                               ArgumentError,
                               MemoryError,
                               CapacityError,
                               PlatformError,
                               AlreadyExistsError,
                               NotImplementedError>;

/// @brief Defines any possible factory error at Cyphal transport layer.
///
using FactoryError = cetl::variant<ArgumentError, MemoryError, NotImplementedError>;

/// @brief Defines any possible error at Cyphal media layer.
///
using MediaError = cetl::variant<ArgumentError, PlatformError, CapacityError>;

}  // namespace transport
}  // namespace libcyphal

namespace cetl
{

// C6271889-BCF8-43A9-8D79-FA64FC3EFD93
template <>
constexpr type_id type_id_getter<libcyphal::transport::AnyError>() noexcept
{
    // NOLINTNEXTLINE(cppcoreguidelines-avoid-magic-numbers, readability-magic-numbers)
    return {0xC6, 0x27, 0x18, 0x89, 0xBC, 0xF8, 0x43, 0xA9, 0x8D, 0x79, 0xFA, 0x64, 0xFC, 0x3E, 0xFD, 0x93};
}

}  // namespace cetl

#endif  // LIBCYPHAL_TRANSPORT_ERRORS_HPP_INCLUDED
