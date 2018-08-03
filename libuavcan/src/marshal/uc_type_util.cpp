/*
 * Copyright (C) 2014 Pavel Kirienko <pavel.kirienko@gmail.com>
 */

#include <uavcan/marshal/type_util.hpp>
#include <array>

using ByteLenToByteLenWithPaddingForRuntime = uavcan::ByteLenToByteLenWithPaddingImpl<uavcan::CanBusType::max_frame_size, true>;

namespace uavcan
{

namespace {
    // Provide static storage for the constexpr array so we can use it at runtime.
    static const auto g_length_to_max_data_size = CanBusType::payload_length_to_frame_length;
}

unsigned calculatePaddingBytes(size_t payload_length)
{
    if (payload_length > ByteLenToByteLenWithPaddingForRuntime::max_data_in_frames)
    {
        const unsigned last_frame_bytes = static_cast<unsigned>(payload_length - ByteLenToByteLenWithPaddingForRuntime::amount_of_data_in_first_frame) % ByteLenToByteLenWithPaddingForRuntime::max_data_in_frames;
        return static_cast<unsigned>((g_length_to_max_data_size[last_frame_bytes] - static_cast<uint8_t>(last_frame_bytes)));
    }
    else
    {
        return 0;
    }
}

}
