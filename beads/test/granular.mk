TARGET = granular_test
CC_FILES = granular_test.cc processor.cc delay_clock.cc delay_controller.cc trigger_generator.cc mu_law.cc resources.cc units.cc random.cc
STUBS = beads/test/hardware_stubs
SUITE_FLAGS = -DSTM32H7XX -Wno-int-to-pointer-cast
include beads/test/suite.mk
