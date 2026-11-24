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
TARGET = reachability_ui_test
CC_FILES       = 		reachability_ui_test.cc \
		ui.cc \
		gesture_controller.cc \
		options.cc \
		settings.cc \
		switches.cc

STUBS = beads/test/options_stubs beads/test/common_stubs
HOST_SANITIZERS = 0
# -funsigned-char: stmlib's checksum sums chars, unsigned on the Cortex-M7.
HOST_FLAGS = -DSTM32H7XX -ffp-contract=off -Wno-int-to-pointer-cast \
	-funsigned-char
include beads/test/host.mk
# Each call boundary in the UI, inlined settings calls included, reaches a
# hook in the test: where the audio interrupt could read a half-made change.
$(BUILD_DIR)ui.o: EXTRA_FLAGS += -finstrument-functions
$(BUILD_DIR)gesture_controller.o: EXTRA_FLAGS += -finstrument-functions
