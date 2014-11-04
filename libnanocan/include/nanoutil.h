#ifndef NANOUAVCAN_UTIL_H
#define NANOUAVCAN_UTIL_H

#include <stdint.h>

uint16_t make_float16(float value);

uint8_t get_transfer_id( uint32_t can_id );
uint8_t is_last_frame( uint32_t can_id );
uint8_t get_frame_idx( uint32_t can_id );
uint8_t get_source_node_id( uint32_t can_id );
uint8_t get_transfer_type( uint32_t can_id );
uint16_t get_data_type_id( uint32_t can_id );

#endif

