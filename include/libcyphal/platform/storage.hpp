/// @copyright
/// Copyright (C) OpenCyphal Development Team  <opencyphal.org>
/// Copyright Amazon.com Inc. or its affiliates.
/// SPDX-License-Identifier: MIT

#ifndef LIBCYPHAL_PLATFORM_STORAGE_HPP_INCLUDED
#define LIBCYPHAL_PLATFORM_STORAGE_HPP_INCLUDED

#include "libcyphal/types.hpp"

#include <cetl/pf17/cetlpf.hpp>
#include <cetl/pf20/cetlpf.hpp>

#include <cstddef>
#include <cstdint>

namespace libcyphal
{
namespace platform
{
namespace storage
{

/// Defines possible errors that can occur during storage operations.
///
enum class Error : std::uint8_t
{
    Existence,  ///< Entry does not exist but should; or exists but shouldn't.
    API,        ///< Bad API invocation (e.g., null pointer).
    Capacity,   ///< No space left on the storage device.
    IO,         ///< Device input/output error.
    Internal,   ///< Internal failure in the filesystem (storage corruption or logic error).

};  // Error

/// Defines interface of a very simple API for storing and retrieving named blobs.
///
/// The underlying storage implementation is required to be power-loss tolerant and to
/// validate data integrity per key (e.g., using CRC and such).
/// This interface is fully blocking and should only be used during initialization and shutdown,
/// never during normal operation. Non-blocking adapters can be built on top of it.
///
class IKeyValue
{
public:
    IKeyValue(IKeyValue&&)                 = delete;
    IKeyValue(const IKeyValue&)            = delete;
    IKeyValue& operator=(IKeyValue&&)      = delete;
    IKeyValue& operator=(const IKeyValue&) = delete;

    /// Retrieve data by a key.
    ///
    /// If the key does not exist, the `Error::Existence` is returned.
    ///
    /// @param key The key of the value to retrieve.
    /// @param data The buffer to write the data to.
    /// @return Either the number of bytes read or an error.
    ///
    virtual auto get(const cetl::string_view key, const cetl::span<std::uint8_t> data) const  //
        -> Expected<std::size_t, Error> = 0;

    /// Store data under a key.
    ///
    /// Existing data, if any, is replaced entirely.
    /// New file and its parent directories created implicitly.
    /// Either all or none of the data bytes are written.
    ///
    /// @param key The key of the value to store.
    /// @param data The buffer to read the data from.
    /// @return Either an error or nothing.
    ///
    virtual auto put(const cetl::string_view key, const cetl::span<const std::uint8_t> data)  //
        -> cetl::optional<Error> = 0;

    /// Remove data under a key.
    ///
    /// If the key does not exist, the existence error is returned.
    ///
    /// @param key The key of the value to remove.
    /// @return Either an error or nothing.
    ///
    virtual auto drop(const cetl::string_view key) -> cetl::optional<Error> = 0;

protected:
    IKeyValue()  = default;
    ~IKeyValue() = default;

};  // IKeyValue

}  // namespace storage
}  // namespace platform
}  // namespace libcyphal

#endif  // LIBCYPHAL_PLATFORM_STORAGE_HPP_INCLUDED
