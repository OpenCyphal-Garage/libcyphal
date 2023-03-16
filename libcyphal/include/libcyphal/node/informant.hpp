/// @copyright Copyright Amazon.com Inc. and its affiliates. All Rights Reserved.
/// @file
/// Used by the Node to fetch some information for the Node.

#ifndef LIBCYPHAL_NODE_INFORMANT_HPP_INCLUDED
#define LIBCYPHAL_NODE_INFORMANT_HPP_INCLUDED

#include <cstdint>
#include <limits>
#include "libcyphal/types/common.hpp"
#include "libcyphal/types/status.hpp"

#include "cetl/pf20/span.hpp"

namespace libcyphal
{
namespace node
{

/// @brief A structure to hold the version number. Should follow semantic versioning
/// @see http://semvar.org
struct Version
{
    std::uint8_t major;
    std::uint8_t minor;
};

/// @brief The informant is used by the Node to fetch some information for the Node.
class Informant
{
public:
    constexpr static std::size_t MaxNameLength = 50;
    constexpr static std::size_t MaxCOALength  = 222;

    using Name = cetl::pf20::span<const char>;
    using COA  = cetl::pf20::span<const std::uint8_t>;

    /// @brief Used by the Node to set the Hardware Version.
    /// @return Version The major and minor build number
    virtual Version getHardwareVersion() const noexcept = 0;

    /// @brief Used by the Node to set the Software Version
    /// @return Version The major and minor build number
    virtual Version getSoftwareVersion() const noexcept = 0;

    /// @brief Used to set the revision of the Software from its repository using
    /// @return uint64_t The short git hash set on the code. Can return zero if no value is desired.
    virtual std::uint64_t getSoftwareRevision() const noexcept = 0;

    /// @brief Used by the Node to Fetch the unique ID.
    /// @return UID The reference to an output array to hold the ID.
    virtual const UID& getUniqueId() const noexcept = 0;

    /// @brief Used by the Node to retrieve the Node information.
    /// @return Name The reference to a fixed output array to hold the name. The name must be null terminated.
    virtual const Name& getName() const noexcept = 0;

    /// @brief Used by the Node to retrieve the Software CRC-64-WE value. Zero is a valid response
    virtual std::uint64_t getSoftwareCRC() const noexcept = 0;

    /// @brief Used by the Node to retrieve the COA.
    /// @return COA The reference to an output array to store the Certificate of Authority
    virtual const COA& getCertificateOfAuthority() const noexcept = 0;

    /// @brief Used to determine if the Informant is ready to return the Information about the Node.
    /// @retval result_e::NOT_READY
    /// @retval result_e::SUCCESS
    virtual Status getStatus() const noexcept = 0;

protected:
    ~Informant() = default;
};
}  // namespace node
}  // namespace libcyphal

#endif  // LIBCYPHAL_NODE_INFORMANT_HPP_INCLUDED
