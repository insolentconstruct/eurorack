# Copyright 2026 insolentconstruct.
#
# Author: insolentconstruct
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

# Link in a fixed order (the default wildcard order depends on the filesystem).
include $(KIT_MAKEFILE)
CC_FILES = $(notdir $(foreach p,$(PACKAGES),$(sort $(wildcard $(p)/*.cc))))
C_FILES = $(notdir $(foreach p,$(PACKAGES),$(sort $(wildcard $(p)/*.c)))) $(sort $(CORE_C_FILES)) $(sort $(DRIVERS_C_FILES))
AS_FILES = $(notdir $(foreach p,$(PACKAGES),$(sort $(wildcard $(p)/*.s))))
ifneq ($(words $(OBJ_FILES)),$(words $(sort $(OBJ_FILES))))
$(error Duplicate object basenames: cannot safely use upstream VPATH)
endif
ifneq ($(filter $(notdir $(STARTUP_OBJ)),$(OBJ_FILES)),)
$(error Startup appears in package objects)
endif
.PHONY: kit-info
kit-info:
	@printf '%s\n' '$(OBJS)' > '$(BUILD_DIR)objects.txt'
	@printf '%s\n' '$(CC) $(LDFLAGS) -o $(TARGET_ELF) $(OBJS)' > '$(BUILD_DIR)link.txt'

# Dependency list, used to collect license notices.
.PHONY: notice-deps
notice-deps: $(OBJS:.o=.nd)
$(BUILD_DIR)%.nd: %.s
	$(CC) -c -x assembler-with-cpp -M $(ASFLAGS) $< -MF $@ -MT $(@:.nd=.o)
$(BUILD_DIR)%.nd: %.c
	$(CC) -M $(CCFLAGS) $< -MF $@ -MT $(@:.nd=.o)
$(BUILD_DIR)%.nd: %.cc
	$(CXX) -M $(CCFLAGS) $(CPPFLAGS) $< -MF $@ -MT $(@:.nd=.o)
