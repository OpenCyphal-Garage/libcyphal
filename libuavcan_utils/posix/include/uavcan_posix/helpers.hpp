/*
 * Copyright (C) 2014 Pavel Kirienko <pavel.kirienko@gmail.com>
 * Copyright 2018 Amazon.com, Inc. or its affiliates. All Rights Reserved.
 */

#pragma once

#include <memory>
#include <string>
#include <vector>
#include <chrono>
#include <iostream>
#include <sstream>
#include <uavcan/uavcan.hpp>
#include <uavcan_posix/socketcan.hpp>

namespace uavcan_posix
{

// +--------------------------------------------------------------------------+
// | MACHINE ID HELPERS
// +--------------------------------------------------------------------------+

static constexpr int MachineIDSize = 16;

using MachineID = std::array<std::uint8_t, MachineIDSize>;

/**
 * This method computes a unique ID for a UAVCAN node.
 * @param node_name     Node name string (e.g. "org.uavcan.linux_app.dynamic_node_id_server")
 * @param instance_id   Instance ID byte, e.g. node ID (optional)
 */
template <typename MachineIDReader> MachineID makeApplicationID(const std::string& node_name,
                                                                const std::uint8_t instance_id = 0)
{
    const auto& machine_id = MachineIDReader().read();

    union HalfID
    {
        std::uint64_t num;
        std::uint8_t bytes[8];

        HalfID(std::uint64_t arg_num) : num(arg_num) { }
    };

    MachineID out;

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

// +--------------------------------------------------------------------------+
// | LOGGING HELPERS
// +--------------------------------------------------------------------------+
/**
 * Default log sink will dump everything into stderr.
 * It is installed by default.
 */
class DefaultLogSink : public uavcan::ILogSink
{
    void log(const uavcan::protocol::debug::LogMessage& message) override
    {
        const auto tt = std::chrono::system_clock::to_time_t(std::chrono::system_clock::now());
        const auto tstr = std::ctime(&tt);
        std::cerr << "### UAVCAN " << tstr << message << std::endl;
    }
};

// +--------------------------------------------------------------------------+
// | SERVICE CLIENT HELPERS
// +--------------------------------------------------------------------------+
/**
 * Wrapper over uavcan::ServiceClient<> for blocking calls.
 * Blocks on uavcan::Node::spin() internally until the call is complete.
 */
template <typename DataType>
class BlockingServiceClient : public uavcan::ServiceClient<DataType>
{
    typedef uavcan::ServiceClient<DataType> Super;

    typename DataType::Response response_;
    bool call_was_successful_;

    void callback(const uavcan::ServiceCallResult<DataType>& res)
    {
        response_ = res.getResponse();
        call_was_successful_ = res.isSuccessful();
    }

    void setup()
    {
        Super::setCallback(std::bind(&BlockingServiceClient::callback, this, std::placeholders::_1));
        call_was_successful_ = false;
        response_ = typename DataType::Response();
    }

public:
    BlockingServiceClient(uavcan::INode& node)
        : uavcan::ServiceClient<DataType>(node)
        , call_was_successful_(false)
    {
        setup();
    }

    /**
     * Performs a blocking service call using default timeout (see the specs).
     * Use @ref getResponse() to get the actual response.
     * Returns negative error code.
     */
    int blockingCall(uavcan::NodeID server_node_id, const typename DataType::Request& request)
    {
        return blockingCall(server_node_id, request, Super::getDefaultRequestTimeout());
    }

    /**
     * Performs a blocking service call using the specified timeout. Please consider using default timeout instead.
     * Use @ref getResponse() to get the actual response.
     * Returns negative error code.
     */
    int blockingCall(uavcan::NodeID server_node_id, const typename DataType::Request& request,
                     uavcan::MonotonicDuration timeout)
    {
        const auto SpinDuration = uavcan::MonotonicDuration::fromMSec(2);
        setup();
        Super::setRequestTimeout(timeout);
        const int call_res = Super::call(server_node_id, request);
        if (call_res >= 0)
        {
            while (Super::hasPendingCalls())
            {
                const int spin_res = Super::getNode().spin(SpinDuration);
                if (spin_res < 0)
                {
                    return spin_res;
                }
            }
        }
        return call_res;
    }

    /**
     * Whether the last blocking call was successful.
     */
    bool wasSuccessful() const { return call_was_successful_; }

    /**
     * Use this to retrieve the response of the last blocking service call.
     * This method returns default constructed response object if the last service call was unsuccessful.
     */
    const typename DataType::Response& getResponse() const { return response_; }
};

} // namespace uavcan_posix
