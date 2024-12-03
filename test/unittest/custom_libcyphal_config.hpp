/// @copyright
/// Copyright (C) OpenCyphal Development Team  <opencyphal.org>
/// Copyright Amazon.com Inc. or its affiliates.
/// SPDX-License-Identifier: MIT

#ifndef LIBCYPHAL_CUSTOM_LIBCYPHAL_CONFIG_HPP
#define LIBCYPHAL_CUSTOM_LIBCYPHAL_CONFIG_HPP

namespace custom
{

// Forward declaration of the custom configuration class.
class MyConfig;

}  // namespace custom

// Overriding the default libcyphal configuration file with custom one.
// NOLINTNEXTLINE(cppcoreguidelines-macro-usage)
#define LIBCYPHAL_CONFIG ::custom::MyConfig
#include <libcyphal/config.hpp>

#include <cstddef>

namespace custom
{

class MyConfig final : public libcyphal::Config<MyConfig>
{
    using ConfigBase = Config;

public:
    class Presentation final : public ConfigBase::Presentation
    {
    protected:
        friend ConfigBase::Presentation;

        static constexpr std::size_t SmallPayloadSize_Impl()
        {
            // Default is `256` but for some unit tests we want just `6`.
            // For example, it will force serialization of `Heartbeat_1_0` message (7 bytes) to use PMR.
            return 6;
        }

    };  // Presentation

    class Application final : public ConfigBase::Application
    {
        using ApplicationBase = ConfigBase::Application;

    public:
        class Node final : public ApplicationBase::Node
        {
        protected:
            friend ApplicationBase::Node;

            static constexpr std::size_t HeartbeatProducer_UpdateCallback_FunctionSize_Impl()  // NOSONAR cpp:S799
            {
                // Default is `4` but for our unit tests `2` is enough.
                return sizeof(void*) * 2;
            }

        };  // Node

    };  // Application

};  // MyConfig

}  // namespace custom

#endif  // LIBCYPHAL_CUSTOM_LIBCYPHAL_CONFIG_HPP
