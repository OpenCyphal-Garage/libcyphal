#!/usr/bin/env bash

# Export to tell cmake which native compilers to use.
# TODO: Migrate to GCC 7 or newer
export CXX="g++-5" CC="gcc-5";

# provide a default LIBUAVCAN_MAKE_JOBS value unless 
# this is overridden in the environment.
if [ -z ${LIBUAVCAN_MAKE_JOBS+x} ]; then
    export LIBUAVCAN_MAKE_JOBS="4";
    echo "LIBUAVCAN_MAKE_JOBS unset. Using default value ${LIBUAVCAN_MAKE_JOBS}"
fi
