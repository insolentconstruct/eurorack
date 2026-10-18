PACKAGES       =  beads/dsp beads/test stmlib/utils stmlib/dsp beads

VPATH          = $(PACKAGES)

TARGET         = backup_test
BUILD_ROOT     = build/
BUILD_DIR      = $(BUILD_ROOT)$(TARGET)/
CC_FILES       = 		backup_test.cc \
		background_buffer_backup.cc \
		settings.cc
HOST_FLAGS = \
	-DTEST -Os -g -Wall -Werror -Wno-unused-local-typedefs -DSTM32H7XX -ffp-contract=off \
	-Wno-int-to-pointer-cast -Ibeads/test/backup_stubs -Ibeads/test/common_stubs -I.
HOST_LDFLAGS =
include beads/test/host.mk
