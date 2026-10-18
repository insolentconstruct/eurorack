# Copyright 2026 Insolent Construct.
# Author: Insolent Construct

TARGET = build/options_tables

$(TARGET): beads/build/tools/options_tables.cc beads/options.cc \
		beads/options.h beads/dsp/playback_options.h
	mkdir -p build
	g++ -DTEST -DSTM32H7XX -Ibeads/test/common_stubs -std=c++98 -Wall -Werror -I. \
		-fsanitize=address,undefined,integer-divide-by-zero \
		-ffunction-sections -fdata-sections -Wl,--gc-sections \
		-fno-sanitize-recover=all -fno-pie $< beads/options.cc -no-pie -o $@

.PHONY: test
test: $(TARGET)
	ASAN_OPTIONS=detect_leaks=0 python3 beads/build/tools/options_tables_test.py
