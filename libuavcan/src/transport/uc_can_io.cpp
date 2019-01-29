/*
* CAN bus IO logic.
* Copyright (C) 2014 Pavel Kirienko <pavel.kirienko@gmail.com>
* Copyright (C) 2019 Theodoros Ntakouris <zarkopafilis@gmail.com>
*/

#include <uavcan/transport/can_io.hpp>
#include <uavcan/debug.hpp>
#include <functional>
#include <cassert>

namespace uavcan {
/*
* CanRxFrame
*/
#if UAVCAN_TOSTRING

std::string CanRxFrame::toString(StringRepresentation mode) const {
    std::string out = CanFrame::toString(mode);
    out.reserve(128);
    out += " ts_m=" + ts_mono.toString();
    out += " ts_utc=" + ts_utc.toString();
    out += " iface=";
    out += char('0' + iface_index);
    return out;
}

#endif

/*
* CanTxQueue::Entry
*/
void CanTxQueueEntry::destroy(CanTxQueueEntry *&obj, IPoolAllocator &allocator) {
    if (obj != UAVCAN_NULLPTR) {
        obj->~CanTxQueueEntry();
        allocator.deallocate(obj);
        obj = UAVCAN_NULLPTR;
    }
}

#if UAVCAN_TOSTRING

std::string CanTxQueueEntry::toString() const {
    std::string str_qos;
    switch (qos) {
        case Volatile: {
            str_qos = "<volat> ";
            break;
        }
        case Persistent: {
            str_qos = "<perst> ";
            break;
        }
        default: {
            UAVCAN_ASSERT(0);
            str_qos = "<?WTF?> ";
            break;
        }
    }
    return str_qos + frame.toString();
}

#endif

/*
* CanTxQueue
*/
CanTxQueue::~CanTxQueue() {
    // Remove all nodes & node contents of the tree without performing re-balancing steps
    postOrderNodeTraverseRecursively(this->root_, [this](Node*& n){
        CanTxQueueEntry::destroy(n->data, this->allocator_);
    });
    // Step 2: AvlTree destructor is called to remove all the Node* (automatically after)
}

bool CanTxQueue::contains(const CanFrame &frame) const{
    Node *n = this->root_;

    while (n != UAVCAN_NULLPTR) {
        if(frame.priorityHigherThan(n->data->frame)){
            n = n->right;
            continue;
        }

        if(frame.priorityLowerThan(n->data->frame)){
            n = n->left;
            continue;
        }

        return frame == n->data->frame;
    }
    return false;
}

void CanTxQueue::safeIncrementRejectedFrames() {
    if (rejected_frames_cnt_ < NumericTraits<uint32_t>::max()) {
        rejected_frames_cnt_++;
    }
}

void CanTxQueue::push(const CanFrame &frame, MonotonicTime tx_deadline, Qos qos, CanIOFlags flags) {
    const MonotonicTime timestamp = sysclock_.getMonotonic();

    if (timestamp >= tx_deadline) {
        UAVCAN_TRACE("CanTxQueue", "Push rejected: already expired");
        safeIncrementRejectedFrames();
        return;
    }

    void *praw = this->allocator_.allocate(sizeof(CanTxQueueEntry));
    if (praw == UAVCAN_NULLPTR) {
        UAVCAN_TRACE("CanTxQueue", "Push rejected: OOM (CanTxQueueEntry)");
        safeIncrementRejectedFrames();
        return;
    }

    CanTxQueueEntry *entry = new(praw) CanTxQueueEntry(frame, tx_deadline, qos, flags);
    UAVCAN_ASSERT(entry);
    auto result = AvlTree::insert(entry);

    if (!result) {
        /* AVL Tree could not allocate a new node */
        UAVCAN_TRACE("CanTxQueue", "Push rejected: OOM (AvlTree::Node)");
        safeIncrementRejectedFrames();
        CanTxQueueEntry::destroy(entry, this->allocator_);
    }
}

void CanTxQueue::remove(CanTxQueueEntry *entry){
    if (entry == UAVCAN_NULLPTR) {
        return;
    }

    // Make the AvlTree remove the specific entry deleting it's Node *
    this->root_ = this->AvlTree::remove_helper(this->root_, entry);
    // Then let the entry destroy it's own contents
    CanTxQueueEntry::destroy(entry, this->allocator_);
}

CanTxQueueEntry *CanTxQueue::peek(){
    auto maxNode = searchForNonExpiredMax(this->root_);

    if(maxNode == UAVCAN_NULLPTR){
        return UAVCAN_NULLPTR;
    }

    return maxNode->data;
}

bool CanTxQueue::topPriorityHigherOrEqual(const CanFrame &rhs_frame){
    auto peek_entry = peek();
    if (peek_entry == UAVCAN_NULLPTR) {
        return false;
    }
    return !rhs_frame.priorityHigherThan(peek_entry->frame);
}

uavcan::AvlTree<uavcan::CanTxQueueEntry>::Node *CanTxQueue::searchForNonExpiredMax(Node *n) {
    if (n == UAVCAN_NULLPTR) {
        return UAVCAN_NULLPTR;
    }

    auto timestamp = sysclock_.getMonotonic();

    if(n->data->isExpired(timestamp)){
        auto expiredEntry = n->data;
        this->root_ = remove_always(n);
        CanTxQueueEntry::destroy(expiredEntry, this->allocator_);

        return searchForNonExpiredMax(this->root_);
    }

    while(n->right != UAVCAN_NULLPTR && n->right->data->isExpired(timestamp)){
        auto expiredEntry = n->data;
        n->right = remove_always(n);
        CanTxQueueEntry::destroy(expiredEntry, this->allocator_);
    }

    auto r = searchForNonExpiredMax(n->right);

    if (r != UAVCAN_NULLPTR) {
        return r;
    }

    return n;
}

/*
* CanIOManager
*/
int
CanIOManager::sendToIface(uint8_t iface_index, const CanFrame &frame, MonotonicTime tx_deadline, CanIOFlags flags) {
    UAVCAN_ASSERT(iface_index < MaxCanIfaces);
    ICanIface *const iface = driver_.getIface(iface_index);
    if (iface == UAVCAN_NULLPTR) {
        UAVCAN_ASSERT(0);   // Nonexistent interface
        return -ErrLogic;
    }
    const int res = iface->send(frame, tx_deadline, flags);
    if (res != 1) {
        UAVCAN_TRACE("CanIOManager", "Send failed: code %i, iface %i, frame %s",
                     res, iface_index, frame.toString().c_str());
    }
    if (res > 0) {
        counters_[iface_index].frames_tx += unsigned(res);
    }
    return res;
}

int CanIOManager::sendFromTxQueue(uint8_t iface_index) {
    UAVCAN_ASSERT(iface_index < MaxCanIfaces);
    CanTxQueueEntry *entry = tx_queues_[iface_index]->peek();
    if (entry == UAVCAN_NULLPTR) {
        return 0;
    }
    const int res = sendToIface(iface_index, entry->frame, entry->deadline, entry->flags);
    if (res > 0) {
        tx_queues_[iface_index]->remove(entry);
    }
    return res;
}

int CanIOManager::callSelect(CanSelectMasks &inout_masks, const CanFrame *(&pending_tx)[MaxCanIfaces],
                             MonotonicTime blocking_deadline) {
    const CanSelectMasks in_masks = inout_masks;

    const int res = driver_.select(inout_masks, pending_tx, blocking_deadline);
    if (res < 0) {
        return -ErrDriver;
    }

    inout_masks.read &= in_masks.read;  // Driver is not required to clean the masks
    inout_masks.write &= in_masks.write;
    return res;
}

CanIOManager::CanIOManager(ICanDriver &driver, IPoolAllocator &allocator, ISystemClock &sysclock,
                           std::size_t mem_blocks_per_iface)
        : driver_(driver), sysclock_(sysclock), num_ifaces_(driver.getNumIfaces()) {
    if (num_ifaces_ < 1 || num_ifaces_ > MaxCanIfaces) {
        handleFatalError("Num ifaces");
    }

    if (mem_blocks_per_iface == 0) {
        mem_blocks_per_iface = allocator.getBlockCapacity() / (num_ifaces_ + 1U) + 1U;
    }
    UAVCAN_TRACE("CanIOManager", "Memory blocks per iface: %u, total: %u",
                 unsigned(mem_blocks_per_iface), unsigned(allocator.getBlockCapacity()));

    for (int i = 0; i < num_ifaces_; i++) {
        tx_queues_[i].construct<IPoolAllocator &, ISystemClock &, std::size_t>
                (allocator, sysclock, mem_blocks_per_iface);
    }
}

uint8_t CanIOManager::makePendingTxMask() const {
    uint8_t write_mask = 0;
    for (uint8_t i = 0; i < getNumIfaces(); i++) {
        if (!tx_queues_[i]->isEmpty()) {
            write_mask = uint8_t(write_mask | (1 << i));
        }
    }
    return write_mask;
}

CanIfacePerfCounters CanIOManager::getIfacePerfCounters(uint8_t iface_index) const {
    ICanIface *const iface = driver_.getIface(iface_index);
    if (iface == UAVCAN_NULLPTR || iface_index >= MaxCanIfaces) {
        UAVCAN_ASSERT(0);
        return CanIfacePerfCounters();
    }
    CanIfacePerfCounters cnt;
    cnt.errors = iface->getErrorCount() + tx_queues_[iface_index]->getRejectedFrameCount();
    cnt.frames_rx = counters_[iface_index].frames_rx;
    cnt.frames_tx = counters_[iface_index].frames_tx;
    return cnt;
}

int CanIOManager::send(const CanFrame &frame, MonotonicTime tx_deadline, MonotonicTime blocking_deadline,
                       uint8_t iface_mask, Qos qos, CanIOFlags flags) {
    const uint8_t num_ifaces = getNumIfaces();
    const uint8_t all_ifaces_mask = uint8_t((1U << num_ifaces) - 1);
    iface_mask &= all_ifaces_mask;

    if (blocking_deadline > tx_deadline) {
        blocking_deadline = tx_deadline;
    }

    int retval = 0;

    while (true)        // Somebody please refactor this.
    {
        if (iface_mask == 0) {
            break;
        }

        CanSelectMasks masks;
        masks.write = iface_mask | makePendingTxMask();
        {
            // Building the list of next pending frames per iface.
            // The driver will give them a scrutinizing look before deciding whether he wants to accept them.
            // TODO: What does this mean? can we optimize further before passing them to the driver?
            const CanFrame *pending_tx[MaxCanIfaces] = {};
            for (int i = 0; i < num_ifaces; i++) {
                CanTxQueue &q = *tx_queues_[i];
                auto peek_entry = q.peek();
                const CanFrame *peek_frame = peek_entry == UAVCAN_NULLPTR ? UAVCAN_NULLPTR : &peek_entry->frame;

                if (iface_mask & (1 << i))      // I hate myself so much right now.
                {
                    auto hasPriority = false;

                    // This may seem duplicate of topPriorityHigherOrEqual but we want to avoid traversing the queue again
                    if(peek_entry != UAVCAN_NULLPTR){
                        hasPriority = !frame.priorityHigherThan(*peek_frame);
                    }

                    pending_tx[i] = hasPriority ? peek_frame : &frame;
                } else {
                    pending_tx[i] = peek_frame;
                }
            }

            const int select_res = callSelect(masks, pending_tx, blocking_deadline);
            if (select_res < 0) {
                return -ErrDriver;
            }
            UAVCAN_ASSERT(masks.read == 0);
        }

        // Transmission
        for (uint8_t i = 0; i < num_ifaces; i++) {
            if (masks.write & (1 << i)) {
                int res = 0;
                if (iface_mask & (1 << i)) {
                    if (tx_queues_[i]->topPriorityHigherOrEqual(frame)) {
                        res = sendFromTxQueue(
                                i);                 // May return 0 if nothing to transmit (e.g. expired)
                    }
                    if (res <= 0) {
                        res = sendToIface(i, frame, tx_deadline, flags);
                        if (res > 0) {
                            iface_mask &= uint8_t(~(1 << i));     // Mark transmitted
                        }
                    }
                } else {
                    res = sendFromTxQueue(i);
                }
                if (res > 0) {
                    retval++;
                }
            }
        }

        // Timeout. Enqueue the frame if wasn't transmitted and leave.
        const bool timed_out = sysclock_.getMonotonic() >= blocking_deadline;
        if (masks.write == 0 || timed_out) {
            if (!timed_out) {
                UAVCAN_TRACE("CanIOManager", "Send: Premature timeout in select(), will try again");
                continue;
            }
            for (uint8_t i = 0; i < num_ifaces; i++) {
                if (iface_mask & (1 << i)) {
                    tx_queues_[i]->push(frame, tx_deadline, qos, flags);
                }
            }
            break;
        }
    }
    return retval;
}

int CanIOManager::receive(CanRxFrame &out_frame, MonotonicTime blocking_deadline, CanIOFlags &out_flags) {
    const uint8_t num_ifaces = getNumIfaces();

    while (true) {
        CanSelectMasks masks;
        masks.write = makePendingTxMask();
        masks.read = uint8_t((1 << num_ifaces) - 1);
        {
            const CanFrame *pending_tx[MaxCanIfaces] = {};
            for (int i = 0; i < num_ifaces; i++)      // Dear compiler, kindly unroll this. Thanks.
            {
                auto entry = tx_queues_[i]->peek();
                pending_tx[i] = (entry == UAVCAN_NULLPTR) ? UAVCAN_NULLPTR : &entry->frame;
            }

            const int select_res = callSelect(masks, pending_tx, blocking_deadline);
            if (select_res < 0) {
                return -ErrDriver;
            }
        }

        // Write - if buffers are not empty, one frame will be sent for each iface per one receive() call
        for (uint8_t i = 0; i < num_ifaces; i++) {
            if (masks.write & (1 << i)) {
                (void) sendFromTxQueue(
                        i);  // It may fail, we don't care. Requested operation was receive, not send.
            }
        }

        // Read
        for (uint8_t i = 0; i < num_ifaces; i++) {
            if (masks.read & (1 << i)) {
                ICanIface *const iface = driver_.getIface(i);
                if (iface == UAVCAN_NULLPTR) {
                    UAVCAN_ASSERT(0);   // Nonexistent interface
                    continue;
                }

                const int res = iface->receive(out_frame, out_frame.ts_mono, out_frame.ts_utc, out_flags);
                if (res == 0) {
                    UAVCAN_ASSERT(
                            0);   // select() reported that iface has pending RX frames, but receive() returned none
                    continue;
                }
                out_frame.iface_index = i;

                if ((res > 0) && !(out_flags & CanIOFlagLoopback)) {
                    counters_[i].frames_rx += 1;
                }
                return (res < 0) ? -ErrDriver : res;
            }
        }

        // Timeout checked in the last order - this way we can operate with expired deadline:
        if (sysclock_.getMonotonic() >= blocking_deadline) {
            break;
        }
    }
    return 0;
}

}
