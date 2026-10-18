PACKAGES       =  beads/drivers beads/dsp beads/test stmlib/utils stmlib/dsp \
		beads

VPATH          = $(PACKAGES) beads/dsp/fx

TARGET         = firmware_test
BUILD_ROOT     = build/
BUILD_DIR      = $(BUILD_ROOT)$(TARGET)/
CC_FILES       = 		firmware_test.cc \
		beads.cc \
		cv_reader.cc \
		normalization_detector.cc \
		gain_control.cc \
		background_buffer_backup.cc \
		factory_test.cc \
		synth.cc \
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
	-Wno-int-to-pointer-cast -Ibeads/test/firmware_stubs -Ibeads/test/common_stubs -I. \
	-fsanitize=address,undefined,integer-divide-by-zero -fno-sanitize-recover=all -fno-pie
HOST_LDFLAGS = \
	-fsanitize=address,undefined,integer-divide-by-zero -no-pie
$(BUILD_DIR)beads.o: EXTRA_FLAGS = -Dmain=BeadsMain
include beads/test/host.mk
