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
#include <uavcan/node/sub_node.hpp>
#include <uavcan_posix/socketcan.hpp>

namespace uavcan_posix
{

/**
 * Contains all drivers needed for uavcan::Node.
 */
template<typename ClockType> struct DriverPack
{
    ClockType clock;
    std::unique_ptr<uavcan::ICanDriver> can;

    explicit DriverPack(std::unique_ptr<uavcan::ICanDriver>&& can_driver)
        : clock()
        , can(std::move(can_driver))
    { }

    explicit DriverPack(const std::vector<std::string>& iface_names)
        : clock()
    {
        std::unique_ptr<SocketCanDriver> socketcan(new SocketCanDriver(clock));
        for (auto ifn : iface_names)
        {
            if (socketcan->addIface(ifn) < 0)
            {
                throw Exception("Failed to add iface " + ifn);
            }
        }
        can = std::move(socketcan);
    }
};

template <typename ClockType>
using DriverPackPtr = std::unique_ptr<DriverPack<ClockType>>;

using TimerPtr = std::unique_ptr<uavcan::Timer>;

template <typename T>
using SubscriberPtr = std::unique_ptr<uavcan::Subscriber<T>>;

template <typename T>
using PublisherPtr = std::unique_ptr<uavcan::Publisher<T>>;

template <typename T>
using ServiceServerPtr = std::unique_ptr<uavcan::ServiceServer<T>>;

template <typename T>
using ServiceClientPtr = std::unique_ptr<uavcan::ServiceClient<T>>;

template <typename T>
using BlockingServiceClientPtr = std::unique_ptr<BlockingServiceClient<T>>;

static constexpr std::size_t NodeMemPoolSize = 1024 * 512;  ///< This shall be enough for any possible use case

/**
 * Generic wrapper for node objects with some additional convenience functions.
 */
template <typename NodeType, typename ClockType>
class NodeBase : public NodeType
{
protected:
    DriverPackPtr<ClockType> driver_pack_;

    static void enforce(int error, const std::string& msg)
    {
        if (error < 0)
        {
            std::ostringstream os;
            os << msg << " [" << error << "]";
            throw Exception(os.str());
        }
    }

    template <typename DataType>
    static std::string getDataTypeName()
    {
        return DataType::getDataTypeFullName();
    }

public:
    /**
     * Simple forwarding constructor, compatible with uavcan::Node.
     */
    NodeBase(uavcan::ICanDriver& can_driver, uavcan::ISystemClock& clock) :
        NodeType(can_driver, clock)
    { }

    /**
     * Takes ownership of the driver container.
     */
    explicit NodeBase(DriverPackPtr<ClockType> driver_pack)
        : NodeType(*driver_pack->can, driver_pack->clock)
        , driver_pack_(std::move(driver_pack))
    { }

    /**
     * Allocates @ref uavcan::Subscriber in the heap.
     * The subscriber will be started immediately.
     * @throws Exception.
     */
    template <typename DataType>
    SubscriberPtr<DataType> makeSubscriber(const typename uavcan::Subscriber<DataType>::Callback& cb)
    {
        SubscriberPtr<DataType> p(new uavcan::Subscriber<DataType>(*this));
        enforce(p->start(cb), "Subscriber start failure " + getDataTypeName<DataType>());
        return p;
    }

    /**
     * Allocates @ref uavcan::Publisher in the heap.
     * The publisher will be initialized immediately.
     * @throws Exception.
     */
    template <typename DataType>
    PublisherPtr<DataType> makePublisher(uavcan::MonotonicDuration tx_timeout =
                                             uavcan::Publisher<DataType>::getDefaultTxTimeout())
    {
        PublisherPtr<DataType> p(new uavcan::Publisher<DataType>(*this));
        enforce(p->init(), "Publisher init failure " + getDataTypeName<DataType>());
        p->setTxTimeout(tx_timeout);
        return p;
    }

    /**
     * Allocates @ref uavcan::ServiceServer in the heap.
     * The server will be started immediately.
     * @throws Exception.
     */
    template <typename DataType>
    ServiceServerPtr<DataType> makeServiceServer(const typename uavcan::ServiceServer<DataType>::Callback& cb)
    {
        ServiceServerPtr<DataType> p(new uavcan::ServiceServer<DataType>(*this));
        enforce(p->start(cb), "ServiceServer start failure " + getDataTypeName<DataType>());
        return p;
    }

    /**
     * Allocates @ref uavcan::ServiceClient in the heap.
     * The service client will be initialized immediately.
     * @throws Exception.
     */
    template <typename DataType>
    ServiceClientPtr<DataType> makeServiceClient(const typename uavcan::ServiceClient<DataType>::Callback& cb)
    {
        ServiceClientPtr<DataType> p(new uavcan::ServiceClient<DataType>(*this));
        enforce(p->init(), "ServiceClient init failure " + getDataTypeName<DataType>());
        p->setCallback(cb);
        return p;
    }

    /**
     * Allocates @ref uavcan_posix::BlockingServiceClient in the heap.
     * The service client will be initialized immediately.
     * @throws Exception.
     */
    template <typename DataType>
    BlockingServiceClientPtr<DataType> makeBlockingServiceClient()
    {
        BlockingServiceClientPtr<DataType> p(new BlockingServiceClient<DataType>(*this));
        enforce(p->init(), "BlockingServiceClient init failure " + getDataTypeName<DataType>());
        return p;
    }

    /**
     * Allocates @ref uavcan::Timer in the heap.
     * The timer will be started immediately in one-shot mode.
     */
    TimerPtr makeTimer(uavcan::MonotonicTime deadline, const typename uavcan::Timer::Callback& cb)
    {
        TimerPtr p(new uavcan::Timer(*this));
        p->setCallback(cb);
        p->startOneShotWithDeadline(deadline);
        return p;
    }

    /**
     * Allocates @ref uavcan::Timer in the heap.
     * The timer will be started immediately in periodic mode.
     */
    TimerPtr makeTimer(uavcan::MonotonicDuration period, const typename uavcan::Timer::Callback& cb)
    {
        TimerPtr p(new uavcan::Timer(*this));
        p->setCallback(cb);
        p->startPeriodic(period);
        return p;
    }

};

/**
 * Wrapper for uavcan::Node with some additional convenience functions.
 * Note that this wrapper adds stderr log sink to @ref uavcan::Logger, which can be removed if needed.
 * Use one of the Node::make factory methods to instantiate.
 */
template<typename ClockType> class Node : public NodeBase<uavcan::Node<NodeMemPoolSize>, ClockType>
{
    using Base = NodeBase<uavcan::Node<NodeMemPoolSize>, ClockType>;

    DefaultLogSink log_sink_;

    /**
     * Simple forwarding constructor, compatible with uavcan::Node.
     */
    Node(uavcan::ICanDriver& can_driver, uavcan::ISystemClock& clock) :
        Base(can_driver, clock)
    {
        Base::getLogger().setExternalSink(&log_sink_);
    }

    /**
     * Takes ownership of the driver container.
     */
    explicit Node(DriverPackPtr<ClockType>&& driver_pack) :
        Base(std::move(driver_pack))
    {
        Base::getLogger().setExternalSink(&log_sink_);
    }

public:

    Node(const Node&) = delete;
    Node& operator=(const Node&) = delete;

    using Ptr = std::unique_ptr<Node<ClockType>>;

    /**
     * Use this function to create a node instance with default SocketCAN driver.
     * It accepts the list of interface names to use for the new node, e.g. "can1", "vcan2", "slcan0".
     * Clock adjustment mode will be detected automatically unless provided explicitly.
     * @throws Exception.
     */
    static Ptr create(const std::vector<std::string>& iface_names)
    {
        return Ptr(new Node<ClockType>(DriverPackPtr<ClockType>(new DriverPack<ClockType>(iface_names))));
    }

    /**
     * Use this function to create a node instance with a custom driver.
     * Clock adjustment mode will be detected automatically unless provided explicitly.
     * @throws Exception.
     */
    static Ptr create(std::unique_ptr<uavcan::ICanDriver> can_driver)
    {
        return Ptr(new Node<ClockType>(DriverPackPtr<ClockType>(new DriverPack<ClockType>(std::move(can_driver)))));
    }

    /**
     * This function extends the other two overloads in such a way that it instantiates and initializes
     * the node immediately; if initialization fails, it throws.
     *
     * If NodeID is not provided, it will not be initialized, and therefore the node will be started in
     * listen-only (i.e. silent) mode. The node can be switched to normal (i.e. non-silent) mode at any
     * later time by calling setNodeID() explicitly. Read the Node class docs for more info.
     *
     * Clock adjustment mode will be detected automatically unless provided explicitly.
     *
     * @throws Exception, uavcan_posix::LibuavcanErrorException.
     */
    template <typename DriverType> static
    Ptr create(const DriverType& driver,
                    const uavcan::NodeStatusProvider::NodeName& name,
                    const uavcan::protocol::SoftwareVersion& software_version,
                    const uavcan::protocol::HardwareVersion& hardware_version,
                    const uavcan::NodeID node_id = uavcan::NodeID(),
                    const uavcan::TransferPriority node_status_transfer_priority =
                        uavcan::TransferPriority::Default)
    {
        auto node = Node<ClockType>::create(driver);

        node->setName(name);
        node->setSoftwareVersion(software_version);
        node->setHardwareVersion(hardware_version);

        if (node_id.isValid())
        {
            node->setNodeID(node_id);
        }

        const auto res = node->start(node_status_transfer_priority);
        if (res < 0)
        {
            throw LibuavcanErrorException(res);
        }

        return node;
    }

};

/**
 * Wrapper for uavcan::SubNode with some additional convenience functions.
 * Use one of the SubNode::make factory methods to instantiate.
 */
template<typename ClockType> class SubNode : public NodeBase<uavcan::SubNode<NodeMemPoolSize>, ClockType>
{
    using Base = NodeBase<uavcan::SubNode<NodeMemPoolSize>, ClockType>;

    /**
     * Simple forwarding constructor, compatible with uavcan::Node.
     */
    SubNode(uavcan::ICanDriver& can_driver, uavcan::ISystemClock& clock) : Base(can_driver, clock) { }

    /**
     * Takes ownership of the driver container.
     */
    explicit SubNode(DriverPackPtr<ClockType>&& driver_pack) : Base(std::move(driver_pack)) { }

public:

    SubNode(const SubNode&) = delete;
    SubNode& operator=(const SubNode&) = delete;

    using Ptr = std::unique_ptr<SubNode>;

    /**
     * Use this function to create a sub-node instance with default SocketCAN driver.
     * It accepts the list of interface names to use for the new node, e.g. "can1", "vcan2", "slcan0".
     * Clock adjustment mode will be detected automatically unless provided explicitly.
     * @throws Exception.
     */
    static Ptr create(const std::vector<std::string>& iface_names)
    {
        return Ptr(new SubNode(DriverPackPtr<ClockType>(new DriverPack<ClockType>(iface_names))));
    }

    /**
     * Use this function to create a sub-node instance with a custom driver.
     * Clock adjustment mode will be detected automatically unless provided explicitly.
     * @throws Exception.
     */
    static Ptr create(std::unique_ptr<uavcan::ICanDriver> can_driver)
    {
        return Ptr(new SubNode(DriverPackPtr<ClockType>(new DriverPack<ClockType>(std::move(can_driver)))));
    }

    /**
     * This function extends the other two overloads in such a way that it instantiates the node
     * and sets its Node ID immediately.
     *
     * Clock adjustment mode will be detected automatically unless provided explicitly.
     *
     * @throws Exception, uavcan_posix::LibuavcanErrorException.
     */
    template <typename DriverType>
    static Ptr create(DriverType driver, const uavcan::NodeID node_id)
    {
        auto sub_node = SubNode<ClockType>::create(std::move(driver));
        sub_node->setNodeID(node_id);
        return sub_node;
    }
};


} // namespace uavcan_posix
