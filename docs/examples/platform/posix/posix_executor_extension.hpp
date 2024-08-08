/// @copyright
/// Copyright (C) OpenCyphal Development Team  <opencyphal.org>
/// Copyright Amazon.com Inc. or its affiliates.
/// SPDX-License-Identifier: MIT
///

#ifndef EXAMPLE_PLATFORM_POSIX_EXECUTOR_HPP_INCLUDED
#define EXAMPLE_PLATFORM_POSIX_EXECUTOR_HPP_INCLUDED

#include <cetl/pf17/cetlpf.hpp>
#include <cetl/rtti.hpp>
#include <libcyphal/executor.hpp>

namespace example
{
namespace platform
{
namespace posix
{

class IPosixExecutorExtension
{
    // FFE3771E-7962-4CEA-ACA6-ED7895699080
    using IPosixExecutorExtensionTypeIdType = cetl::
        // NOLINTNEXTLINE(cppcoreguidelines-avoid-magic-numbers, readability-magic-numbers)
        type_id_type<0xFF, 0xE3, 0x77, 0x1E, 0x79, 0x62, 0x4C, 0xEA, 0xAC, 0xA6, 0xED, 0x78, 0x95, 0x69, 0x90, 0x80>;

public:
    IPosixExecutorExtension() = default;

    IPosixExecutorExtension(const IPosixExecutorExtension&)                = delete;
    IPosixExecutorExtension(IPosixExecutorExtension&&) noexcept            = delete;
    IPosixExecutorExtension& operator=(const IPosixExecutorExtension&)     = delete;
    IPosixExecutorExtension& operator=(IPosixExecutorExtension&&) noexcept = delete;

    struct WhenCondition
    {
        struct HandleReadable
        {
            int fd;
        };
        struct HandleWritable
        {
            int fd;
        };

        using Variant = cetl::variant<HandleReadable, HandleWritable>;
    };
    virtual bool scheduleCallbackWhen(libcyphal::IExecutor::Callback::Any& callback,
                                      const WhenCondition::Variant&        condition) = 0;

    // MARK: - cetl::rtti

    static constexpr cetl::type_id _get_type_id_() noexcept
    {
        return cetl::type_id_type_value<IPosixExecutorExtensionTypeIdType>();
    }

protected:
    ~IPosixExecutorExtension() = default;

};  // IPosixExecutorExtension

}  // namespace posix
}  // namespace platform
}  // namespace example

#endif  // EXAMPLE_PLATFORM_POSIX_EXECUTOR_HPP_INCLUDED
