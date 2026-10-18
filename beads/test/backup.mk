TARGET = backup_test
CC_FILES = backup_test.cc background_buffer_backup.cc settings.cc
STUBS = beads/test/backup_stubs
SUITE_FLAGS = -DSTM32H7XX -Wno-int-to-pointer-cast
SANITIZERS =
include beads/test/suite.mk
