# Copyright 2026 Insolent Construct.
# Author: Insolent Construct
# Beads' synth from a WAVE section against the compiled-in table.

TARGET = wave_section_test
CC_FILES       = 		wave_section_test.cc \
		synth.cc \
		reverb.cc \
		trigger_generator.cc \
		resources.cc \
		units.cc \
		pitch_quantizer.cc \
		random.cc \
		plaits_blob.cc \
		plaits_region.cc \
		profile.cc \
		plaits_resources.cc

STUBS = beads/test/common_stubs
HOST_FLAGS = -DPLAITS_BLOBS
include beads/test/host.mk
VPATH += beads/blob
