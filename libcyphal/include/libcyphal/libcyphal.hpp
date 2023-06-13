/// @file
/// Minimal required include to use libcyphal. All other libcyphal headers include this header either directly or
/// transitively.
///
/// @copyright
/// Copyright (C) OpenCyphal Development Team  <opencyphal.org>
/// Copyright Amazon.com Inc. or its affiliates.
/// SPDX-License-Identifier: MIT
///

#ifndef LIBCYPHAL_HPP_INCLUDED
#define LIBCYPHAL_HPP_INCLUDED

#include <cstdint>
#include <type_traits>
#include <chrono>

// +--------------------------------------------------------------------------+
/// @defgroup macros_versioning Versioning macros.
///
/// Macros specifying and controlling versions of the library, the Cyphal
/// specification, compilers, standards, etc.
/// @{

#define LIBCYPHAL_VERSION_MAJOR 3  // < libcyphal major version definition.
#define LIBCYPHAL_VERSION_MINOR 0  // < libcyphal minor version definition.

/// The Cyphal specification version this version of libcyphal adheres to.
#define LIBCYPHAL_CYPHAL_SPECIFICATION_VERSION_MAJOR 1

/// Libcyphal C++ version check - validate the version of the C++ standard used during compilation.
///
/// libcyphal v2.0 makes careful use of the standard library and requires features introduced
/// in c++14. While the library does not require standards newer then c++14 it does compile
/// correctly in code that compiles using newer standards.
///
/// Also note that libcyphal functions normally without enabling c++ exceptions and makes no
/// use of RTTI. It furthermore does not use the standard heap for allocations on any platform.
///
/// You can define LIBCYPHAL_CPP_VERSION_NOCHECK to skip these checks.

#ifndef LIBCYPHAL_CPP_VERSION_NOCHECK
#    if (__cplusplus >= 201402L)
#        define LIBCYPHAL_CPP_VERSION_NOCHECK 1
#    else
#  error Unsupported C++ standard (C++14 or greater required). You can explicitly set LIBCYPHAL_CPP_VERSION_NOCHECK to silence this error.
#    endif
#endif

/// @} // end of macros_versioning

namespace libcyphal
{

/// List of known results
enum class ResultCode : std::int8_t
{
    /// Values greater than 0 are success values for libcyphal results.
    Success = 1,

    /// The operation can be considered a success but could have been more robust.
    /// if a call is meant to send on at least one interface in a redundant group
    /// of interfaces, the result is a Success if all interfaces were available
    /// and accepted the data and a PartialSuccess if at least one interface was
    /// available and accepted the data.
    PartialSuccess = 2,

    /// Values of 0 or less are failure values for libcyphal results but Timeout
    /// may be considered a nominal case for some methods.
    Timeout = 0,

    UnknownError = -1,

    /// An operation could not be completed because of numeric overflow, invalid
    /// conversions, or other arithmetic errors.
    ArithmeticError = -2,

    /// Generic error for an error related to internal buffers.
    BufferError = -3,

    /// Generic error for failures to find a resource. These can be caused by
    /// key errors, index errors, or other lookup failures.
    LookupError = -4,

    /// Generic error for failures to allocate memory.
    MemoryError = -5,

    /// A function was not implemented for the given type.
    NotImplementedError = -6,

    /// A resource was closed and cannot be used.
    ResourceClosedError = -7,

    /// A method was called with an invalid argument. This is also returned from
    /// various initialize methods where data passed into a constructor was invalid.
    InvalidArgumentError = -8,

    /// Where a function requires a given ordering of inputs this is returned if the
    /// ordering requirement is violated.
    OutOfOrderError = -9,

    /// Error returned by platform-specific implementations of the network layer.
    NetworkSystemError = -10,

    /// Error returned when a resource is used before it is initialized.
    UninitializedError = -11,

    /// Error in the addressing of a resource.
    AddressError = -12,

    /// Error caused by a failure to connect to a remote resource.
    ConnectionError = -13,

    /// Operation was attempted on an entity that was not in a state to accept it.
    InvalidStateError = -14
};

enum class FlagsLayer : std::int8_t
{
    Unspecified  = 0U,
    Network      = 1U,
    Transport    = 2U,
    Presentation = 3U,
    Application  = 4U
};

/// Helper to determine if a given Status had a layer value set in the flags field.
/// @param layer            The layer value to query for.
/// @param status_flags     The flags value to query.
/// @return true if the first three bits in the status.flags field were equal to the value specified by the layer
/// enumeration value.
constexpr bool isLayerStatus(const FlagsLayer layer, const std::uint8_t status_flags)
{
    return (0x7U & status_flags) == static_cast<std::underlying_type<FlagsLayer>::type>(layer);
}

/// Helper to retrieve the non-layer bits of the status flags field.
/// @param status_flags The flags to retrieve the bits from.
/// @return The value of the flags field shifted down 3-bits.
constexpr std::uint8_t getFlags(const std::uint8_t status_flags)
{
    return (status_flags >> 3U);
}

/// Helper for forming a valid flags value for the Status type.
/// @param layer All status flags that have data should set the layer.
/// @param data  5-bits of data to back into the flags. All methods that set this data shall document the usage.
/// @return Packed value to store in a Status flags field.
constexpr std::uint8_t makeFlags(const FlagsLayer layer, const std::uint8_t data)
{
    static_assert(sizeof(std::underlying_type<FlagsLayer>::type) <= sizeof(std::uint8_t),
                  "FlagsLayer underlying type is too large on this platform.");
    return static_cast<std::uint8_t>(static_cast<std::underlying_type<FlagsLayer>::type>(layer) |
                                     (0xFF & (data << 3U)));
}

/// Simple status for use throughout libcyphal. This is an immutable type.
/// See `MutableStatus` for a mutable version.
class Status final
{
public:
    operator bool() const noexcept
    {
        return (result == ResultCode::Success || result == ResultCode::PartialSuccess);
    }

    bool succeededOrTimedOut() const noexcept
    {
        return (result == ResultCode::Timeout || static_cast<bool>(this));
    }

    const ResultCode result;

    /// Optional flags for a status. The first three bits are reserved for the FlagsLayer value. Each method that
    /// utilizes the last 5 bits must document the semantics.
    const std::uint8_t flags;

    /// The Status ID is an optional, unique ID used to identify a specific status. This is useful for internal errors
    /// where the program is not able to respond to the specific error but a programmer may need more information than,
    /// for example, a generic "internal error" result.
    /// As a convention; all ids shall appear as hexadecimal literals to support grep or find in the source code.
    /// Note that 0x00 is the same as "no id".
    /// Where flags is 0 this value must be unique within the libcyphal codebase. By setting flags the value can be
    /// interpreted differently.
    const std::uint16_t id;

    Status(ResultCode result, FlagsLayer flag_layer, std::uint8_t flag_data, std::uint16_t id) noexcept
        : result{result}
        , flags{makeFlags(flag_layer, flag_data)}
        , id{id}
    {
    }

    Status(ResultCode result, std::uint16_t id) noexcept
        : result{result}
        , flags{0}
        , id{id}
    {
    }

    Status(ResultCode result) noexcept
        : Status(result, 0)
    {
    }

    Status() noexcept
        : Status(ResultCode::Success, 0)
    {
    }

    Status(const Status& rhs) noexcept
        : result{rhs.result}
        , flags{rhs.flags}
        , id{rhs.id}
    {
    }
    Status(Status&& rhs) noexcept
        : result{rhs.result}
        , flags{rhs.flags}
        , id{rhs.id}
    {
    }

    ~Status() noexcept = default;

    Status& operator=(const Status& rhs) noexcept = delete;
    Status& operator=(Status&&) noexcept          = delete;

    operator ResultCode() const noexcept
    {
        return result;
    }

    bool operator==(const Status& other) const noexcept
    {
        return result == other.result && id == other.id;
    }

    bool operator==(const ResultCode& other_result) const noexcept
    {
        return result == other_result;
    }
};

///
/// Helper to build a result with an ID for a function with branching logic and a single return statement.
/// For example:
/// ```
/// Status doTheThing(ThingToDo action)
/// {
///     // 0x00 should never be returned
///     MutableStatus result{ResultCode::UnknownError, 0x00};
///     Status take_action_result = takeAction(action);
///     if (!take_action_result)
///     {
///         // 0x01 result is from the takeAction method.
///         result = MutableStatus{take_action_result.result, 0x01};
///     }
///     else
///     {
///         // 0x02 result is the result of the action itself.
///         result = MutableStatus{action.getResult(), 0x02};
///     }
///     // To boolean test a MutableStatus, first cast it to a Status. This
///     // results in the same assembly generated for the Status boolean
///     // operator at O1 and higher.
///     if (!static_cast<Status>(result))
///     {
///         // we're about to fail. Cleanup.
///         action.undoPartials();
///     }
///     // MutableStatus should never be the return value of a function but can
///     // be implicitly converted to a Status in a return statement.
///     return result;
///  }
/// ```
/// Note that, where Status identifiers are not used, it is adequate to use a ResultCode variable instead of this type.
///
struct MutableStatus final
{
    ResultCode    result;
    FlagsLayer    layer;
    std::uint8_t  flag_data;
    std::uint16_t id;

    operator Status() const noexcept
    {
        return Status(result, layer, flag_data, id);
    }
};

using NodeID                     = std::uint16_t;
constexpr NodeID anonymousNodeID = std::numeric_limits<NodeID>::max();
using PortID                     = std::uint16_t;
using TransferID                 = std::uint64_t;

///
/// libcyphal takes advantage of an assumption that a small number of redundant interfaces will be used for any single
/// transport. If this assumption is incorrect then the design of the library's internal will need to change since
/// changing this constant will increase the size of several data structures for all interface types and may cause
/// memory or performance issues. We do not recommend increasing this value beyond 4 although reducing it is acceptable
/// (as long as it is not reduced to 0).
constexpr std::size_t maxRedundantInterfaces = 3;

static_assert(maxRedundantInterfaces > 0, "maxRedundantInterfaces must be greater than 0");
static_assert(maxRedundantInterfaces <= 4,
              "maxRedundantInterfaces should be less than or equal to 4. See documentation for more details.");

/// Runnable objects do work asynchronously but only when run() is called. This allows super-loop firmware to separate
/// execution into application work and library work, and for threaded software applications to service the library from
/// dedicated threads.
/// Each object that implements IRunnable shall document how when it must be run to achieve certain functionality and
/// timing guarantees.
class IRunnable
{
public:
    /// Run the object for no more that max_run_duration. Implementations shall document the timing guarantees of this
    /// function including if they are soft or hard guarantees and the conditions that can lead to violations.
    /// @param max_run_duration The maximum amount of time to run the object for. It may exit early without error.
    /// @return The result of the operation.
    virtual Status runFor(std::chrono::microseconds max_run_duration) noexcept = 0;

    /// In multi-threaded environments this function may be used to cancel a running object.
    ///
    /// Shutting down runnable objects in a multi-threaded environment may look something like this:
    /// @TODO: code example
    ///     if thread.is_running():
    ///         runner->cancel();
    ///         thread.join();
    ///
    /// @return NotImplementedError if the object does not support cancellation, Success if the
    /// object was possibly running and is now cancelled, and ResourceClosedError if the object was
    /// not running.
    virtual Status cancel() noexcept = 0;

protected:
    virtual ~IRunnable() = default;
};

}  // namespace libcyphal

#endif  // LIBCYPHAL_HPP_INCLUDED
