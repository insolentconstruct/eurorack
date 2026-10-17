TARGET = build/options_tables

$(TARGET): beads/test/options_tables.cc beads/options.h beads/dsp/playback_options.h
	mkdir -p build
	g++ -std=c++98 -Wall -Werror -I. \
		-fsanitize=address,undefined,integer-divide-by-zero \
		-fno-sanitize-recover=all -fno-pie $< -no-pie -o $@

.PHONY: test
test: $(TARGET)
	ASAN_OPTIONS=detect_leaks=0 python3 beads/test/options_tables_test.py
