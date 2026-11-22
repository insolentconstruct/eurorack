TARGET = storage_test
CC_FILES       = 		storage_test.cc \
		settings.cc

STUBS = beads/test/common_stubs
# -funsigned-char: stmlib's checksum sums chars, unsigned on the Cortex-M7.
HOST_FLAGS = -DSTM32H7XX -ffp-contract=off -Wno-int-to-pointer-cast \
	-funsigned-char
include beads/test/host.mk
