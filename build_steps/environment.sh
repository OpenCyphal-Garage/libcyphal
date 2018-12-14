#!/usr/bin/env bash

# Export to tell cmake which native compilers to use.
export CXX="g++-7" CC="gcc-7";

# provide a default LIBUAVCAN_MAKE_JOBS value unless 
# this is overridden in the environment.
if [ -z ${LIBUAVCAN_MAKE_JOBS+x} ]; then
    export LIBUAVCAN_MAKE_JOBS="4";
    echo "LIBUAVCAN_MAKE_JOBS unset. Using default value ${LIBUAVCAN_MAKE_JOBS}"
fi
