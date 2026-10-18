TARGET = backup_test
CC_FILES       = 		backup_test.cc \
		background_buffer_backup.cc \
		settings.cc

STUBS = beads/test/backup_stubs beads/test/common_stubs
HOST_SANITIZERS = 0
HOST_FLAGS = -DSTM32H7XX -ffp-contract=off -Wno-int-to-pointer-cast
include beads/test/host.mk
