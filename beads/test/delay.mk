TARGET = delay_test
CC_FILES       = 		delay_test.cc \
		delay_clock.cc \
		mu_law.cc \
		resources.cc \
		units.cc \
		random.cc

STUBS = beads/test/common_stubs
include beads/test/host.mk
