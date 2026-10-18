TARGET = mix_test
CC_FILES       = 		mix_test.cc \
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

STUBS = beads/test/common_stubs
include beads/test/host.mk
