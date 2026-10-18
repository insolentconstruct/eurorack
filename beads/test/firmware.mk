TARGET = firmware_test
CC_FILES = leds.cc firmware_test.cc beads.cc cv_reader.cc normalization_detector.cc gain_control.cc background_buffer_backup.cc factory_test.cc synth.cc processor.cc mu_law.cc delay_controller.cc delay_clock.cc ui.cc settings.cc switches.cc trigger_generator.cc resources.cc units.cc random.cc
STUBS = beads/test/firmware_stubs
SUITE_FLAGS = -include cstdio -DSTM32H7XX -Wno-int-to-pointer-cast
include beads/test/suite.mk
$(BUILD_DIR)beads.o: EXTRA_FLAGS = -Dmain=BeadsMain
