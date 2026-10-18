TARGET = pitch_quantizer_test
CC_FILES       = pitch_quantizer_test.cc \
		resources.cc \
		units.cc \
		pitch_quantizer.cc \
		random.cc

STUBS = beads/test/common_stubs
include beads/test/host.mk
