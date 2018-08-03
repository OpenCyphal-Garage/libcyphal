/*
 * Copyright (C) 2014 Pavel Kirienko <pavel.kirienko@gmail.com>
 */

#include <algorithm>
#include <gtest/gtest.h>
#include <uavcan/transport/outgoing_transfer_registry.hpp>
#include "../clock.hpp"
#include "transfer_test_helpers.hpp"


TEST(OutgoingTransferRegistry, Basic)
{
    using uavcan::OutgoingTransferRegistryKey;
    uavcan::PoolAllocator<uavcan::MemPoolBlockSize * 2, uavcan::MemPoolBlockSize> poolmgr;
    uavcan::OutgoingTransferRegistry otr(poolmgr);

    otr.cleanup(tsMono(1000));

    const OutgoingTransferRegistryKey keys[] =
    {
        OutgoingTransferRegistryKey(123, uavcan::TransferTypeServiceRequest,   42),
        OutgoingTransferRegistryKey(321, uavcan::TransferTypeMessageBroadcast, 0),
        OutgoingTransferRegistryKey(213, uavcan::TransferTypeServiceRequest,   2),
        OutgoingTransferRegistryKey(312, uavcan::TransferTypeServiceRequest,   4),
        OutgoingTransferRegistryKey(456, uavcan::TransferTypeServiceRequest,   2),
        OutgoingTransferRegistryKey(457, uavcan::TransferTypeServiceRequest,   2),
        OutgoingTransferRegistryKey(458, uavcan::TransferTypeServiceRequest,   2),
        OutgoingTransferRegistryKey(459, uavcan::TransferTypeServiceRequest,   2),
        OutgoingTransferRegistryKey(460, uavcan::TransferTypeServiceRequest,   2),
        OutgoingTransferRegistryKey(470, uavcan::TransferTypeServiceRequest,   2)
    };

    ASSERT_EQ(0, otr.accessOrCreate(keys[0], tsMono(1000000))->get());
    ASSERT_EQ(0, otr.accessOrCreate(keys[1], tsMono(1000000))->get());
    ASSERT_EQ(0, otr.accessOrCreate(keys[2], tsMono(1000000))->get());
    ASSERT_EQ(0, otr.accessOrCreate(keys[3], tsMono(1000000))->get());
    bool did_run_out_of_memory = false;
    size_t i = 4;
    for (; !did_run_out_of_memory && i < std::extent<decltype(keys)>::value; ++i)
    {
        if(!otr.accessOrCreate(keys[i], tsMono(1000000)))
        {
            did_run_out_of_memory = true;
        }
    }

    ASSERT_TRUE(did_run_out_of_memory) << "The MemPoolBlockSize is larger then this test expected." << std::endl;

    /*
     * Incrementing a little
     */
    otr.accessOrCreate(keys[0], tsMono(2000000))->increment();
    otr.accessOrCreate(keys[0], tsMono(4000000))->increment();
    otr.accessOrCreate(keys[0], tsMono(3000000))->increment();
    ASSERT_EQ(3, otr.accessOrCreate(keys[0], tsMono(5000000))->get());

    otr.accessOrCreate(keys[2], tsMono(2000000))->increment();
    otr.accessOrCreate(keys[2], tsMono(3000000))->increment();
    ASSERT_EQ(2, otr.accessOrCreate(keys[2], tsMono(6000000))->get());

    otr.accessOrCreate(keys[3], tsMono(9000000))->increment();
    ASSERT_EQ(1, otr.accessOrCreate(keys[3], tsMono(4000000))->get());

    ASSERT_EQ(0, otr.accessOrCreate(keys[1], tsMono(4000000))->get());

    ASSERT_FALSE(otr.accessOrCreate(keys[i], tsMono(1000000)));        // Still OOM

    /*
     * Checking existence
     * Exist: 0, 1, 2, 3
     * Does not exist: 4
     */
    ASSERT_TRUE(otr.exists(keys[1].getDataTypeID(), keys[1].getTransferType()));
    ASSERT_TRUE(otr.exists(keys[0].getDataTypeID(), keys[0].getTransferType()));
    ASSERT_TRUE(otr.exists(keys[3].getDataTypeID(), keys[3].getTransferType()));
    ASSERT_TRUE(otr.exists(keys[2].getDataTypeID(), keys[2].getTransferType()));

    ASSERT_FALSE(otr.exists(keys[1].getDataTypeID(), keys[2].getTransferType()));  // Invalid combination
    ASSERT_FALSE(otr.exists(keys[0].getDataTypeID(), keys[1].getTransferType()));  // Invalid combination
    ASSERT_FALSE(otr.exists(keys[i].getDataTypeID(), keys[i].getTransferType()));  // Plain missing

    /*
     * Cleaning up
     */
    otr.cleanup(tsMono(4000001));    // Kills 1, 3
    ASSERT_EQ(0, otr.accessOrCreate(keys[1], tsMono(1000000))->get());
    ASSERT_EQ(0, otr.accessOrCreate(keys[3], tsMono(1000000))->get());
    otr.accessOrCreate(keys[1], tsMono(5000000))->increment();
    otr.accessOrCreate(keys[3], tsMono(5000000))->increment();

    ASSERT_EQ(3, otr.accessOrCreate(keys[0], tsMono(5000000))->get());
    ASSERT_EQ(2, otr.accessOrCreate(keys[2], tsMono(6000000))->get());

    otr.cleanup(tsMono(5000001));    // Kills 1, 3 (He needs a bath, Jud. He stinks of the ground you buried him in.), 0
    ASSERT_EQ(0, otr.accessOrCreate(keys[0], tsMono(1000000))->get());
    ASSERT_EQ(0, otr.accessOrCreate(keys[1], tsMono(1000000))->get());
    ASSERT_EQ(0, otr.accessOrCreate(keys[3], tsMono(1000000))->get());

    ASSERT_EQ(2, otr.accessOrCreate(keys[2], tsMono(1000000))->get());

    otr.cleanup(tsMono(5000001));    // Frees some memory for 4
    ASSERT_EQ(0, otr.accessOrCreate(keys[0], tsMono(1000000))->get());
}
