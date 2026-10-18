PACKAGES       =  beads/dsp beads/test stmlib/utils stmlib/dsp beads

VPATH          = $(PACKAGES)

TARGET         = beads_test
BUILD_ROOT     = build/
BUILD_DIR      = $(BUILD_ROOT)$(TARGET)/
CC_FILES       = 		processor.cc \
		synth.cc \
		beads_test.cc \
		mu_law.cc \
		random.cc \
		resources.cc \
		delay_clock.cc \
		delay_controller.cc \
		trigger_generator.cc \
		pitch_quantizer.cc \
		units.cc
HOST_FLAGS = \
	-DTEST -Os -g -Wall -Werror -Wno-unused-local-typedefs -Ibeads/test/common_stubs -I.
HOST_LDFLAGS =
include beads/test/host.mk
