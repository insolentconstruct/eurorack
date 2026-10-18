TARGET = delay_mode_test
CC_FILES = delay_mode_test.cc processor.cc mu_law.cc delay_clock.cc delay_controller.cc trigger_generator.cc resources.cc units.cc random.cc
STUBS = beads/test/delay_mode_stubs
SUITE_FLAGS = -include cstdio
include beads/test/suite.mk
