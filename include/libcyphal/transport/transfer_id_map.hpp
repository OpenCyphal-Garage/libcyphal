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

namespace libcyphal
{
namespace transport
{

/// @brief Defines an abstract interface of a transport id map.
///
/// Presentation layer uses this interface to map session specifiers to their transfer ids.
/// Users may provide a custom implementation of this interface to maintain/persist transfer IDs.
/// See `Presentation::setTransferIdMap` method for more details.
///
class ITransferIdMap
{
public:
    /// Hashable specifier of a session.
    ///
    struct SessionSpec
    {
        const PortId port_id;
        const NodeId node_id;

        bool operator==(const SessionSpec& other) const
        {
            return port_id == other.port_id && node_id == other.node_id;
        }

    };  // SessionSpec

    ITransferIdMap(const ITransferIdMap&)                = delete;
    ITransferIdMap(ITransferIdMap&&) noexcept            = delete;
    ITransferIdMap& operator=(const ITransferIdMap&)     = delete;
    ITransferIdMap& operator=(ITransferIdMap&&) noexcept = delete;

    /// Gets the transfer ID for the given session specifier.
    ///
    /// An implementation is expected to be fast (at least O(log), better O(1)) and non-blocking.
    ///
    /// @param session_spec The unique session specifier.
    /// @return The transfer ID which was last set (by the `setIdFor`).
    ///         Or some default value (zero) if not set yet.
    ///
    virtual TransferId getIdFor(const SessionSpec& session_spec) const noexcept = 0;

    /// Sets the transfer ID for the given session specifier.
    ///
    /// An implementation is expected to be fast (at least O(log), better O(1)) and non-blocking.
    /// Depending on the implementation, the previously set transfer ids may be stored in memory
    /// or also persisted to a file (but probably on exit to fulfill the above performance expectations).
    ///
    virtual void setIdFor(const SessionSpec& session_spec, const TransferId transfer_id) noexcept = 0;

protected:
    ITransferIdMap()  = default;
    ~ITransferIdMap() = default;

};  // ITransferIdMap

/// Internal implementation details of the Transport layer.
/// Not supposed to be used directly by the users of the library.
///
namespace detail
{

/// @brief Defines an abstract storage interface of a transport id.
///
class ITransferIdStorage
{
public:
    ITransferIdStorage(const ITransferIdStorage&)                = delete;
    ITransferIdStorage(ITransferIdStorage&&) noexcept            = delete;
    ITransferIdStorage& operator=(const ITransferIdStorage&)     = delete;
    ITransferIdStorage& operator=(ITransferIdStorage&&) noexcept = delete;

    /// Loads last saved transfer ID.
    ///
    /// An implementation is expected to be fast (at least O(log), better O(1)) and non-blocking.
    ///
    /// @return The transfer ID which was last saved (by the `save` method).
    ///         Or some default value (zero) if not set yet.
    ///
    virtual TransferId load() const noexcept = 0;

    /// Saves the transfer ID.
    ///
    /// An implementation is expected to be fast (at least O(log), better O(1)) and non-blocking.
    ///
    virtual void save(const TransferId transfer_id) noexcept = 0;

protected:
    ITransferIdStorage()  = default;
    ~ITransferIdStorage() = default;

};  // ITransferIdStorage

/// @brief Defines a trivial transfer ID generator.
///
/// The generator is trivial in the sense that it simply increments the transfer ID.
/// B/c modulo is expected to be quite big (like >= 2^48), collisions of transfer ids are unlikely.
/// Normally in use for UDP transport, where the modulo is `2^64 - 1`.
///
class TrivialTransferIdGenerator
{
public:
    explicit TrivialTransferIdGenerator(ITransferIdStorage& transfer_id_storage) noexcept
        : transfer_id_storage_{transfer_id_storage}
    {
    }

    /// @brief Generates the next transfer ID for an output session.
    ///
    CETL_NODISCARD TransferId nextTransferId() const noexcept
    {
        const auto curr_transfer_id = transfer_id_storage_.load();
        const auto next_transfer_id = curr_transfer_id + 1;
        transfer_id_storage_.save(next_transfer_id);
        return curr_transfer_id;
    }

private:
    // MARK: Data members:

    ITransferIdStorage& transfer_id_storage_;

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
    SmallRangeTransferIdGenerator(const TransferId modulo, ITransferIdStorage& transfer_id_storage) noexcept
        : modulo_{modulo}
        , transfer_id_storage_{transfer_id_storage}
        , in_use_transfer_ids_{}
    {
        CETL_DEBUG_ASSERT(modulo > 0, "Transfer ID modulo must be greater than 0.");
        CETL_DEBUG_ASSERT(modulo <= Size, "Transfer ID modulo must be <= than `Size`.");
    }

    /// @brief Generates the next available (not in use) transfer ID for an output session.
    ///
    /// The worst-case complexity is linear of the number of pending requests.
    ///
    CETL_NODISCARD cetl::optional<TransferId> nextTransferId() noexcept
    {
        auto       curr_transfer_id  = transfer_id_storage_.load() % modulo_;
        const auto final_transfer_id = curr_transfer_id;
        while (in_use_transfer_ids_.test(curr_transfer_id))
        {
            curr_transfer_id = (curr_transfer_id + 1) % modulo_;
            if (curr_transfer_id == final_transfer_id)
            {
                return cetl::nullopt;
            }
        }
        const auto next_transfer_id = (curr_transfer_id + 1) % modulo_;
        transfer_id_storage_.save(next_transfer_id);
        return curr_transfer_id;
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

    const TransferId    modulo_;
    ITransferIdStorage& transfer_id_storage_;
    std::bitset<Size>   in_use_transfer_ids_;

};  // SmallRangeTransferIdGenerator

}  // namespace detail
}  // namespace transport
}  // namespace libcyphal

template <>
struct std::hash<libcyphal::transport::ITransferIdMap::SessionSpec>
{
    std::size_t operator()(const libcyphal::transport::ITransferIdMap::SessionSpec& spec) const noexcept
    {
        const std::size_t h1 = std::hash<libcyphal::transport::PortId>{}(spec.port_id);
        const std::size_t h2 = std::hash<libcyphal::transport::NodeId>{}(spec.node_id);
        return h1 ^ (h2 << 1);
    }
};

#endif  // LIBCYPHAL_TRANSPORT_TRANSFER_ID_GENERATORS_HPP_INCLUDED
