/// @copyright Copyright Amazon.com Inc. and its affiliates. All Rights Reserved.
/// @file
/// Static storage used by libcyphal::wrappers::can::Base

#include "posix/libcyphal/wrappers/can/base.hpp"

namespace libcyphal
{
namespace wrappers
{
namespace can
{

// @TODO: major refactor is needed to use CETL's pmr instead of this ugliness. The user of libcyphal should provide this
// type of memory and only demonstration/example code should have mass allocations like this in it.
std::size_t Base::heap_storage_size_bytes_ = LIBCYPHAL_TRANSPORT_MAX_HEAP_SIZE;
std::aligned_storage<sizeof(std::uint8_t), O1HEAP_ALIGNMENT> Base::heap_storage_[LIBCYPHAL_TRANSPORT_MAX_HEAP_SIZE];

}  // namespace can
}  // namespace wrappers
}  // namespace libcyphal

