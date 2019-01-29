/*
 * Copyright (C) 2019 Theodoros Ntakouris <zarkopafilis@gmail.com>
 * */

#include <gtest/gtest.h>
#include <uavcan/util/avl_tree.hpp>

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

    bool operator=(const Entry & other) const
    {
        return this->key == other.key;
    }
};

/* OOM-Unsafe */
Entry *makeEntry(PoolAllocator allocator, int key, int payload){
    void *praw = allocator.allocate(sizeof(Entry));

    Entry *e = new (praw) Entry();
    UAVCAN_ASSERT(e);

    e->key = key;
    e->payload = payload;
    return e;
}

/* Basic sanity checks */
TEST(AvlTree, Sanity){
    uavcan::PoolAllocator<64 * 4, 64> pool; // 4 (x2) entries capacity

    AvlTree<Entry> tree(pool, 4);
    EXPECT_TRUE(tree.isEmpty());
    EXPECT_EQ(0, pool.getNumUsedBlocks());

    auto e1 = makeEntry(1, 1);
    auto e2 = makeEntry(2, 2);
    auto e3 = makeEntry(3, 3);
    auto e4 = makeEntry(4, 4);

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

/* Check all possible rotation / balancing cases */
TEST(AvlTree, AllRotations){

}