# Copyright 2026 Insolent Construct.
# Author: Insolent Construct
TARGET = scenario_host
CC_FILES       = 		scenario_host.cc \
		processor.cc \
		playback_options.cc \
		diffuser.cc \
		reverb.cc \
		mu_law.cc \
		delay_controller.cc \
		delay_clock.cc \
		trigger_generator.cc \
		resources.cc \
		units.cc \
		pitch_quantizer.cc \
		random.cc

PACKAGES = beads/repro/desktop
STUBS = beads/test/common_stubs
include beads/test/host.mk
