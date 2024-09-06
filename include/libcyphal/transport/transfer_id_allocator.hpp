/// @copyright
/// Copyright (C) OpenCyphal Development Team  <opencyphal.org>
/// Copyright Amazon.com Inc. or its affiliates.
/// SPDX-License-Identifier: MIT

#ifndef LIBCYPHAL_TRANSPORT_TRANSFER_ID_ALLOCATOR_HPP_INCLUDED
#define LIBCYPHAL_TRANSPORT_TRANSFER_ID_ALLOCATOR_HPP_INCLUDED

#include "types.hpp"

#include <cetl/cetl.hpp>
#include <cetl/pf17/cetlpf.hpp>

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

/// @brief Defines a trivial transfer ID allocator.
///
/// The allocator is trivial in the sense that it simply modulo increments the transfer ID.
/// B/c modulo is expected to be quite big (like >= 2^48), collisions of transfer ids are unlikely.
/// Normally in use for UDP transport, where the modulo is `2^64 - 1`.
///
class TrivialTransferIdAllocator
{
public:
    explicit TrivialTransferIdAllocator(const TransferId modulo)
        : modulo_{modulo}
    {
    }

    CETL_NODISCARD transport::TransferId allocateTransferId()
    {
        return std::exchange(next_transfer_id_, (next_transfer_id_ + 1) % modulo_);
    }

private:
    // MARK: Data members:

    const transport::TransferId modulo_;
    transport::TransferId       next_transfer_id_{0};

};  // TrivialTransferIdAllocator

}  // namespace detail
}  // namespace transport
}  // namespace libcyphal

#endif  // LIBCYPHAL_TRANSPORT_TRANSFER_ID_ALLOCATOR_HPP_INCLUDED
