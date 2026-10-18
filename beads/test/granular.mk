TARGET = granular_test
CC_FILES       = 		granular_test.cc \
		processor.cc \
		playback_options.cc \
		diffuser.cc \
		reverb.cc \
		delay_clock.cc \
		delay_controller.cc \
		trigger_generator.cc \
		mu_law.cc \
		resources.cc \
		units.cc \
		pitch_quantizer.cc \
		random.cc

STUBS = beads/test/options_stubs beads/test/common_stubs
HOST_FLAGS = -DSTM32H7XX -Wno-int-to-pointer-cast
include beads/test/host.mk
