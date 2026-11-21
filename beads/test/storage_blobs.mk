# The storage tests as development builds compile them, with the Plaits
# bank checks.

TARGET = storage_blobs_test
CC_FILES       = 		storage_test.cc \
		settings.cc

STUBS = beads/test/common_stubs
HOST_FLAGS = -DPLAITS_BLOBS -DSTM32H7XX -ffp-contract=off -Wno-int-to-pointer-cast
include beads/test/host.mk
