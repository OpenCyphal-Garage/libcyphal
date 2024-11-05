/// @copyright
/// Copyright (C) OpenCyphal Development Team  <opencyphal.org>
/// Copyright Amazon.com Inc. or its affiliates.
/// SPDX-License-Identifier: MIT

#ifndef EXAMPLE_PLATFORM_STORAGE_HPP_INCLUDED
#define EXAMPLE_PLATFORM_STORAGE_HPP_INCLUDED

#include <cetl/pf17/cetlpf.hpp>
#include <cetl/pf20/cetlpf.hpp>
#include <libcyphal/platform/storage.hpp>
#include <libcyphal/types.hpp>

#include <cerrno>
#include <cstddef>
#include <fstream>
#include <iostream>
#include <stdio.h>
#include <string>
#include <sys/stat.h>
#include <utility>

namespace example
{
namespace platform
{
namespace storage
{

class KeyValue final : public libcyphal::platform::storage::IKeyValue
{
public:
    explicit KeyValue(std::string root_path)
        : root_path_{std::move(root_path)}
    {
        if ((mkdir(root_path_.c_str(), 0755) != 0) && (errno != EEXIST))
        {
            std::cerr << "Error making folder: '" << root_path_ << "'.\n";
            std::cerr << "Error: " << std::strerror(errno) << std::endl;
        }
    }

private:
    using Error = libcyphal::platform::storage::Error;

    /// In practice, the keys could be hashed, so it won't be necessary to deal with directory nesting.
    /// This is fine b/c we don't need key listing, and so we don't have to retain the key names.
    ///
    std::string makeFilePath(const cetl::string_view key) const
    {
        return root_path_ + "/" + std::string{key.cbegin(), key.cend()};
    }

    // MARK: - libcyphal::platform::storage::IKeyValue

    auto get(const cetl::string_view        key,
             const cetl::span<std::uint8_t> data) const -> libcyphal::Expected<std::size_t, Error> override
    {
        const auto    file_path = makeFilePath(key);
        std::ifstream file{file_path, std::ios::in | std::ios::binary | std::ios::ate};
        if (!file)
        {
            return Error::Existence;
        }

        const auto data_size =
            std::min(static_cast<std::streamsize>(file.tellg()), static_cast<std::streamsize>(data.size()));
        file.seekg(0);
        file.read(reinterpret_cast<char*>(data.data()), data_size);
        if (!file)
        {
            std::cerr << "Error reading file: '" << file_path << "'.\n";
            std::cerr << "Error: " << std::strerror(errno) << std::endl;
            return Error::IO;
        }

        return static_cast<std::size_t>(file.gcount());
    }

    auto put(const cetl::string_view key, const cetl::span<const std::uint8_t> data)  //
        -> cetl::optional<Error> override
    {
        const auto    file_path = makeFilePath(key);
        std::ofstream file{file_path, std::ios::out | std::ios::binary};
        if (!file)
        {
            std::cerr << "Error opening file: '" << file_path << "'.\n";
            std::cerr << "Error: " << std::strerror(errno) << std::endl;
            return Error::IO;
        }

        file.write(reinterpret_cast<const char*>(data.data()), static_cast<std::streamsize>(data.size()));
        if (!file)
        {
            std::cerr << "Error writing to file: '" << file_path << "'.\n";
            std::cerr << "Error: " << std::strerror(errno) << std::endl;
            return Error::IO;
        }

        return cetl::nullopt;
    }

    auto drop(const cetl::string_view key) -> cetl::optional<Error> override
    {
        const auto file_path = makeFilePath(key);
        if ((std::remove(file_path.c_str()) != 0) && (errno != ENOENT))
        {
            std::cerr << "Error removing file: '" << file_path << "'.\n";
            std::cerr << "Error: " << std::strerror(errno) << std::endl;
            return Error::IO;
        }
        return cetl::nullopt;
    }

    const std::string root_path_;

};  // KeyValue

}  // namespace storage
}  // namespace platform
}  // namespace example

#endif  // EXAMPLE_PLATFORM_STORAGE_HPP_INCLUDED
