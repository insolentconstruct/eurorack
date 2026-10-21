TARGET = plaits_synth_test
include beads/test/plaits_sources.mk
CC_FILES       = 		plaits_synth_test.cc \
		reverb.cc \
		trigger_generator.cc \
		resources.cc \
		units.cc \
		pitch_quantizer.cc \
		random.cc \
		$(PLAITS_CC_FILES)

STUBS = beads/test/common_stubs
HOST_FLAGS = -ffp-contract=off
include beads/test/host.mk
