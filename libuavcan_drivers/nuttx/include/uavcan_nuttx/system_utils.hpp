/*
 * Copyright (C) 2014 Pavel Kirienko <pavel.kirienko@gmail.com>
 */

#pragma once

#include <array>
#include <cassert>
#include <cctype>
#include <fstream>
#include <string>
#include <vector>
#include <cstdint>
#include <algorithm>
#include <utility>
#include <uavcan/data_type.hpp>

namespace uavcan_nuttx
{
/**
 * This class can find and read machine ID from a text file, represented as 32-char (16-byte) long hexadecimal string,
 * possibly with separators (like dashes or colons). If the available ID is more than 16 bytes, extra bytes will be
 * ignored. A shorter ID will not be accepted as valid.
 * In order to be read, the ID must be located on the first line of the file and must not contain any whitespace
 * characters.
 *
 * Examples of valid ID:
 *   0123456789abcdef0123456789abcdef
 *   20CE0b1E-8C03-07C8-13EC-00242C491652
 */
class MachineIDReader
{
public:
    static constexpr int MachineIDSize = 16;

    typedef std::array<std::uint8_t, MachineIDSize> MachineID;

    /**
     * This class can use extra seach locations. If provided, they will be checked first, before default ones.
     */
    MachineIDReader(const std::vector<std::string>& extra_search_locations = {})
    { }

    /**
     * Just like @ref readAndGetLocation(), but this one doesn't return location where this ID was obtained from.
     */
    MachineID read() const
    {
    	std::string idString = "0123456789abcde"; //FIXME nuttx uuid
    	std::array<uint8_t, MachineIDSize> idArray;
    	std::copy(idString.begin(), idString.end(), idArray.data());
    	return idArray;
    }
};



/**
 * This class computes unique ID for a UAVCAN node in a Linux application.
 * It takes the following inputs:
 *  - Unique machine ID
 *  - Node name string (e.g. "org.uavcan.linux_app.dynamic_node_id_server")
 *  - Instance ID byte, e.g. node ID (optional)
 */
inline std::array<std::uint8_t, 16> makeApplicationID(const MachineIDReader::MachineID& machine_id,
                                                      const std::string& node_name,
                                                      const std::uint8_t instance_id = 0)
{
    union HalfID
    {
        std::uint64_t num;
        std::uint8_t bytes[8];

        HalfID(std::uint64_t arg_num) : num(arg_num) { }
    };

    std::array<std::uint8_t, 16> out;

    // First 8 bytes of the application ID are CRC64 of the machine ID in native byte order
    {
        uavcan::DataTypeSignatureCRC crc;
        crc.add(machine_id.data(), static_cast<unsigned>(machine_id.size()));
        HalfID half(crc.get());
        std::copy_n(half.bytes, 8, out.begin());
    }

    // Last 8 bytes of the application ID are CRC64 of the node name and optionally node ID
    {
        uavcan::DataTypeSignatureCRC crc;
        crc.add(reinterpret_cast<const std::uint8_t*>(node_name.c_str()), static_cast<unsigned>(node_name.length()));
        crc.add(instance_id);
        HalfID half(crc.get());
        std::copy_n(half.bytes, 8, out.begin() + 8);
    }

    return out;
}

}
