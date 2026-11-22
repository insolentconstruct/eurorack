# Copyright 2026 Insolent Construct.
# Author: Insolent Construct
# The options record, its compactor and the calibration rules.

TARGET = record_test
CC_FILES       = 		record_test.cc \
		settings.cc

STUBS = beads/test/common_stubs
# -funsigned-char: stmlib's checksum sums chars, unsigned on the Cortex-M7.
HOST_FLAGS = -DSTM32H7XX -ffp-contract=off -Wno-int-to-pointer-cast \
	-funsigned-char
include beads/test/host.mk
