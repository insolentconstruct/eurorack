# Copyright 2026 Insolent Construct.
#
# Author: Insolent Construct (insolentconstruct@gmail.com)
#
# Permission is hereby granted, free of charge, to any person obtaining a copy
# of this software and associated documentation files (the "Software"), to deal
# in the Software without restriction, including without limitation the rights
# to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
# copies of the Software, and to permit persons to whom the Software is
# furnished to do so, subject to the following conditions:
#
# The above copyright notice and this permission notice shall be included in
# all copies or substantial portions of the Software.
#
# THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
# IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
# FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
# AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
# LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
# OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN
# THE SOFTWARE.
#
# See http://creativecommons.org/licenses/MIT/ for more information.
#
TARGET = resonator_deferred_test
include beads/test/plaits_sources.mk
CC_FILES       = 		resonator_deferred_test.cc \
		beads.cc \
		abyss.cc \
		engine_transition.cc \
		cv_reader.cc \
		normalization_detector.cc \
		gain_control.cc \
		background_buffer_backup.cc \
		factory_test.cc \
		synth.cc \
		rings_synth.cc \
		processor.cc \
		processor_options.cc \
		spectral_processor.cc \
		phase_vocoder.cc \
		stft.cc \
		frame_transformation.cc \
		atan.cc \
		playback_options.cc \
		diffuser.cc \
		reverb.cc \
		reverb_decay.cc \
		reverb_characters.cc \
		extended_audio.cc \
		mu_law.cc \
		delay_controller.cc \
		delay_clock.cc \
		ui.cc \
		gesture_controller.cc \
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
HOST_FLAGS = -DSTM32H7XX -Wno-int-to-pointer-cast $(PLAITS_FLAGS)
include beads/test/host.mk

$(BUILD_DIR)beads.o: EXTRA_FLAGS = -Dmain=BeadsMain
