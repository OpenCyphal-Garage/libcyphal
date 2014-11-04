#
# Copyright (C) 2014 Pavel Kirienko <pavel.kirienko@gmail.com>
#

LIBNANOCAN_DIR := $(dir $(lastword $(MAKEFILE_LIST)))

UAVCAN_DIR := $(LIBNANOCAN_DIR)/../

#
# Library sources
#
LIBNANOCAN_SRC := $(shell find $(LIBNANOCAN_DIR)/src/ -type f -name '*.c')

LIBNANOCAN_INC := $(LIBNANOCAN_DIR)/include

#
# DSDL compiler executable
#
LIBNANOCAN_DSDLC := $(LIBNANOCAN_DIR)/nano_dsdl_compiler/libnanocan_dsdlc

#
# Standard DSDL definitions
#
UAVCAN_DSDL_DIR := $(UAVCAN_DIR)/dsdl/uavcan
