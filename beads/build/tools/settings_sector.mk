# Copyright 2026 Insolent Construct.
# Author: Insolent Construct

TARGET = build/settings_sector_check

# -funsigned-char: stmlib's checksum sums chars, unsigned on the Cortex-M7.

$(TARGET): beads/build/tools/settings_sector_check.cc beads/settings.h \
		stmlib/system/sector_storage_h7xx.h
	mkdir -p build
	g++ -DTEST -DSTM32H7XX -Ibeads/test/common_stubs -std=c++98 -Wall -Werror -I. -funsigned-char \
		-Wno-int-to-pointer-cast \
		-fsanitize=address,undefined,integer-divide-by-zero \
		-fno-sanitize-recover=all -fno-pie $< -no-pie -o $@

.PHONY: test
test: $(TARGET)
	ASAN_OPTIONS=detect_leaks=0 python3 beads/build/tools/settings_sector_test.py
