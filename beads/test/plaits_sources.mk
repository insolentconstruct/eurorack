# Copyright 2026 Insolent Construct.
# Author: Insolent Construct
# Plaits synth sources, for host programs that link it; include after TARGET.

PLAITS_PACKAGES = plaits/dsp/chords plaits/dsp/engine plaits/dsp/engine2 \
	plaits/dsp/fm plaits/dsp/physical_modelling plaits/dsp/speech
PLAITS_CC_FILES = plaits_synth.cc \
		plaits_voice.cc \
		plaits_resources.cc \
		additive_engine.cc \
		algorithms.cc \
		bass_drum_engine.cc \
		chiptune_engine.cc \
		chord_bank.cc \
		chord_engine.cc \
		dx_units.cc \
		fm_engine.cc \
		grain_engine.cc \
		hi_hat_engine.cc \
		lpc_speech_synth.cc \
		lpc_speech_synth_controller.cc \
		lpc_speech_synth_phonemes.cc \
		lpc_speech_synth_words.cc \
		modal_engine.cc \
		modal_voice.cc \
		naive_speech_synth.cc \
		noise_engine.cc \
		particle_engine.cc \
		phase_distortion_engine.cc \
		resonator.cc \
		sam_speech_synth.cc \
		six_op_engine.cc \
		snare_drum_engine.cc \
		speech_engine.cc \
		string.cc \
		string_engine.cc \
		string_machine_engine.cc \
		string_voice.cc \
		swarm_engine.cc \
		virtual_analog_engine.cc \
		virtual_analog_vcf_engine.cc \
		waveshaping_engine.cc \
		wavetable_engine.cc \
		wave_terrain_engine.cc

# Plaits' DX7 units shift negative values left, which the firmware's GCC
# does arithmetically; keep the other checks on Plaits' own code.
PLAITS_OBJS = $(patsubst %.cc,$(or $(BUILD_ROOT),build/)$(TARGET)/%.o, \
	$(filter-out plaits_synth.cc plaits_voice.cc,$(PLAITS_CC_FILES)))
$(PLAITS_OBJS): EXTRA_FLAGS = -fno-sanitize=shift-base
