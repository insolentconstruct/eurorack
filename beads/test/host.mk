# Copyright 2026 Insolent Construct.
# Author: Insolent Construct
# Shared native test rules; program makefiles retain their sanitizer flags.

PACKAGES = beads/drivers beads/dsp beads/test stmlib/utils stmlib/dsp beads
VPATH = $(PACKAGES) beads/dsp/fx beads/dsp/pvoc $(PLAITS_PACKAGES)
BUILD_ROOT ?= build/
BUILD_DIR = $(BUILD_ROOT)$(TARGET)/
HOST_SANITIZERS ?= 1
# Favor sanitizer runtime over code size in native tests (not firmware builds).
HOST_FLAGS += -DTEST -O1 -g -Wall -Werror -Wno-unused-local-typedefs \
	$(addprefix -I,$(STUBS)) -I.
ifeq ($(HOST_SANITIZERS),1)
HOST_FLAGS += -fsanitize=address,undefined,integer-divide-by-zero \
	-fno-sanitize-recover=all -fno-pie
HOST_LDFLAGS += -fsanitize=address,undefined,integer-divide-by-zero -no-pie
endif

# The modal resonator in the processor's options block reads Rings' tables.
ifneq ($(filter processor_options.cc,$(CC_FILES)),)
CC_FILES += processor_resonator.cc modal_resonator.cc rings_resources.cc
endif

ifneq ($(filter settings.cc,$(CC_FILES)),)
CC_FILES += profile.cc
ifeq ($(filter pitch_quantizer.cc,$(CC_FILES)),)
CC_FILES += pitch_quantizer.cc
endif
ifeq ($(filter random.cc,$(CC_FILES)),)
CC_FILES += random.cc
endif
endif

OBJ_FILES = $(CC_FILES:.cc=.o)
OBJS = $(patsubst %,$(BUILD_DIR)%,$(OBJ_FILES)) $(STARTUP_OBJ)
DEPS = $(OBJS:.o=.d)
DEP_FILE = $(BUILD_DIR)depends.mk
.DEFAULT_GOAL := all

all: $(TARGET)

$(BUILD_DIR):
	mkdir -p $(BUILD_DIR)

$(BUILD_DIR)%.o: %.cc | $(BUILD_DIR)
	g++ -c $(HOST_FLAGS) $(EXTRA_FLAGS) $< -o $@

$(BUILD_DIR)%.d: %.cc | $(BUILD_DIR)
	g++ -MM -MP $(HOST_FLAGS) $(EXTRA_FLAGS) $< -MF $@ -MT $(@:.d=.o)

$(TARGET): $(OBJS)
	g++ -o $(TARGET) $(OBJS) $(HOST_LDFLAGS)

depends: $(DEPS)
	cat $(DEPS) > $(DEP_FILE)

$(DEP_FILE): $(DEPS) | $(BUILD_DIR)
	cat $(DEPS) > $(DEP_FILE)

.PHONY: test depends
# Kept separate so programs may append an advanced configuration.
test: run-test
.PHONY: run-test
run-test: $(TARGET)
	ASAN_OPTIONS=detect_leaks=0 ./$(TARGET) $(TEST_ARGS)

include $(DEP_FILE)
