/// @file
/// Contains metadata types and functions for the Cyphal transport layer.
///
/// @copyright
/// Copyright (C) OpenCyphal Development Team  <opencyphal.org>
/// Copyright Amazon.com Inc. or its affiliates.
/// SPDX-License-Identifier: MIT
///

#ifndef LIBCYPHAL_TRANSPORT_PAYLOAD_METADATA_HPP_INCLUDED
#define LIBCYPHAL_TRANSPORT_PAYLOAD_METADATA_HPP_INCLUDED

#include <cstddef>

namespace libcyphal
{
namespace transport
{

/// This information is obtained from the data type definition.
///
/// Eventually, this type might include the runtime type identification information,
/// if it is ever implemented in Cyphal. The alpha revision used to contain the "data type hash" field here,
/// but this concept was found deficient and removed from the proposal.
/// You can find related discussion in https://forum.opencyphal.org/t/alternative-transport-protocols-in-uavcan/324.
struct PayloadMetadata
{
    /// The minimum amount of memory required to hold any serialized representation of any compatible version
    /// of the data type; or, on other words, it is the the maximum possible size of received objects.
    /// The size is specified in bytes because extent is guaranteed (by definition) to be an integer number of bytes
    /// long.
    ///
    /// This parameter is determined by the data type author at the data type definition time.
    /// It is typically larger than the maximum object size in order to allow the data type author to
    /// introduce more fields in the future versions of the type;
    /// for example, ``MyMessage.1.0`` may have the maximum size of 100 bytes and the extent 200 bytes;
    /// a revised version ``MyMessage.1.1`` may have the maximum size anywhere between 0 and 200 bytes.
    /// It is always safe to pick a larger value if not sure.
    /// You will find a more rigorous description in the Cyphal Specification.
    ///
    /// Transport implementations may use this information to statically size receive buffers.
    std::size_t extent_bytes;
};

}  // namespace transport
}  // namespace libcyphal

#endif  // LIBCYPHAL_TRANSPORT_PAYLOAD_METADATA_HPP_INCLUDED
