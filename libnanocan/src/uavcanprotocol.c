#include <uavcanprotocol.h>
#include <nanoutil.h>

static uint8_t uavcan_node_id;

// CAN ID
// 29 bits total
//
// 28 27 26 25 24 23 22 21 20 19 18 17 16 15 14 13 12 11 10 9 8 7 6 5 4 3 2 1 0
//|     data type id            | tt  | source node id     | frame idx |l| tid |
//
// tt = transfer type (type of message) 
// l = last frame?

uint32_t create_can_id( uint8_t transfer_id, uint16_t data_type_id )
{
    return (transfer_id & 7) |           // Transfer ID
        (1 << 3) |                       // Last Frame (always set)
        (0 << 4) |                       // Frame Index (always 0)
        ((uavcan_node_id & 127) << 10) | // Source Node ID
        (2 << 17) |                      // Transfer Type (always 2 - Message Broadcast)
        ((data_type_id & 1023) << 19);   // Data Type ID
}

uint16_t extract_data_type_id( uint32_t can_id )
{
    return get_data_type_id( can_id );
}

