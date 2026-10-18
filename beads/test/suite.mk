# Copyright 2026 Insolent Construct. MIT; see beads/build/LICENSE.
PACKAGES = beads/test/stubs beads/drivers beads/dsp beads/test stmlib/utils stmlib/dsp beads
VPATH = $(PACKAGES)
BUILD_ROOT ?= build/
BUILD_DIR = $(BUILD_ROOT)$(notdir $(TARGET))/
OBJS = $(addprefix $(BUILD_DIR),$(CC_FILES:.cc=.o))
SANITIZERS ?= -fsanitize=address,undefined,integer-divide-by-zero -fno-sanitize-recover=all -fno-pie
CPPFLAGS = -DTEST $(SUITE_FLAGS) $(REFERENCE_INCLUDE) $(addprefix -I,$(STUBS)) -Ibeads/test/stubs -I.
CXXFLAGS = -Os -g -Wall -Werror -Wno-unused-local-typedef -Wno-unused-local-typedefs $(SANITIZERS)
LDFLAGS = $(SANITIZERS) $(if $(SANITIZERS),-no-pie)

all: $(TARGET)

$(BUILD_DIR):
	mkdir -p $@

$(BUILD_DIR)%.o: %.cc | $(BUILD_DIR)
	g++ -c $(CPPFLAGS) $(CXXFLAGS) $(EXTRA_FLAGS) -MMD -MP $< -o $@

$(TARGET): $(OBJS)
	g++ -o $@ $(OBJS) $(LDFLAGS)

.PHONY: test
test: $(TARGET)
	ASAN_OPTIONS=detect_leaks=0 $(abspath $(TARGET))

-include $(OBJS:.o=.d)
