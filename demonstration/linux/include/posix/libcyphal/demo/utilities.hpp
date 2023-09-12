/// @copyright Copyright 2023 Amazon.com Inc. and its affiliates. All Rights Reserved.
/// @file
/// Utility functions for demo apps

#ifndef DEMO_UTILITIES_HPP
#define DEMO_UTILITIES_HPP

#include <atomic>
#include <errno.h>
#include <signal.h>
#include <stdio.h>
#include <time.h>

namespace demo
{

constexpr long kOneSecondInNanoseconds = 1000000000L;

/// @brief Support clean shutdown via ctrl-c
static std::atomic<bool> continue_running(true);
static void sigint_handler(int signal) {
    printf("Attempting to terminate gracefully. Try CTRL+\\ if unsuccessful.\n");
    (void) signal;
    continue_running = false;
}

/// @brief Sleeps for specified amount of time
/// @param[in] kSleepTimeNs Amount of time to sleep in nanoseconds
static void high_resolution_sleep(long kSleepTimeNs) {
    struct timespec now {};
    struct timespec remain {};

    clock_gettime(CLOCK_MONOTONIC, &now);
    long new_ns_value = (now.tv_sec * kOneSecondInNanoseconds + now.tv_nsec) + kSleepTimeNs;
    now.tv_nsec = new_ns_value % kOneSecondInNanoseconds;
    now.tv_sec = new_ns_value / kOneSecondInNanoseconds;
    bool done = false;
    int return_value = 0;
    while (!done) {
        return_value = clock_nanosleep(CLOCK_MONOTONIC, TIMER_ABSTIME, &now, &remain);
        done = (return_value == 0 || return_value != EINTR);
    }
}

/// @brief This allows the user to not have to cast to the real type of the enum
///        when using it's underlying type as in a printf or an assignment.
/// @param[in] E object who's value is desired in the underlying data type
/// @return value of input with the underlying data type
template <typename E>
constexpr typename std::underlying_type<E>::type to_underlying(E e) noexcept {
    static_assert(std::is_enum<E>::value, "Must be an enumeration type!");
    return static_cast<typename std::underlying_type<E>::type>(e);
}

}  // namespace demo

#endif  // DEMO_UTILITIES_HPP