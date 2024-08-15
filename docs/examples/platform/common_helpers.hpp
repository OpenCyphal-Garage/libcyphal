/// @copyright
/// Copyright (C) OpenCyphal Development Team  <opencyphal.org>
/// Copyright Amazon.com Inc. or its affiliates.
/// SPDX-License-Identifier: MIT

#ifndef EXAMPLE_PLATFORM_COMMON_HELPERS_HPP_INCLUDED
#define EXAMPLE_PLATFORM_COMMON_HELPERS_HPP_INCLUDED

#include <libcyphal/transport/errors.hpp>
#include <libcyphal/types.hpp>

#include <cetl/pf17/cetlpf.hpp>

#include <gmock/gmock.h>
#include <gtest/gtest.h>

#include <cstdint>
#include <cstdlib>
#include <sstream>
#include <string>
#include <vector>

namespace example
{
namespace platform
{

struct CommonHelpers
{
    static std::vector<std::string> splitInterfaceAddresses(const std::string& iface_addresses_str)
    {
        std::vector<std::string> iface_addresses;
        std::istringstream       iss(iface_addresses_str);
        std::string              str;
        while (std::getline(iss, str, ' '))
        {
            iface_addresses.push_back(str);
        }
        return iface_addresses;
    }

    struct Printers
    {
        static std::string describeError(const libcyphal::transport::StateError&)
        {
            return "StateError";
        }
        static std::string describeError(const libcyphal::transport::AnonymousError&)
        {
            return "AnonymousError";
        }
        static std::string describeError(const libcyphal::transport::ArgumentError&)
        {
            return "ArgumentError";
        }
        static std::string describeError(const libcyphal::transport::MemoryError&)
        {
            return "MemoryError";
        }
        static std::string describeError(const libcyphal::transport::CapacityError&)
        {
            return "CapacityError";
        }
        static std::string describeError(const libcyphal::transport::AlreadyExistsError&)
        {
            return "AlreadyExistsError";
        }
        static std::string describeError(const libcyphal::transport::PlatformError& error)
        {
            const auto code = error->code();
            return "Failure: PlatformError{code=" + std::to_string(code) + ", msg='" +
                   std::strerror(static_cast<int>(code)) + "'}";
        }

        static std::string describeAnyFailure(const libcyphal::transport::AnyFailure& failure)
        {
            return "Failure: " + cetl::visit([](const auto& error) { return describeError(error); }, failure);
        }
    };

    template <typename Executor>
    static void runMainLoop(Executor&                                        executor,
                            const libcyphal::TimePoint                       deadline,
                            const std::function<void(libcyphal::TimePoint)>& spin_extra_action)
    {
        libcyphal::Duration worst_lateness{0};

        while (executor.now() < deadline)
        {
            const auto spin_result = executor.spinOnce();
            worst_lateness         = std::max(worst_lateness, spin_result.worst_lateness);

            spin_extra_action(spin_result.approx_now);

            cetl::optional<libcyphal::Duration> opt_timeout;
            if (spin_result.next_exec_time.has_value())
            {
                opt_timeout = spin_result.next_exec_time.value() - executor.now();
            }
            EXPECT_THAT(executor.pollAwaitableResourcesFor(opt_timeout), testing::Eq(cetl::nullopt));
        }

        std::cout << "worst_callback_lateness=" << worst_lateness.count() << "us\n";
    }

};  // CommonHelpers

}  // namespace platform
}  // namespace example

#endif  // EXAMPLE_PLATFORM_TRACKING_MEMORY_RESOURCE_HPP_INCLUDED
