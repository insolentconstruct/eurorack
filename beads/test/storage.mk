TARGET = storage_test
CC_FILES       = 		storage_test.cc \
		settings.cc

STUBS = beads/test/common_stubs
HOST_FLAGS = -DSTM32H7XX -ffp-contract=off -Wno-int-to-pointer-cast
include beads/test/host.mk
