/*
 * Copyright (C) 2014 Pavel Kirienko <pavel.kirienko@gmail.com>
 * Copyright (C) 2019 Theodoros Ntakouris <zarkopafilis@gmail.com>
 */

#include <gtest/gtest.h>
#include <uavcan/transport/can_io.hpp>
#include <uavcan/util/avl_tree.hpp>
#include "can.hpp"

TEST(CanTxQueue, TxQueue)
{
    using uavcan::CanTxQueue;
    using uavcan::CanTxQueueEntry;
    using uavcan::CanFrame;
    using uavcan::Qos;

    ASSERT_GE(40, sizeof(CanTxQueueEntry)); // should be true for any platforms, though not required

    // 2 blocks per entry now, due to AvlTree
    uavcan::PoolAllocator<64 * 8, 64> pool;

    SystemClockMock clockmock;

    CanTxQueue queue(pool, clockmock, 99999);
    EXPECT_TRUE(queue.isEmpty());

    const uavcan::CanIOFlags flags = 0;

    // Descending priority
    const CanFrame f0 = makeCanFrame(0, "f0", EXT);
    const CanFrame f1 = makeCanFrame(10, "f1", EXT);
    const CanFrame f2 = makeCanFrame(20, "f2", EXT);
    const CanFrame f3 = makeCanFrame(100, "f3", EXT);
    const CanFrame f4 = makeCanFrame(10000, "f4", EXT);
    const CanFrame f5 = makeCanFrame(99999, "f5", EXT);
    const CanFrame f6 = makeCanFrame(999999, "f6", EXT);

    /*
     * Basic priority insertion
     */
    queue.push(f4, tsMono(100), Qos::Persistent, flags);
    EXPECT_FALSE(queue.isEmpty());
    EXPECT_TRUE(queue.contains(f4));
    EXPECT_EQ(1, queue.getSize());
    EXPECT_EQ(2, pool.getNumUsedBlocks());

    EXPECT_EQ(f4, queue.peek()->frame);
    EXPECT_TRUE(queue.topPriorityHigherOrEqual(f5));
    EXPECT_TRUE(queue.topPriorityHigherOrEqual(f4)); // Equal
    EXPECT_FALSE(queue.topPriorityHigherOrEqual(f3));

    queue.push(f3, tsMono(200), Qos::Persistent, flags);
    EXPECT_EQ(f3, queue.peek()->frame);
    EXPECT_EQ(2, queue.getSize());


    queue.push(f0, tsMono(300), Qos::Volatile, flags);
    EXPECT_EQ(f0, queue.peek()->frame);
    EXPECT_EQ(3, queue.getSize());


    queue.push(f1, tsMono(400), Qos::Volatile, flags);
    EXPECT_EQ(f0, queue.peek()->frame);              // Still f0, since it is highest
    EXPECT_TRUE(queue.topPriorityHigherOrEqual(f0)); // Equal
    EXPECT_TRUE(queue.topPriorityHigherOrEqual(f1));

    EXPECT_EQ(0, queue.getRejectedFrameCount());
    EXPECT_EQ(4, queue.getSize());

    EXPECT_TRUE(queue.contains(f0));
    EXPECT_TRUE(queue.contains(f1));
    EXPECT_TRUE(queue.contains(f3));
    EXPECT_TRUE(queue.contains(f4));

    EXPECT_FALSE(queue.contains(f2));
    EXPECT_FALSE(queue.contains(f5));
    EXPECT_FALSE(queue.contains(f6));

    /*
     * Removing
     */

    CanTxQueueEntry *entry = queue.peek();
    EXPECT_TRUE(entry);

    queue.remove(entry);
    EXPECT_TRUE(entry);
    EXPECT_EQ(3, queue.getSize());

    EXPECT_FALSE(queue.contains(f0));
    EXPECT_TRUE(queue.contains(f1));
    EXPECT_TRUE(queue.contains(f3));
    EXPECT_TRUE(queue.contains(f4));
    EXPECT_FALSE(queue.contains(f2));
    EXPECT_FALSE(queue.contains(f5));
    EXPECT_FALSE(queue.contains(f6));

    while(queue.peek() != UAVCAN_NULLPTR){
        queue.remove(queue.peek());
    }

    EXPECT_FALSE(queue.peek());
    EXPECT_FALSE(queue.contains(f0));
    EXPECT_FALSE(queue.contains(f5));

    EXPECT_EQ(0, queue.getSize());
    EXPECT_EQ(0, pool.getNumUsedBlocks());
    EXPECT_FALSE(queue.peek());
    EXPECT_FALSE(queue.topPriorityHigherOrEqual(f0));

    /*
    * Expiration Auto Remove on Peek
    */

    queue.push(f0, tsMono(999), Qos::Persistent, flags);
    queue.push(f4, tsMono(101), Qos::Persistent, flags);

    clockmock.monotonic = 102; // make f4 expire
    EXPECT_TRUE(queue.contains(f0));
    EXPECT_TRUE(queue.contains(f4)); // f0 is higher priority, so will get traversed first -- f4 not yet removed

    auto peek = queue.peek();
    EXPECT_EQ(f0, peek->frame);

    queue.remove(peek);

    EXPECT_EQ(1, queue.getSize());
    EXPECT_EQ(2, pool.getNumUsedBlocks());

    EXPECT_FALSE(queue.peek()); // f4 will be removed now that queue only contains f4
    EXPECT_FALSE(queue.contains(f4));

    EXPECT_EQ(0, queue.getSize());
    EXPECT_EQ(0, pool.getNumUsedBlocks());

    queue.push(f4, tsMono(98), Qos::Persistent, flags); // already expired

    EXPECT_FALSE(queue.peek());
    EXPECT_EQ(1, queue.getRejectedFrameCount());
    EXPECT_EQ(0, queue.getSize());
    EXPECT_EQ(0, pool.getNumUsedBlocks());

    /*
     * Add multiple so that we reach OOM
     */

    queue.push(f0, tsMono(900), Qos::Persistent, flags);
    EXPECT_EQ(2, pool.getNumUsedBlocks());

    queue.push(f1, tsMono(1000), Qos::Persistent, flags);
    EXPECT_EQ(4, pool.getNumUsedBlocks());

    queue.push(f2, tsMono(1100), Qos::Persistent, flags);
    EXPECT_EQ(6, pool.getNumUsedBlocks());

    queue.push(f3, tsMono(1200), Qos::Persistent, flags);
    EXPECT_EQ(8, pool.getNumUsedBlocks());
    EXPECT_TRUE(queue.contains(f3));

    queue.push(f4, tsMono(1300), Qos::Persistent, flags);
    EXPECT_EQ(8, pool.getNumUsedBlocks());
    EXPECT_FALSE(queue.contains(f4));

    EXPECT_EQ(4, queue.getSize());
    EXPECT_EQ(8, pool.getNumUsedBlocks());
    EXPECT_FALSE(queue.contains(f4)); // OOM happened on insertion
    EXPECT_EQ(2, queue.getRejectedFrameCount());

    /*
     * Remove all - cleanup
     */

    while(queue.peek() != UAVCAN_NULLPTR){
        queue.remove(queue.peek());
    }

    EXPECT_EQ(0, queue.getSize());
    EXPECT_EQ(0, pool.getNumUsedBlocks());
}
