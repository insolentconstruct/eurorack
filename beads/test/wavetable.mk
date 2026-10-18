TARGET = wavetable_test
CC_FILES = wavetable_test.cc synth.cc trigger_generator.cc resources.cc units.cc random.cc
SUITE_FLAGS = -include cstdio
include beads/test/suite.mk
