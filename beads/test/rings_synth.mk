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
# See http:#creativecommons.org/licenses/MIT/ for more information.
#
# -----------------------------------------------------------------------------
#
# The Rings synth core at Beads' block size, as the blob builds it.
TARGET = rings_synth_test
CC_FILES = rings_synth_test.cc rings_core_host.cc rings_synth.cc \
	rings_synth_core.cc \
	part.cc string_synth_part.cc string.cc fm_voice.cc resonator.cc \
	rings_resources.cc units.cc random.cc reverb.cc trigger_generator.cc \
	resources.cc pitch_quantizer.cc
STUBS = beads/test/common_stubs
HOST_FLAGS = -DRINGS_BLOB -DRINGS_BLOCK_SIZE=12
include beads/test/host.mk
VPATH += beads/blob/synths rings/dsp
