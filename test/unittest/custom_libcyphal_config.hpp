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
