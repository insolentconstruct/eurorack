# Copyright 2026 Insolent Construct.
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

# Run inside the GCC 9 build kit. Stage a separate source tree for the timing ELF.
OUT ?= /out
JOBS ?= 2
STOCK_REGION ?=

.PHONY: timing
timing:
	@test -n "$(TOOLCHAIN_PATH)"
	@mkdir -p "$(OUT)"
	@test -z "$$(ls -A "$(OUT)")"
	mkdir "$(OUT)/source"
	cp -a beads plaits rings stmlib "$(OUT)/source/"
	cd "$(OUT)/source" && sha256sum -c beads/test/s1_timing/application.sha256
	cd "$(OUT)/source" && GIT_CEILING_DIRECTORIES="$(abspath $(OUT))" git apply --unidiff-zero beads/test/s1_timing/application.patch
	cd "$(OUT)/source" && $(MAKE) -j$(JOBS) --no-print-directory \
		-o beads/resources.cc -o beads/resources.h \
		-f beads/build/link-order.mk KIT_MAKEFILE=beads/makefile \
		BUILD_ROOT="$(abspath $(OUT))/objects/" \
		TOOLCHAIN_PATH="$(TOOLCHAIN_PATH)" KIT_GCC9=1 bin
	cp "$(OUT)/objects/beads/beads.elf" "$(OUT)/app.elf"
	cp "$(OUT)/objects/beads/beads.bin" "$(OUT)/app.bin"
ifeq ($(STOCK_REGION),)
	bash beads/build/tools/build_region.sh "$(abspath $(OUT))/region"
	cp "$(OUT)/region/region.bin" "$(OUT)/region.bin"
else
	@printf '%s  %s\n' "$$(sed -n 's/  region.bin$$//p' beads/build/expected-hashes.sha256)" \
		"$(STOCK_REGION)" | sha256sum -c -
	cp "$(STOCK_REGION)" "$(OUT)/region.bin"
endif
	python3 beads/test/s1_timing/image.py "$(OUT)" "$(TOOLCHAIN_PATH)bin/arm-none-eabi-nm"
	cd "$(OUT)" && sha256sum app.elf app.bin region.bin image.bin receipt.json > SHA256SUMS

.PHONY: verify
verify:
	cd "$(OUT)" && sha256sum -c "$(CURDIR)/beads/test/s1_timing/expected-hashes.sha256"
