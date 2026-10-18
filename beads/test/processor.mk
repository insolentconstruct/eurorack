TARGET = processor_test
CC_FILES = leds.cc processor_test.cc processor.cc mu_law.cc delay_controller.cc delay_clock.cc ui.cc settings.cc switches.cc trigger_generator.cc resources.cc units.cc random.cc
STUBS = beads/test/hardware_stubs
SUITE_FLAGS = -include cstdio -DSTM32H7XX -Wno-int-to-pointer-cast
include beads/test/suite.mk
