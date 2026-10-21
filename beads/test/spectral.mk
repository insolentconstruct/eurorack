TARGET = spectral_test$(SHY)
CC_FILES       = spectral_test.cc \
		clouds_pvoc_reference.cc \
		processor.cc \
		spectral_processor.cc \
		phase_vocoder.cc \
		stft.cc \
		frame_transformation.cc \
		atan.cc \
		pitch_quantizer.cc \
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

# The firmware's CMSIS FFT, built for the host with a stand-in arm_math.h.
# SHY=_shy builds the port with Clouds' ShyFFT instead, to match Clouds
# exactly.
CMSIS_DIR = stmlib/third_party/STM/CMSIS
ifeq ($(SHY),)
C_FILES = arm_rfft_fast_f32.c \
		arm_rfft_fast_init_f32.c \
		arm_cfft_f32.c \
		arm_cfft_radix8_f32.c \
		arm_common_tables.c \
		arm_const_structs.c \
		arm_bitreversal_host.c
endif

STUBS = beads/test/cmsis_stubs beads/test/common_stubs
HOST_FLAGS = -ffp-contract=off $(if $(SHY),,-DUSE_ARM_FFT) $(EXTRA)
C_OBJS = $(patsubst %.c,$(BUILD_ROOT)$(TARGET)/%.o,$(C_FILES))
HOST_LDFLAGS = $(C_OBJS)
include beads/test/host.mk
VPATH += $(CMSIS_DIR)/DSP_Lib/TransformFunctions \
		$(CMSIS_DIR)/DSP_Lib/CommonTables beads/test/cmsis_stubs

$(TARGET): $(C_OBJS)

$(BUILD_DIR)%.o: %.c | $(BUILD_DIR)
	gcc -c -O1 -g -Wall -Werror -ffp-contract=off -Ibeads/test/cmsis_stubs \
		-include beads/test/cmsis_stubs/arm_math.h \
		-I$(CMSIS_DIR)/CM3_h7xx $< -o $@

ifeq ($(SHY),)
test:
	$(MAKE) -f beads/test/spectral.mk SHY=_shy test
endif

.PHONY: capture
capture:
	$(MAKE) -f beads/test/spectral.mk EXTRA=-DGOLDEN_CAPTURE run-test
