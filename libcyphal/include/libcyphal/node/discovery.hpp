/// @copyright Copyright Amazon.com Inc. and its affiliates. All Rights Reserved.
/// @file
/// Used to either discover the network for a node's ID or retrieve it from some persistent storage.

#ifndef LIBCYPHAL_NODE_DISCOVERY_HPP_INCLUDED
#define LIBCYPHAL_NODE_DISCOVERY_HPP_INCLUDED

#include <limits>
#include "libcyphal/transport/id_types.hpp"
#include "libcyphal/types/common.hpp"
#include "libcyphal/types/status.hpp"

namespace libcyphal
{
namespace node
{

/// @brief An interface used to either discover the network for a node's ID or retrieve it from some persistent storage.
class Discovery
{
public:
    /// @brief Used the inform the caller about which type of Node ID it has
    enum class Type : EnumType
    {
        Unassigned = 0,  //!< The Node ID has not been defined
        Static,          //!< The Node ID was determined statically
        Persistent,      //!< The Node ID was loaded from some Persistent Storage
        Dynamic          //!< The Node ID was dynamically assigned.
    };

    /// @todo populate this from a higher level application to determine type
    ///       remove as needed and update dependencies.
    enum class TransportType : EnumType
    {
        None = 0,
    };

    /// @brief Used to determine the current state of the discovery process.
    /// @retval result_e::SUCCESS The Node ID was obtained
    /// @retval result_e::BUSY The Node ID is being obtained
    /// @retval result_e::NOT_AVAILABLE The Node ID is not available yet. Call @ref Start to begin the process.
    /// @retval All other values are failures.
    virtual Status getStatus() const noexcept = 0;

    /// @brief Used by the caller to determine the type of the Node ID returned
    /// @pre GetState must return State::kObtained
    /// @return Type The type of the ID returned to the informant. Will return Type::kUnassigned
    virtual Type getType() const noexcept = 0;

    /// @brief Returns the value of the Node ID for a specified Transport
    /// @param[in] name Transport name
    /// @return NodeID Returns zero until the state of Status() is successful.
    virtual NodeID getID(TransportType name) const noexcept = 0;

    /// @brief Used to start the process of obtaining a Node ID
    /// @pre GetState - if returns State::kUnattempted then this API should be called
    /// @post GetState - call until either State::kObtained or State::kFailure
    /// @retval result_e::NOT_READY The process can not start yet.
    /// @retval result_e::SUCCESS The process has started.
    /// @retval result_e::ALREADY_INITIALIZED The process has already completed once.
    virtual Status start() = 0;

protected:
    ~Discovery() = default;
};
}  // namespace node
}  // namespace libcyphal

#endif  // LIBCYPHAL_NODE_DISCOVERY_HPP_INCLUDED