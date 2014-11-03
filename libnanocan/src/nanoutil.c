#include "nanoutil.h"

#include <unistd.h>
#include <stdlib.h>
#include <math.h>

/*
 * Float16 support
 */
uint16_t make_float16(float value)
{
    uint16_t hbits = signbit(value) << 15;
    if (value == 0.0F)
    {
        return hbits;
    }
    if (isnan(value))
    {
        return hbits | 0x7FFFU;
    }
    if (isinf(value))
    {
        return hbits | 0x7C00U;
    }
    int exp;
    (void)frexp(value, &exp);
    if (exp > 16)
    {
        return hbits | 0x7C00U;
    }
    if (exp < -13)
    {
        value = ldexp(value, 24);
    }
    else
    {
        value = ldexp(value, 11 - exp);
        hbits |= ((exp + 14) << 10);
    }
    const int32_t ival = (int32_t)value;
    hbits |= (uint16_t)(((ival < 0) ? (-ival) : ival) & 0x3FFU);
    float diff = fabs(value - (float)ival);
    hbits += diff >= 0.5F;
    return hbits;
}

uint8_t get_transfer_id( uint32_t can_id )
{
    return can_id & 7;
}

uint8_t is_last_frame( uint32_t can_id )
{
    return (( can_id & 8 ) == 0x08 );
}

uint8_t get_frame_idx( uint32_t can_id )
{
    return (( can_id >> 4 ) & 0x3F );
}

uint8_t get_source_node_id( uint32_t can_id )
{
    return (( can_id >> 10 ) & 0x7F );
}

uint8_t get_transfer_type( uint32_t can_id )
{
    return (( can_id >> 17 ) & 0x03 );
}

uint16_t get_data_type_id( uint32_t can_id )
{
    return (( can_id >> 19 ) & 0x03FF );
}

