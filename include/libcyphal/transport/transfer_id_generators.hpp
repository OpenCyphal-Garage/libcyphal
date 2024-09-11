/// @copyright
/// Copyright (C) OpenCyphal Development Team  <opencyphal.org>
/// Copyright Amazon.com Inc. or its affiliates.
/// SPDX-License-Identifier: MIT

#ifndef LIBCYPHAL_TRANSPORT_TRANSFER_ID_GENERATORS_HPP_INCLUDED
#define LIBCYPHAL_TRANSPORT_TRANSFER_ID_GENERATORS_HPP_INCLUDED

#include "types.hpp"

#include <cetl/cetl.hpp>
#include <cetl/pf17/cetlpf.hpp>

#include <bitset>
#include <cstddef>
#include <utility>

namespace libcyphal
{
namespace transport
{

/// Internal implementation details of the Presentation layer.
/// Not supposed to be used directly by the users of the library.
///
namespace detail
{

/// @brief Defines a trivial transfer ID generator.
///
/// The generator is trivial in the sense that it simply increments the transfer ID.
/// B/c modulo is expected to be quite big (like >= 2^48), collisions of transfer ids are unlikely.
/// Normally in use for UDP transport, where the modulo is `2^64 - 1`.
///
class TrivialTransferIdGenerator
{
public:
    /// @brief Returns the next transfer ID.
    ///
    CETL_NODISCARD TransferId nextTransferId() noexcept
    {
        return std::exchange(next_transfer_id_, next_transfer_id_ + 1);
    }

    /// @brief Sets next transfer ID.
    ///
    /// In use for testing purposes.
    ///
    void setNextTransferId(const TransferId transfer_id) noexcept
    {
        next_transfer_id_ = transfer_id;
    }

private:
    // MARK: Data members:

    TransferId next_transfer_id_{0};

};  // TrivialTransferIdGenerator

// MARK: -

/// @brief Defines a small range transfer ID generator.
///
/// The generator tracks allocated transfer ids by using bitset operations.
/// Its `Size` and modulo is expected to be quite small (like <= 2^8).
/// Normally in use for CAN transport, where the modulo is `2^5`.
///
template <std::size_t Size>
class SmallRangeTransferIdGenerator
{
    static_assert(Size > 0, "Size must be greater than 0.");

public:
    explicit SmallRangeTransferIdGenerator(const TransferId modulo) noexcept
        : modulo_{modulo}
        , next_transfer_id_{0}
        , in_use_transfer_ids_{}
    {
        CETL_DEBUG_ASSERT(modulo > 0, "Transfer ID modulo must be greater than 0.");
        CETL_DEBUG_ASSERT(modulo <= Size, "Transfer ID modulo must be <= than `Size`.");
    }

    /// @brief Returns the next available (not in use) transfer ID.
    ///
    /// The worst-case complexity is linear of the number of pending requests.
    ///
    CETL_NODISCARD cetl::optional<TransferId> nextTransferId() noexcept
    {
        const auto end = next_transfer_id_;
        while (in_use_transfer_ids_.test(next_transfer_id_))
        {
            next_transfer_id_ = (next_transfer_id_ + 1) % modulo_;
            if (next_transfer_id_ == end)
            {
                return cetl::nullopt;
            }
        }

        return std::exchange(next_transfer_id_, (next_transfer_id_ + 1) % modulo_);
    }

    /// @brief Marks given transfer ID as in use.
    ///
    /// Such retained transfer IDs will be skipped by the `nextTransferId` method.
    ///
    void retainTransferId(const TransferId transfer_id) noexcept
    {
        CETL_DEBUG_ASSERT(transfer_id < modulo_, "Valid Transfer ID must be less than modulo.");
        in_use_transfer_ids_[transfer_id] = true;
    }

    /// @brief Marks given transfer ID as not in use anymore.
    ///
    void releaseTransferId(const TransferId transfer_id) noexcept
    {
        CETL_DEBUG_ASSERT(transfer_id < modulo_, "Valid Transfer ID must be less than modulo.");
        in_use_transfer_ids_[transfer_id] = false;
    }

private:
    // MARK: Data members:

    const TransferId  modulo_;
    TransferId        next_transfer_id_;
    std::bitset<Size> in_use_transfer_ids_;

};  // SmallRangeTransferIdGenerator

}  // namespace detail
}  // namespace transport
}  // namespace libcyphal

#endif  // LIBCYPHAL_TRANSPORT_TRANSFER_ID_GENERATORS_HPP_INCLUDED
