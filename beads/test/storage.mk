TARGET = storage_test
CC_FILES = storage_test.cc settings.cc
SUITE_FLAGS = -DSTM32H7XX -Wno-int-to-pointer-cast
include beads/test/suite.mk
