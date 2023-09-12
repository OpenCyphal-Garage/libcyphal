/// @copyright Copyright Amazon.com Inc. and its affiliates. All Rights Reserved.
/// @file
/// Defines the Status type used throughout libcyphal for function status. Provides ResultCode and CauseCode if desired.

#ifndef LIBCYPHAL_TYPES_STATUS_HPP_INCLUDED
#define LIBCYPHAL_TYPES_STATUS_HPP_INCLUDED

#include <cstdint>
#include "libcyphal/types/common.hpp"

namespace libcyphal
{

using StatusType = std::int_fast8_t;
using ArdStatus  = std::int32_t;

/// @brief List of known results
/// @todo Replace with something more generic and accepted by the open source community
enum class ResultCode : EnumType
{
    ///  Values greater than 0 are success values for libcyphal results.
    Success = 1,

    /// Some parts of a non-atomic operation completed successfully but other parts failed.
    /// This result will only be used where additional information about the failure parts
    /// can allow the caller to recover.
    SuccessPartial = 2,

    /// The operation didn't do anything but no failures occurred. For example, this would be
    /// returned for a read operation that read nothing successfully.
    SuccessNothing = 3,

    /// No errors occurred but the operation did not complete because a timeout period was reached.
    SuccessTimeout = 4,

    /// An operation failed because a buffer was full. For some operations this implies
    /// that trying again with the same input can be successful.
    BufferFull = 0,

    /// A generic failure.
    Failure = -1,

    /// One or more parameters provided to a function were invalid.
    BadArgument = -2,

    /// An operation failed because there was inadequate memory available.
    OutOfMemory = -3,

    /// A lookup failed to find anything for the given search parameters.
    NotFound = -4,

    /// The operation failed because it was not implemented.
    NotImplemented = -5,

    /// request/response/broadcast not ready because there is a lack of publication records for the request
    NotInitialized = -6,

    /// records not udpated, but could be later
    NotReady = -7,

    /// Resource being used
    Busy = -8,

    /// Invalid state, such as registration being closed
    NotAllowed = -9,

    /// Not enough resources, for example subscription records
    NotEnough = -10,

    /// Resource not available
    NotAvailable = -11,

    /// Resource being accessed is not configured for use.
    /// This implies a configuration does exist that would make the access valid.
    NotConfigured = -12,

    /// Invalid state/parameters
    Invalid = -13,

    /// Failed to Bind to socket
    FailedToBindToSocket = -14,

    /// Receiver failed to join multicast group
    FailedToJoinMulticastGroup = -15,

    /// The operation experienced an internal inconsistency or an unexpected
    /// result from a lower layer.
    UnknownInternalError = -16,

};

/// @brief holds the result code
/// @todo Replace with something more generic and accepted by the open source community
union ResultCodeType
{
    /// @brief constructor that sets result to Success
    constexpr ResultCodeType() noexcept
        : raw{0} {};
    /// @brief constructor that sets result to given ResultCode
    /// @param[in] result ResultCode to set
    constexpr ResultCodeType(ResultCode result) noexcept
        : code{result} {};
    /// @brief constructor that sets result to a given generic status
    /// @param[in] result generic status code to set
    constexpr ResultCodeType(StatusType result) noexcept
        : raw{result} {};
    /// @brief ResultCode Copy constructor
    /// @param[in] other ResultCode to copy from
    constexpr ResultCodeType(const ResultCodeType& other) noexcept
        : ResultCodeType(other.raw)
    {
    }
    /// @brief ResultCode Copy assignment
    /// @param[in] other ResultCode to copy from
    constexpr ResultCodeType& operator=(const ResultCodeType& other) noexcept
    {
        raw = other.raw;
        return (*this);
    };
    ResultCode code;
    StatusType raw;
};

/// @brief List of known causes
/// @todo Replace with something more generic and accepted by the open source community
enum class CauseCode : EnumType
{
    NoCause            = 0,
    Session            = 1,
    Resource           = 2,
    FiniteStateMachine = 3,
    Parameter          = 4,
    Unknown
};

/// @brief holds the cause code
/// @todo Replace with something more generic and accepted by the open source community
union CauseCodeType
{
    /// @brief constructor that sets cause to no cause
    constexpr CauseCodeType() noexcept
        : raw{0} {};
    /// @brief constructor that sets cause to given CauseCode
    /// @param[in] result CauseCode to set
    constexpr CauseCodeType(CauseCode cause) noexcept
        : code{cause} {};
    /// @brief constructor that sets cause to a given generic cause
    /// @param[in] result generic cause to set
    constexpr CauseCodeType(StatusType cause) noexcept
        : raw{cause} {};
    /// @brief CauseCode Copy constructor
    /// @param[in] other CauseCode to copy from
    constexpr CauseCodeType(const CauseCodeType& other) noexcept
        : CauseCodeType(other.raw)
    {
    }
    /// @brief CauseCode Copy assignment
    /// @param[in] other CauseCode to copy from
    constexpr CauseCodeType& operator=(const CauseCodeType& other) noexcept
    {
        raw = other.raw;
        return (*this);
    };
    CauseCode  code;
    StatusType raw;
};

/// @brief Simple status for use throughout libcyphal
/// @todo Replace with something more generic and accepted by the open source community
struct Status
{
    /// @brief constructor sets Status to Success
    constexpr Status() noexcept
        : Status{ResultCode::Success}
    {
    }
    /// @brief constructor from a generic integer status
    constexpr Status(StatusType status) noexcept
        : result{status}
        , cause{CauseCode::NoCause}
    {
    }
    /// @brief constructor from a result code
    constexpr Status(ResultCode status) noexcept
        : Status(status, CauseCode::NoCause)
    {
    }
    /// @brief constructor from a result code with a cause code
    constexpr Status(ResultCode status, CauseCode cause) noexcept
        : result{static_cast<StatusType>(status)}
        , cause{static_cast<StatusType>(cause)}
    {
    }
    /// @brief constructor from a generic integer status and generic cause code
    constexpr Status(StatusType status, StatusType cause_code) noexcept
        : result{status}
        , cause{cause_code}
    {
    }

    /// @brief Status copy constructor
    /// @param[in] status Status to copy from
    constexpr Status(const Status& status) noexcept
    {
        result = status.result;
        cause  = status.cause;
    }
    /// @brief Status Copy assignemnt
    /// @param other Status to copy from
    /// @return updated Status
    constexpr Status& operator=(const Status& other) noexcept
    {
        result = other.result;
        cause  = other.cause;
        return (*this);
    };
    /// @brief Able to set Status to a ResultCode
    /// @param other Result Code to copy from
    /// @return updated Status
    constexpr Status& operator=(const ResultCode& other) noexcept
    {
        result = static_cast<ResultCodeType>(other);
        cause  = static_cast<CauseCodeType>(CauseCode::NoCause);
        return (*this);
    };
    /// @brief Bias to if any failure, return failure
    /// @param other Status to compare against
    /// @return updated Status
    constexpr Status& operator+=(const ResultCode& other) noexcept
    {
        if ((other != ResultCode::Success) || (result.code != ResultCode::Success))
        {
            result = ResultCode::Failure;
        }
        return (*this);
    };
    /// @brief Bias to if any failure, return failure
    /// @param other Status to compare against
    /// @return updated Status
    constexpr Status& operator+=(const Status& other) noexcept
    {
        if ((other.result.code != ResultCode::Success) || (result.code != ResultCode::Success))
        {
            result = ResultCode::Failure;
        }
        return (*this);
    };
    /// Can't have this if we want to constexpr
    // ~Status() = default;

    /// @brief Checks if current result is a success
    /// @return if Status ResultCode is successful
    constexpr bool isSuccess() const noexcept
    {
        return static_cast<EnumType>(result.code) > 0;
    }
    /// @brief Checks if current result is a failure
    /// @return if Status ResultCode is not successful
    constexpr bool isFailure() const noexcept
    {
        return result.code < ResultCode::Success;
    }
    /// @brief Returns result code
    /// @return current result code
    constexpr ResultCode getResultCode() const noexcept
    {
        return (result.code <= ResultCode::UnknownInternalError) ? ResultCode::UnknownInternalError : result.code;
    }
    /// @brief Returns cause code
    /// @return current cause code
    constexpr CauseCode getCauseCode() const noexcept
    {
        return ((cause.code >= CauseCode::Unknown) || (cause.code < CauseCode::NoCause)) ? CauseCode::Unknown
                                                                                         : cause.code;
    }
    /// @brief Sets the current result
    /// @param[in] code ResultCode to set current result to
    constexpr void setResult(const ResultCode code) noexcept
    {
        result.code = code;
    }
    /// @brief Sets the current result and cause
    /// @param[in] result_code ResultCode to set current result to
    /// @param[in] cause_code CauseCode to set current cause to
    constexpr void setResultAndCause(const ResultCode result_code, const CauseCode cause_code) noexcept
    {
        result.code = result_code;
        cause.code  = cause_code;
    }
    ResultCodeType result;
    CauseCodeType  cause;
};

/// @brief Converts status received from libudpard into a libcypahl Status
/// @param[in] result libudpard status received
/// @return Success or Failure Status class
inline Status ardStatusToCyphalStatus(const ArdStatus result) noexcept
{
    return Status((result < 0) ? ResultCode::Failure : ResultCode::Success);
}

}  // namespace libcyphal

#endif  // LIBCYPHAL_TYPES_STATUS_HPP_INCLUDED
