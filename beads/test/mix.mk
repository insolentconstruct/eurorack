TARGET = mix_test
CC_FILES = mix_test.cc processor.cc mu_law.cc delay_controller.cc delay_clock.cc trigger_generator.cc resources.cc units.cc random.cc
include beads/test/suite.mk
