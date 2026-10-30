TARGET = clouds_test
CC_FILES       = clouds_test.cc \
		pitch_quantizer.cc \
		synth.cc \
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
		trigger_generator.cc \
		resources.cc \
		units.cc \
		random.cc

STUBS = beads/test/common_stubs
HOST_FLAGS = -ffp-contract=off $(EXTRA)
include beads/test/host.mk

.PHONY: capture
capture:
	$(MAKE) -f beads/test/clouds.mk TARGET=clouds_capture \
		EXTRA=-DGOLDEN_CAPTURE test
