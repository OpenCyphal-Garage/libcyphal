/// @copyright
/// Copyright (C) OpenCyphal Development Team  <opencyphal.org>
/// Copyright Amazon.com Inc. or its affiliates.
/// SPDX-License-Identifier: MIT

#ifndef LIBCYPHAL_CUSTOM_LIBCYPHAL_CONFIG_HPP
#define LIBCYPHAL_CUSTOM_LIBCYPHAL_CONFIG_HPP

namespace custom
{

// Forward declaration of the custom configuration struct.
struct MyConfig;

}  // namespace custom

// Overriding the default libcyphal configuration file with custom one.
// NOLINTNEXTLINE(cppcoreguidelines-macro-usage)
#define LIBCYPHAL_CONFIG ::custom::MyConfig
#include <libcyphal/config.hpp>

#include <cstddef>

namespace custom
{

struct MyConfig : libcyphal::Config
{
    /// Redefines time representation as 32-bit milliseconds.
    ///
    /// Milliseconds are chosen b/c there is no implicit conversion from native lizard's microseconds
    /// to lower precision units like milliseconds, so proper explicit `std::chrono::duration_cast` is needed.
    /// For details also see https://github.com/OpenCyphal-Garage/libcyphal/issues/431.
    ///
    struct MonotonicClock final
    {
        using rep        = std::int32_t;
        using period     = std::milli;
        using duration   = std::chrono::duration<rep, period>;
        using time_point = std::chrono::time_point<MonotonicClock>;

        static constexpr bool is_steady = true;

    };  // MonotonicClock

    struct Presentation : Config::Presentation
    {
        static constexpr std::size_t SmallPayloadSize()
        {
            // Default is `256` but for some unit tests we want just `6`.
            // For example, it will force serialization of `Heartbeat_1_0` message (7 bytes) to use PMR.
            return 6;
        }

    };  // Presentation

    struct Application : Config::Application
    {
        struct Node : Config::Application::Node
        {
            static constexpr std::size_t HeartbeatProducer_UpdateCallback_FunctionSize()  // NOSONAR cpp:S799
            {
                // Default is `4` but for our unit tests `2` is enough.
                return sizeof(void*) * 2;
            }

        };  // Node

    };  // Application

};  // MyConfig

}  // namespace custom

#endif  // LIBCYPHAL_CUSTOM_LIBCYPHAL_CONFIG_HPP
