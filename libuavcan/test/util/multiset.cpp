/*
 * Copyright (C) 2014 Pavel Kirienko <pavel.kirienko@gmail.com>
 */

#include <string>
#include <cstdio>
#include <memory>
#include <gtest/gtest.h>
#include <uavcan/util/multiset.hpp>


static std::string toString(long x)
{
    char buf[80];
    std::snprintf(buf, sizeof(buf), "%li", x);
    return std::string(buf);
}

static bool oddValuePredicate(const std::string& value)
{
    EXPECT_FALSE(value.empty());
    const int num = atoi(value.c_str());
    return num & 1;
}

struct FindPredicate
{
    const std::string target;
    FindPredicate(const std::string& target) : target(target) { }
    bool operator()(const std::string& value) const { return value == target; }
};

struct NoncopyableWithCounter : uavcan::Noncopyable
{
    static int num_objects;
    long long value;

    NoncopyableWithCounter() : value(0) { num_objects++; }
    NoncopyableWithCounter(long long x) : value(x) { num_objects++; }
    ~NoncopyableWithCounter() { num_objects--; }

    static bool isNegative(const NoncopyableWithCounter& val)
    {
        return val.value < 0;
    }

    bool operator==(const NoncopyableWithCounter& ref) const { return ref.value == value; }
};

int NoncopyableWithCounter::num_objects = 0;

template <typename T>
struct SummationOperator : uavcan::Noncopyable
{
    T accumulator;
    SummationOperator() : accumulator() { }
    void operator()(const T& x) { accumulator += x; }
};

struct ClearingOperator
{
    template <typename T>
    void operator()(T& x) const { x = T(); }
};


TEST(Multiset, Basic)
{
    using uavcan::Multiset;

    static const int POOL_BLOCKS = 3;
    uavcan::PoolAllocator<uavcan::MemPoolBlockSize * POOL_BLOCKS, uavcan::MemPoolBlockSize> pool;

    typedef Multiset<std::string, 2> MultisetType;
    std::auto_ptr<MultisetType> mset(new MultisetType(pool));

    typedef SummationOperator<std::string> StringConcatenationOperator;

    // Empty
    mset->removeFirst("foo");
    ASSERT_EQ(0, pool.getNumUsedBlocks());
    ASSERT_FALSE(mset->getByIndex(0));
    ASSERT_FALSE(mset->getByIndex(1));
    ASSERT_FALSE(mset->getByIndex(10000));

    // Static addion
    ASSERT_EQ("1", *mset->emplace("1"));
    ASSERT_EQ("2", *mset->emplace("2"));
    ASSERT_EQ(0, pool.getNumUsedBlocks());
    ASSERT_EQ(2, mset->getNumStaticItems());
    ASSERT_EQ(0, mset->getNumDynamicItems());

    // Ordering
    ASSERT_TRUE(*mset->getByIndex(0) == "1");
    ASSERT_TRUE(*mset->getByIndex(1) == "2");

    {
        StringConcatenationOperator op;
        mset->forEach<StringConcatenationOperator&>(op);
        ASSERT_EQ("12", op.accumulator);
    }

    // Dynamic addition
    ASSERT_EQ("3", *mset->emplace("3"));
    ASSERT_EQ("3", *mset->getByIndex(2));
    ASSERT_EQ(1, pool.getNumUsedBlocks());

    ASSERT_EQ("4", *mset->emplace("4"));
    ASSERT_LE(1, pool.getNumUsedBlocks());      // One or more
    ASSERT_EQ(2, mset->getNumStaticItems());
    ASSERT_EQ(2, mset->getNumDynamicItems());

    // Making sure everything is here
    ASSERT_EQ("1", *mset->getByIndex(0));
    ASSERT_EQ("2", *mset->getByIndex(1));
    // 2 and 3 are not tested because their placement depends on number of items per dynamic block
    ASSERT_FALSE(mset->getByIndex(100));
    ASSERT_FALSE(mset->getByIndex(4));

    const std::string data_at_pos2 = *mset->getByIndex(2);
    const std::string data_at_pos3 = *mset->getByIndex(3);

    // Finding some items
    ASSERT_EQ("1", *mset->find(FindPredicate("1")));
    ASSERT_EQ("2", *mset->find(FindPredicate("2")));
    ASSERT_EQ("3", *mset->find(FindPredicate("3")));
    ASSERT_EQ("4", *mset->find(FindPredicate("4")));
    ASSERT_FALSE(mset->find(FindPredicate("nonexistent")));

    {
        StringConcatenationOperator op;
        mset->forEach<StringConcatenationOperator&>(op);
        std::cout << "Accumulator: " << op.accumulator << std::endl;
        ASSERT_EQ(4, op.accumulator.size());
    }

    // Removing one static; ordering will be preserved
    mset->removeFirst("1");
    mset->removeFirst("foo");                           // There's no such thing anyway
    ASSERT_LE(1, pool.getNumUsedBlocks());
    ASSERT_EQ(1, mset->getNumStaticItems());
    ASSERT_EQ(2, mset->getNumDynamicItems());           // This container does not move items

    // Ordering has not changed
    ASSERT_EQ("2", *mset->getByIndex(0));       // Entry "1" was here
    ASSERT_EQ(data_at_pos2, *mset->getByIndex(1));
    ASSERT_EQ(data_at_pos3, *mset->getByIndex(2));

    // Removing another static
    mset->removeFirst("2");
    ASSERT_EQ(0, mset->getNumStaticItems());
    ASSERT_EQ(2, mset->getNumDynamicItems());
    ASSERT_LE(1, pool.getNumUsedBlocks());

    // Adding some new items
    unsigned max_value_integer = 0;
    for (int i = 0; i < 100; i++)
    {
        const std::string value = toString(i);
        std::string* res = mset->emplace(value);  // Will NOT override above
        if (res == NULL)
        {
            ASSERT_LT(2, i);
            break;
        }
        else
        {
            ASSERT_EQ(value, *res);
        }
        max_value_integer = unsigned(i);
    }
    std::cout << "Max value: " << max_value_integer << std::endl;

    // Making sure there is true OOM
    ASSERT_EQ(0, pool.getNumFreeBlocks());
    ASSERT_FALSE(mset->emplace("nonexistent"));

    // Removing odd values - nearly half of them
    mset->removeAllWhere(oddValuePredicate);

    // Making sure there's no odd values left
    for (unsigned kv_int = 0; kv_int <= max_value_integer; kv_int++)
    {
        const std::string* val = mset->find(FindPredicate(toString(kv_int)));
        if (val)
        {
            ASSERT_FALSE(kv_int & 1);
        }
        else
        {
            ASSERT_TRUE(kv_int & 1);
        }
    }

    // Clearing all strings
    {
        StringConcatenationOperator op;
        mset->forEach<StringConcatenationOperator&>(op);
        std::cout << "Accumulator before clearing: " << op.accumulator << std::endl;
    }
    mset->forEach(ClearingOperator());
    {
        StringConcatenationOperator op;
        mset->forEach<StringConcatenationOperator&>(op);
        std::cout << "Accumulator after clearing: " << op.accumulator << std::endl;
        ASSERT_TRUE(op.accumulator.empty());
    }

    // Making sure the memory will be released
    mset.reset();
    ASSERT_EQ(0, pool.getNumUsedBlocks());
}


TEST(Multiset, PrimitiveKey)
{
    using uavcan::Multiset;

    static const int POOL_BLOCKS = 3;
    uavcan::PoolAllocator<uavcan::MemPoolBlockSize * POOL_BLOCKS, uavcan::MemPoolBlockSize> pool;

    typedef Multiset<int, 2> MultisetType;
    std::auto_ptr<MultisetType> mset(new MultisetType(pool));

    // Empty
    mset->removeFirst(8);
    ASSERT_EQ(0, pool.getNumUsedBlocks());
    ASSERT_EQ(0, mset->getSize());
    ASSERT_FALSE(mset->getByIndex(0));

    // Insertion
    ASSERT_EQ(1, *mset->emplace(1));
    ASSERT_EQ(1, mset->getSize());
    ASSERT_EQ(2, *mset->emplace(2));
    ASSERT_EQ(2, mset->getSize());
    ASSERT_EQ(3, *mset->emplace(3));
    ASSERT_EQ(4, *mset->emplace(4));
    ASSERT_EQ(4, mset->getSize());

#if UAVCAN_CPP_VERSION >= UAVCAN_CPP11
    // Only C++11 because C++03 uses one entry per pool block which breaks ordering
    ASSERT_EQ(1, *mset->getByIndex(0));
    ASSERT_EQ(2, *mset->getByIndex(1));
    ASSERT_EQ(3, *mset->getByIndex(2));
    ASSERT_EQ(4, *mset->getByIndex(3));
    ASSERT_FALSE(mset->getByIndex(5));
    ASSERT_FALSE(mset->getByIndex(1000));
#endif

    // Summation and clearing
    {
        SummationOperator<int> summation_operator;
        mset->forEach<SummationOperator<int>&>(summation_operator);
        ASSERT_EQ(1 + 2 + 3 + 4, summation_operator.accumulator);
    }
    mset->forEach(ClearingOperator());
    {
        SummationOperator<int> summation_operator;
        mset->forEach<SummationOperator<int>&>(summation_operator);
        ASSERT_EQ(0, summation_operator.accumulator);
    }
}


TEST(Multiset, NoncopyableWithCounter)
{
    using uavcan::Multiset;

    static const int POOL_BLOCKS = 3;
    uavcan::PoolAllocator<uavcan::MemPoolBlockSize * POOL_BLOCKS, uavcan::MemPoolBlockSize> pool;

    typedef Multiset<NoncopyableWithCounter, 2> MultisetType;
    std::auto_ptr<MultisetType> mset(new MultisetType(pool));

    ASSERT_EQ(0, NoncopyableWithCounter::num_objects);
    ASSERT_EQ(0,    mset->emplace()->value);
    ASSERT_EQ(1, NoncopyableWithCounter::num_objects);
    ASSERT_EQ(123,  mset->emplace(123)->value);
    ASSERT_EQ(2, NoncopyableWithCounter::num_objects);
    ASSERT_EQ(-456, mset->emplace(-456)->value);
    ASSERT_EQ(3, NoncopyableWithCounter::num_objects);
    ASSERT_EQ(456,  mset->emplace(456)->value);
    ASSERT_EQ(4, NoncopyableWithCounter::num_objects);
    ASSERT_EQ(-789, mset->emplace(-789)->value);
    ASSERT_EQ(5, NoncopyableWithCounter::num_objects);

    mset->removeFirst(NoncopyableWithCounter(0));
    ASSERT_EQ(4, NoncopyableWithCounter::num_objects);
    ASSERT_EQ(123, mset->getByIndex(0)->value);

    mset->removeFirstWhere(&NoncopyableWithCounter::isNegative);
    ASSERT_EQ(3, NoncopyableWithCounter::num_objects);
    ASSERT_EQ(456, mset->getByIndex(1)->value);                 // -456 is now removed

    mset->removeAllWhere(&NoncopyableWithCounter::isNegative);
    ASSERT_EQ(2, NoncopyableWithCounter::num_objects);          // Only 1 and 2 are left

    mset.reset();

    ASSERT_EQ(0, pool.getNumUsedBlocks());
    ASSERT_EQ(0, NoncopyableWithCounter::num_objects);          // All destroyed
}
