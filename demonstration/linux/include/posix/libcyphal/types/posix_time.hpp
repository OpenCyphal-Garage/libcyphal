/// @copyright Copyright Amazon.com Inc. and its affiliates. All Rights Reserved.
/// @file
/// Posix timer implementation using std:chrono

#ifndef POSIX_LIBCYPHAL_TYPES_POSIX_TIMER_HPP
#define POSIX_LIBCYPHAL_TYPES_POSIX_TIMER_HPP

#include <chrono>
#include <cstdint>
#include <libcyphal/types/time.hpp>

namespace libcyphal
{
namespace time
{

/// @brief Posix timer using "chrono" library
class PosixTimer final : public Timer
{
public:
    /// @brief Constructor using high resolutino clock
    explicit PosixTimer() noexcept
        : start_time_{std::chrono::high_resolution_clock::now()}
    {
    }

    /// @brief Destructor to cleanup PosixTimer before exiting
    ~PosixTimer() noexcept = default;

    /// @brief Retrieves the monotonic time in micro seconds
    /// @return Monotonic time in microseconds
    Monotonic getTimeInUs() const noexcept override
    {
        std::chrono::high_resolution_clock::time_point current_time = std::chrono::high_resolution_clock::now();
        auto duration = std::chrono::duration_cast<std::chrono::milliseconds>(current_time - start_time_).count();
        return Monotonic::fromMicrosecond(static_cast<Monotonic::MicrosecondType>(duration));
    }

private:
    const std::chrono::high_resolution_clock::time_point start_time_;
};

}  // namespace time
}  // namespace libcyphal

#endif  // POSIX_LIBCYPHAL_TYPES_POSIX_TIMER_HPP
