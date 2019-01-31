/*
 * Copyright (C) 2019 Theodoros Ntakouris <zarkopafilis@gmail.com>
 * */

#include <gtest/gtest.h>
#include <uavcan/util/avl_tree.hpp>
#include <uavcan/dynamic_memory.hpp>
#include <uavcan/driver/system_clock.hpp>

using uavcan::AvlTree;

struct Entry{
    int key;
    int payload;

    bool operator<(const Entry & other) const
    {
        return this->key < other.key;
    }

    bool operator>(const Entry & other) const
    {
        return this->key > other.key;
    }

    bool operator==(const Entry & other) const
    {
        return this->key == other.key;
    }
};

/* OOM-Unsafe */
inline Entry *makeEntry(uavcan::PoolAllocator<64 * 8, 64> *allocator, int key, int payload){
    void *praw = allocator->allocate(sizeof(Entry));

    Entry *e = new (praw) Entry();
    UAVCAN_ASSERT(e);

    e->key = key;
    e->payload = payload;
    return e;
}

/* Basic sanity checks */
TEST(AvlTree, Sanity){
    uavcan::PoolAllocator<64 * 8, 64> pool; // 4 (x2) entries capacity

    AvlTree<Entry> tree(pool, 99999);
    EXPECT_TRUE(tree.isEmpty());
    EXPECT_EQ(0, pool.getNumUsedBlocks());

    auto e1 = makeEntry(&pool, 1, 1);
    auto e2 = makeEntry(&pool, 2, 2);
    auto e3 = makeEntry(&pool, 3, 3);
    auto e4 = makeEntry(&pool, 4, 4);

    EXPECT_EQ(4, pool.getNumUsedBlocks());

    tree.insert(e1);
    EXPECT_TRUE(tree.contains(e1));
    EXPECT_EQ(e1, tree.max());
    EXPECT_EQ(1, tree.getSize());
    EXPECT_EQ(5, pool.getNumUsedBlocks());

    tree.remove_entry(e1);
    EXPECT_FALSE(tree.contains(e1));
    EXPECT_EQ(UAVCAN_NULLPTR, tree.max());
    EXPECT_EQ(0, tree.getSize());
    EXPECT_EQ(4, pool.getNumUsedBlocks());

    // Won't break if asked to remove data that do not exist
    tree.remove_entry(e1);
    EXPECT_FALSE(tree.contains(e1));
    EXPECT_EQ(UAVCAN_NULLPTR, tree.max());
    EXPECT_EQ(0, tree.getSize());
    EXPECT_EQ(4, pool.getNumUsedBlocks());

    /*
     * Insert e2 - e1 - e3 - e4
     */

    tree.insert(e2);
    EXPECT_TRUE(tree.contains(e2));
    EXPECT_EQ(e2, tree.max());
    EXPECT_EQ(1, tree.getSize());
    EXPECT_EQ(5, pool.getNumUsedBlocks());

    tree.insert(e1);
    EXPECT_TRUE(tree.contains(e1));
    EXPECT_EQ(e2, tree.max());
    EXPECT_EQ(2, tree.getSize());
    EXPECT_EQ(6, pool.getNumUsedBlocks());

    tree.insert(e3);
    EXPECT_TRUE(tree.contains(e3));
    EXPECT_EQ(e3, tree.max());
    EXPECT_EQ(3, tree.getSize());
    EXPECT_EQ(7, pool.getNumUsedBlocks());

    tree.insert(e4);
    EXPECT_TRUE(tree.contains(e4));
    EXPECT_EQ(e4, tree.max());
    EXPECT_EQ(4, tree.getSize());
    EXPECT_EQ(8, pool.getNumUsedBlocks());

    /*
    * Remove e2 - e4
    */

    tree.remove_entry(e2);
    EXPECT_TRUE(tree.contains(e1));
    EXPECT_FALSE(tree.contains(e2));
    EXPECT_TRUE(tree.contains(e3));
    EXPECT_TRUE(tree.contains(e4));
    EXPECT_EQ(e4, tree.max());
    EXPECT_EQ(3, tree.getSize());
    EXPECT_EQ(7, pool.getNumUsedBlocks());

    tree.remove_entry(e4);
    EXPECT_TRUE(tree.contains(e1));
    EXPECT_TRUE(tree.contains(e3));
    EXPECT_FALSE(tree.contains(e4));
    EXPECT_EQ(e3, tree.max());
    EXPECT_EQ(2, tree.getSize());
    EXPECT_EQ(6, pool.getNumUsedBlocks());
}

/* Test multiple entries with same 'key' */
TEST(AvlTree, MultipleEntriesPerKey){
    uavcan::PoolAllocator<64 * 8, 64> pool; // 4 (x2) entries capacity

    AvlTree<Entry> tree(pool, 99999);

    auto e1 = makeEntry(&pool, 1, 1);
    auto e1_1 = makeEntry(&pool, 1, 11);
    auto e1_11 = makeEntry(&pool, 1, 111);

    auto e2 = makeEntry(&pool, 2, 2);

    /*
     * Insert 2 entries with same key
     * */
    tree.insert(e1);

    tree.insert(e1_1);
    EXPECT_TRUE(tree.contains(e1));
    EXPECT_TRUE(tree.contains(e1_1));
    EXPECT_EQ(e1, tree.max());
    EXPECT_EQ(2, tree.getSize());
    EXPECT_EQ(6, pool.getNumUsedBlocks());


    tree.remove_entry(e1);
    EXPECT_FALSE(tree.contains(e1));
    EXPECT_TRUE(tree.contains(e1_1));

    EXPECT_EQ(e1_1, tree.max());
    EXPECT_EQ(1, tree.getSize());
    EXPECT_EQ(5, pool.getNumUsedBlocks());

    tree.remove_entry(e1);

    /*
     * Insert another with higher priority and
     * test again: removing in the middle and end of queue
     * */
    tree.insert(e2);

    tree.insert(e1);
    tree.insert(e1_1);
    tree.insert(e1_11);

    EXPECT_TRUE(tree.contains(e2));
    EXPECT_TRUE(tree.contains(e1));
    EXPECT_TRUE(tree.contains(e1_1));
    EXPECT_TRUE(tree.contains(e1_11));

    EXPECT_EQ(e2, tree.max());
    EXPECT_EQ(4, tree.getSize());
    EXPECT_EQ(8, pool.getNumUsedBlocks());

    tree.remove_entry(e2);
    tree.remove_entry(e1_1); // middle one in node with key == 1
    EXPECT_FALSE(tree.contains(e2));
    EXPECT_TRUE(tree.contains(e1));
    EXPECT_FALSE(tree.contains(e1_1));
    EXPECT_TRUE(tree.contains(e1_11));

    EXPECT_EQ(e1, tree.max()); // peeked in the order they were inserted
    EXPECT_EQ(2, tree.getSize());
    EXPECT_EQ(6, pool.getNumUsedBlocks());

    tree.remove_entry(e1_11); // last one in queue
    EXPECT_EQ(e1, tree.max());
    EXPECT_FALSE(tree.contains(e1_11));

    EXPECT_EQ(1, tree.getSize());
    EXPECT_EQ(5, pool.getNumUsedBlocks());
}

/* Check all possible rotation / balancing cases */
TEST(AvlTree, AllRotations){

}