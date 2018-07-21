#
# Copyright (C) 2014 Pavel Kirienko <pavel.kirienko@gmail.com>
#

LIBUAVCAN_NXPK20_DIR := $(dir $(lastword $(MAKEFILE_LIST)))

LIBUAVCAN_NXPK20_SRC := $(shell find $(LIBUAVCAN_NXPK20_DIR)/src -type f -name '*.cpp')

LIBUAVCAN_NXPK20_INC := $(LIBUAVCAN_NXPK20_DIR)/include/

