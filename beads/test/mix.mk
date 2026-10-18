PACKAGES       =  beads/dsp beads/test stmlib/utils stmlib/dsp beads

VPATH          = $(PACKAGES) beads/dsp/fx

TARGET         = mix_test
BUILD_ROOT     = build/
BUILD_DIR      = $(BUILD_ROOT)$(TARGET)/
CC_FILES       = 		mix_test.cc \
		processor.cc \
		playback_options.cc \
		diffuser.cc \
		reverb.cc \
		mu_law.cc \
		delay_controller.cc \
		delay_clock.cc \
		trigger_generator.cc \
		resources.cc \
		units.cc \
		pitch_quantizer.cc \
		random.cc
HOST_FLAGS = \
	-DTEST -Os -g -Wall -Werror -Wno-unused-local-typedefs -Ibeads/test/common_stubs -I. \
	-fsanitize=address,undefined,integer-divide-by-zero -fno-sanitize-recover=all -fno-pie
HOST_LDFLAGS = \
	-fsanitize=address,undefined,integer-divide-by-zero -no-pie
include beads/test/host.mk
