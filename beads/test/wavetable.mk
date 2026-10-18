TARGET = wavetable_test
CC_FILES       = 		wavetable_test.cc \
		synth.cc \
		reverb.cc \
		trigger_generator.cc \
		resources.cc \
		units.cc \
		pitch_quantizer.cc \
		random.cc

STUBS = beads/test/common_stubs
include beads/test/host.mk
