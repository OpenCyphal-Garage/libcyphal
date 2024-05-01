/// @copyright
/// Copyright (C) OpenCyphal Development Team  <opencyphal.org>
/// Copyright Amazon.com Inc. or its affiliates.
/// SPDX-License-Identifier: MIT

#ifndef LIBCYPHAL_TRANSPORT_ERRORS_HPP_INCLUDED
#define LIBCYPHAL_TRANSPORT_ERRORS_HPP_INCLUDED

#include "libcyphal/types.hpp"

namespace libcyphal
{
namespace transport
{

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

struct PlatformError final
{
    std::uint32_t code;
};

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

#endif  // LIBCYPHAL_TRANSPORT_ERRORS_HPP_INCLUDED
