/*
 * CAN bus IO logic.
 * Copyright (C) 2014 Pavel Kirienko <pavel.kirienko@gmail.com>
 * Copyright (C) 2019 Theodoros Ntakouris <zarkopafilis@gmail.com>
 */

#ifndef UAVCAN_TRANSPORT_CAN_IO_HPP_INCLUDED
#define UAVCAN_TRANSPORT_CAN_IO_HPP_INCLUDED

#include <cassert>
#include <uavcan/error.hpp>
#include <uavcan/std.hpp>
#include <uavcan/util/linked_list.hpp>
#include <uavcan/util/avl_tree.hpp>
#include <uavcan/dynamic_memory.hpp>
#include <uavcan/build_config.hpp>
#include <uavcan/util/templates.hpp>
#include <uavcan/util/lazy_constructor.hpp>
#include <uavcan/driver/can.hpp>
#include <uavcan/driver/system_clock.hpp>
#include <uavcan/time.hpp>

namespace uavcan
{
struct UAVCAN_EXPORT CanRxFrame : public CanFrame
{
    MonotonicTime ts_mono;
    UtcTime ts_utc;
    uint8_t iface_index;

    CanRxFrame()
        : iface_index(0)
    { }

#if UAVCAN_TOSTRING
    std::string toString(StringRepresentation mode = StrTight) const;
#endif
};

struct CanTxQueueEntry  // Not required to be packed - fits the block in any case
{
    MonotonicTime deadline;
    const CanFrame frame;
    CanIOFlags flags;

    CanTxQueueEntry(const CanFrame& arg_frame, const MonotonicTime arg_deadline, CanIOFlags arg_flags)
        : deadline(arg_deadline)
        , frame(arg_frame)
        , flags(arg_flags)
    {
        IsDynamicallyAllocatable<CanTxQueueEntry>::check();
    }

    static void destroy(CanTxQueueEntry*& obj, IPoolAllocator& allocator);

    bool isExpired(const MonotonicTime timestamp) const { return timestamp > deadline; }

    bool operator<(const CanTxQueueEntry& other) const
    {
        return this->frame.priorityLowerThan(other.frame);
    }

    bool operator>(const CanTxQueueEntry& other) const
    {
        return this->frame.priorityHigherThan(other.frame);
    }

    bool operator==(const CanTxQueueEntry& other) const
    {
        return this->frame == other.frame;
    }

#if UAVCAN_TOSTRING
        std::string toString() const;
#endif
};

class UAVCAN_EXPORT CanTxQueue : public AvlTree<CanTxQueueEntry>
{
private:
    void postOrderTraverseEntryCleanup(Node* n);

protected:
    ISystemClock& sysclock_;
    uint32_t rejected_frames_cnt_;

    bool linkedListContains(Node* head, const CanFrame& frame) const;
    void safeIncrementRejectedFrames();
    AvlTree::Node* searchForNonExpiredMax(Node* n);

public:
    CanTxQueue(IPoolAllocator& allocator, ISystemClock& sysclock, std::size_t allocator_quota) :
        AvlTree(allocator, allocator_quota),
        sysclock_(sysclock),
        rejected_frames_cnt_(0)
    {}

    ~CanTxQueue() override;

    /* Avl Tree allocates the AvlTree::Node, while this(CanTxQueue) allocates the CanTxQueueEntry
     * Same logic for removal. */
    void push(const CanFrame& frame, MonotonicTime tx_deadline, CanIOFlags flags);
    void remove(CanTxQueueEntry* entry);

    uint32_t getRejectedFrameCount() const { return rejected_frames_cnt_; }

    bool contains(const CanFrame& frame) const;

    /* Tries to look up rightmost Node. If the frame is expired, removes it and continues traversing */
    CanTxQueueEntry* peek();

    bool topPriorityHigherOrEqual(const CanFrame& rhs_frame);
};

struct UAVCAN_EXPORT CanIfacePerfCounters
{
    uint64_t frames_tx;
    uint64_t frames_rx;
    uint64_t errors;

    CanIfacePerfCounters()
        : frames_tx(0)
        , frames_rx(0)
        , errors(0)
    { }
};

class UAVCAN_EXPORT CanIOManager : Noncopyable
{
    struct IfaceFrameCounters
    {
        uint64_t frames_tx;
        uint64_t frames_rx;

        IfaceFrameCounters()
            : frames_tx(0)
            , frames_rx(0)
        { }
    };

    ICanDriver& driver_;
    ISystemClock& sysclock_;

    LazyConstructor<CanTxQueue> tx_queues_[MaxCanIfaces];
    IfaceFrameCounters counters_[MaxCanIfaces];

    const uint8_t num_ifaces_;

    int sendToIface(uint8_t iface_index, const CanFrame& frame, MonotonicTime tx_deadline, CanIOFlags flags);
    int sendFromTxQueue(uint8_t iface_index);
    int callSelect(CanSelectMasks& inout_masks, const CanFrame* (& pending_tx)[MaxCanIfaces],
                   MonotonicTime blocking_deadline);

public:
    CanIOManager(ICanDriver& driver, IPoolAllocator& allocator, ISystemClock& sysclock,
                 std::size_t mem_blocks_per_iface = 0);

    uint8_t getNumIfaces() const { return num_ifaces_; }

    CanIfacePerfCounters getIfacePerfCounters(uint8_t iface_index) const;

    const ICanDriver& getCanDriver() const { return driver_; }
    ICanDriver& getCanDriver()             { return driver_; }

    uint8_t makePendingTxMask() const;

    /**
     * Returns:
     *  0 - rejected/timedout/enqueued
     *  1+ - sent/received
     *  negative - failure
     */
    int send(const CanFrame& frame, MonotonicTime tx_deadline, MonotonicTime blocking_deadline,
             uint8_t iface_mask, CanIOFlags flags);
    int receive(CanRxFrame& out_frame, MonotonicTime blocking_deadline, CanIOFlags& out_flags);
};

}

#endif // UAVCAN_TRANSPORT_CAN_IO_HPP_INCLUDED
