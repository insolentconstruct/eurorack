TARGET = firmware_test
include beads/test/plaits_sources.mk
CC_FILES       = 		firmware_test.cc \
		beads.cc \
		cv_reader.cc \
		normalization_detector.cc \
		gain_control.cc \
		background_buffer_backup.cc \
		factory_test.cc \
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
		ui.cc \
		options.cc \
		settings.cc \
		switches.cc \
		trigger_generator.cc \
		resources.cc \
		units.cc \
		pitch_quantizer.cc \
		random.cc \
		$(PLAITS_CC_FILES)

STUBS = beads/test/firmware_stubs beads/test/common_stubs
HOST_FLAGS = -DSTM32H7XX -Wno-int-to-pointer-cast
include beads/test/host.mk

$(BUILD_DIR)beads.o: EXTRA_FLAGS = -Dmain=BeadsMain
