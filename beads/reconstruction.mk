# Copyright 2026 Insolent Construct.
# 
# Author: Insolent Construct
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
# Reconstruction build: beads/makefile with a fixed link order, and the
# application padded to its flash slot. See beads/RECONSTRUCTION.md.

include beads/makefile

# Sort the sources of each package and keep the package order. GNU make 3.81
# and 4.3+ sort wildcard results; 3.82 to 4.2 do not.
CC_FILES       = $(notdir $(foreach p,$(PACKAGES),$(sort $(wildcard $(p)/*.cc))))
C_FILES        = $(notdir $(foreach p,$(PACKAGES),$(sort $(wildcard $(p)/*.c)))) \
		$(sort $(CORE_C_FILES)) $(sort $(DRIVERS_C_FILES))
AS_FILES       = $(notdir $(foreach p,$(PACKAGES),$(sort $(wildcard $(p)/*.s))))

# The application slot is 0x50000 bytes, erased to 0xff.
PADDED_BIN = $(BUILD_DIR)$(TARGET)_padded.bin

$(PADDED_BIN): $(TARGET_BIN)
	$(OBJCOPY) -I binary -O binary --pad-to 0x50000 --gap-fill 0xff $< $@

reconstruction: $(PADDED_BIN)
	sha256sum -c beads/reconstruction.sha256

.PHONY: reconstruction
