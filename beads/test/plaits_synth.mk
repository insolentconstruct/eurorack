TARGET = plaits_synth$(if $(PLAITS_AUX),_aux)_test
include beads/test/plaits_sources.mk
CC_FILES       = 		plaits_synth_test.cc \
		plaits_aux.cc \
		plaits_fm_banks.cc \
		reverb.cc \
		trigger_generator.cc \
		resources.cc \
		units.cc \
		pitch_quantizer.cc \
		random.cc \
		$(PLAITS_CC_FILES)

STUBS = beads/test/common_stubs
HOST_FLAGS = -ffp-contract=off $(PLAITS_FLAGS)
# Out-of-range float to integer conversions trap, as the other sanitizers.
ifneq ($(HOST_SANITIZERS),0)
HOST_FLAGS += -fsanitize=float-cast-overflow
HOST_LDFLAGS += -fsanitize=float-cast-overflow
endif
include beads/test/host.mk

# The test also builds and runs the engines with AUX, as Plaits renders them.
ifeq ($(PLAITS_AUX),)
test:
	$(MAKE) -f beads/test/plaits_synth.mk PLAITS_AUX=1 test
endif
