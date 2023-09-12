/// @copyright Copyright Amazon.com, Inc. or its affiliates. All Rights Reserved.
/// @file
/// Build-time configurations and constant expressions.

#ifndef LIBCYPHAL_WRAPPERS_BUILD_CONFIG_HPP_INCLUDED
#define LIBCYPHAL_WRAPPERS_BUILD_CONFIG_HPP_INCLUDED

// +--------------------------------------------------------------------------+
/// @defgroup macros_data Data container size macros.

/// Macros that allow the library to be customized for different
/// target platforms with different memory constraints
/// @{

/// Max size of heap
#ifndef LIBCYPHAL_TRANSPORT_MAX_HEAP_SIZE
#    include <libcyphal/build_config.hpp>
#    define LIBCYPHAL_TRANSPORT_MAX_HEAP_SIZE (LIBCYPHAL_TRANSPORT_MAX_MESSAGE_SIZE_BYTES * 2)
#endif

/// @}  // end of macros_platform

#endif  // LIBCYPHAL_WRAPPERS_BUILD_CONFIG_HPP_INCLUDED
