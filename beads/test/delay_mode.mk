TARGET = delay_mode_test
CC_FILES       = 		delay_mode_test.cc \
		processor.cc \
		spectral_processor.cc \
		phase_vocoder.cc \
		stft.cc \
		frame_transformation.cc \
		atan.cc \
		playback_options.cc \
		diffuser.cc \
		reverb.cc \
		mu_law.cc \
		delay_clock.cc \
		delay_controller.cc \
		trigger_generator.cc \
		resources.cc \
		units.cc \
		pitch_quantizer.cc \
		random.cc

STUBS = beads/test/delay_mode_stubs beads/test/common_stubs
include beads/test/host.mk
