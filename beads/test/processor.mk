PACKAGES       =  beads/drivers beads/dsp beads/test stmlib/utils stmlib/dsp \
		beads

VPATH          = $(PACKAGES) beads/dsp/fx

TARGET         = processor_test
BUILD_ROOT     = build/
BUILD_DIR      = $(BUILD_ROOT)$(TARGET)/
CC_FILES       = 		processor_test.cc \
		processor.cc \
		playback_options.cc \
		diffuser.cc \
		reverb.cc \
		mu_law.cc \
		delay_controller.cc \
		delay_clock.cc \
		ui.cc \
		options.cc \
		settings.cc \
		switches.cc \
		trigger_generator.cc \
		resources.cc \
		units.cc \
		pitch_quantizer.cc \
		random.cc
HOST_FLAGS = \
	-DTEST -Os -g -Wall -Werror -Wno-unused-local-typedefs -DSTM32H7XX \
	-Wno-int-to-pointer-cast -Ibeads/test/options_stubs -Ibeads/test/common_stubs -I. \
	-fsanitize=address,undefined,integer-divide-by-zero -fno-sanitize-recover=all -fno-pie
HOST_LDFLAGS = \
	-fsanitize=address,undefined,integer-divide-by-zero -no-pie
include beads/test/host.mk
