TARGET = trigger_test
CC_FILES       = 		trigger_test.cc \
		trigger_generator.cc \
		resources.cc \
		units.cc \
		random.cc

STUBS = beads/test/common_stubs
include beads/test/host.mk
