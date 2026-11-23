TARGET = options_test
CC_FILES       = 		options_test.cc \
		ui.cc \
		gesture_controller.cc \
		options.cc \
		settings.cc \
		switches.cc

STUBS = beads/test/options_stubs beads/test/common_stubs
HOST_SANITIZERS = 0
# -funsigned-char: stmlib's checksum sums chars, unsigned on the Cortex-M7.
HOST_FLAGS = -DSTM32H7XX -ffp-contract=off -Wno-int-to-pointer-cast \
	-funsigned-char
include beads/test/host.mk
# Each call boundary in the UI, inlined settings calls included, reaches a
# hook in the test: where the audio interrupt could read a half-made change.
$(BUILD_DIR)ui.o $(BUILD_DIR)gesture_controller.o: EXTRA_FLAGS += -finstrument-functions
