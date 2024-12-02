/// @copyright
/// Copyright (C) OpenCyphal Development Team  <opencyphal.org>
/// Copyright Amazon.com Inc. or its affiliates.
/// SPDX-License-Identifier: MIT

#ifndef LIBCYPHAL_PRESENTATION_COMMON_HELPERS_HPP_INCLUDED
#define LIBCYPHAL_PRESENTATION_COMMON_HELPERS_HPP_INCLUDED

#include "libcyphal/config.hpp"
#include "libcyphal/errors.hpp"
#include "libcyphal/transport/scattered_buffer.hpp"

#include <cetl/pf17/cetlpf.hpp>
#include <cetl/pf20/cetlpf.hpp>

#include <nunavut/support/serialization.hpp>

#include <array>
#include <cstdint>
#include <memory>
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

using DeserializationFailure = cetl::variant<MemoryError, nunavut::support::Error>;

template <typename Message>
static cetl::optional<DeserializationFailure> tryDeserializePayload(const transport::ScatteredBuffer& payload,
                                                                    cetl::pmr::memory_resource&       memory,
                                                                    Message&                          out_message)
{
    // To reduce heap allocations, we use stack for "small" (<=256 bytes) messages.
    //
    // Strictly speaking, we could eliminate PMR allocation here in favor of a fixed-size stack buffer
    // (using `Message::_traits_::ExtentBytes` as the maximum possible size for the Message).
    // But this might be dangerous (stack overflow!) in case of large messages, so it's done only for small ones.
    //
    if (payload.size() <= config::presentation::SmallPayloadSize)
    {
        // Next nolint b/c we initialize buffer with payload copying.
        // NOLINTNEXTLINE(cppcoreguidelines-pro-type-member-init,hicpp-member-init)
        std::array<std::uint8_t, config::presentation::SmallPayloadSize> small_buffer;
        //
        const auto                            data_size = payload.copy(0, small_buffer.data(), payload.size());
        const nunavut::support::const_bitspan bitspan{small_buffer.data(), data_size};

        const nunavut::support::SerializeResult result = deserialize(out_message, bitspan);
        return result ? cetl::nullopt : cetl::optional<DeserializationFailure>(result.error());
    }

    // Make a copy of the scattered buffer into a single contiguous (PMR allocated) temp buffer.
    //
    const std::unique_ptr<cetl::byte, PmrRawBytesDeleter>
        tmp_buffer{static_cast<cetl::byte*>(memory.allocate(payload.size())),  // NOSONAR cpp:S5356 cpp:S5357
                   {payload.size(), &memory}};
    if (!tmp_buffer)
    {
        return MemoryError{};
    }
    const auto data_size = payload.copy(0, tmp_buffer.get(), payload.size());

    const auto* const data_raw = static_cast<const void*>(tmp_buffer.get());
    const auto* const data_u8s = static_cast<const std::uint8_t*>(data_raw);  // NOSONAR cpp:S5356 cpp:S5357
    const nunavut::support::const_bitspan bitspan{data_u8s, data_size};

    const nunavut::support::SerializeResult result = deserialize(out_message, bitspan);
    return result ? cetl::nullopt : cetl::optional<DeserializationFailure>(result.error());
}

template <typename Message, typename Result, std::size_t BufferSize, bool IsOnStack, typename Action>
static auto tryPerformOnSerialized(const Message&                    message,
                                   const cetl::pmr::memory_resource& memory,
                                   Action&&                          action) -> std::enable_if_t<IsOnStack, Result>
{
    // Not in use b/c we use stack buffer for small messages.
    (void) memory;

    // Try to serialize the message to raw payload buffer.
    //
    // Next nolint b/c we use a buffer to serialize the message, so no need to zero it (and performance better).
    // NOLINTNEXTLINE(cppcoreguidelines-pro-type-member-init,hicpp-member-init)
    std::array<cetl::byte, BufferSize> buffer;
    // TODO: Eliminate `reinterpret_cast` when Nunavut supports `cetl::byte` at its `serialize`.
    const auto result_size = serialize(message,
                                       // Next nolint & NOSONAR are currently unavoidable.
                                       // NOLINTNEXTLINE(cppcoreguidelines-pro-type-reinterpret-cast)
                                       {reinterpret_cast<std::uint8_t*>(buffer.data()),  // NOSONAR cpp:S3630,
                                        BufferSize});
    if (!result_size)
    {
        return result_size.error();
    }

    const cetl::span<const cetl::byte>                      data_span{buffer.data(), result_size.value()};
    const std::array<const cetl::span<const cetl::byte>, 1> fragments{data_span};

    return std::forward<Action>(action)(fragments);
}

template <typename Message, typename Result, std::size_t BufferSize, bool IsOnStack, typename Action>
static auto tryPerformOnSerialized(const Message&              message,
                                   cetl::pmr::memory_resource& memory,
                                   Action&&                    action) -> std::enable_if_t<!IsOnStack, Result>
{
    // Nolint and NoSonar b/c we use PMR allocation for raw bytes buffer.
    // NOLINTNEXTLINE(*-avoid-c-arrays)
    const std::unique_ptr<cetl::byte[], PmrRawBytesDeleter> buffer  // NOSONAR cpp:S5945 cpp:M23_356
        {static_cast<cetl::byte*>(memory.allocate(BufferSize)),     // NOSONAR cpp:S5356 cpp:S5357
         {BufferSize, &memory}};
    if (!buffer)
    {
        return MemoryError{};
    }

    // Try to serialize the message to raw payload buffer.
    //
    // Next nolint b/c we use a buffer to serialize the message, so no need to zero it (and performance better).
    // TODO: Eliminate `reinterpret_cast` when Nunavut supports `cetl::byte` at its `serialize`.
    const auto result_size = serialize(message,
                                       // Next nolint & NOSONAR are currently unavoidable.
                                       // NOLINTNEXTLINE(cppcoreguidelines-pro-type-reinterpret-cast)
                                       {reinterpret_cast<std::uint8_t*>(buffer.get()),  // NOSONAR cpp:S3630,
                                        BufferSize});
    if (!result_size)
    {
        return result_size.error();
    }

    const cetl::span<const cetl::byte>                      data_span{buffer.get(), result_size.value()};
    const std::array<const cetl::span<const cetl::byte>, 1> fragments{data_span};

    return std::forward<Action>(action)(fragments);
}

}  // namespace detail
}  // namespace presentation
}  // namespace libcyphal

#endif  // LIBCYPHAL_PRESENTATION_COMMON_HELPERS_HPP_INCLUDED
