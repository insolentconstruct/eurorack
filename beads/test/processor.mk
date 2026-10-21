TARGET = processor_test
CC_FILES       = 		processor_test.cc \
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
		delay_controller.cc \
		delay_clock.cc \
		ui.cc \
		options.cc \
		settings.cc \
		switches.cc \
		trigger_generator.cc \
		resources.cc \
		units.cc \
		pitch_quantizer.cc \
		random.cc

STUBS = beads/test/options_stubs beads/test/common_stubs
HOST_FLAGS = -DSTM32H7XX -Wno-int-to-pointer-cast
include beads/test/host.mk
