PACKAGES       =  beads/dsp beads/test stmlib/utils stmlib/dsp beads

VPATH          = $(PACKAGES)

TARGET         = delay_test
BUILD_ROOT     = build/
BUILD_DIR      = $(BUILD_ROOT)$(TARGET)/
CC_FILES       = 		delay_test.cc \
		delay_clock.cc \
		resources.cc \
		units.cc \
		random.cc
HOST_FLAGS = \
	-DTEST -Os -g -Wall -Werror -Wno-unused-local-typedefs -Ibeads/test/common_stubs -I. \
	-fsanitize=address,undefined,integer-divide-by-zero -fno-sanitize-recover=all -fno-pie
HOST_LDFLAGS = \
	-fsanitize=address,undefined,integer-divide-by-zero -no-pie
include beads/test/host.mk
