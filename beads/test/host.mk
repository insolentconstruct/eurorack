# Copyright 2026 Insolent Construct.
# Author: Insolent Construct
# Shared native test rules; program makefiles retain their sanitizer flags.

OBJ_FILES = $(CC_FILES:.cc=.o)
OBJS = $(patsubst %,$(BUILD_DIR)%,$(OBJ_FILES)) $(STARTUP_OBJ)
DEPS = $(OBJS:.o=.d)
DEP_FILE = $(BUILD_DIR)depends.mk
.DEFAULT_GOAL := all

all: $(TARGET)

$(BUILD_DIR):
	mkdir -p $(BUILD_DIR)

$(BUILD_DIR)%.o: %.cc
	g++ -c $(EXTRA_FLAGS) $(HOST_FLAGS) $< -o $@

$(BUILD_DIR)%.d: %.cc
	g++ -MM -MP $(EXTRA_FLAGS) $(HOST_FLAGS) $< -MF $@ -MT $(@:.d=.o)

$(TARGET): $(OBJS)
	g++ -o $(TARGET) $(OBJS) $(HOST_LDFLAGS)

depends: $(DEPS)
	cat $(DEPS) > $(DEP_FILE)

$(DEP_FILE): $(BUILD_DIR) $(DEPS)
	cat $(DEPS) > $(DEP_FILE)

.PHONY: test depends
# Kept separate so programs may append an advanced configuration.
test: run-test
.PHONY: run-test
run-test: $(TARGET)
	ASAN_OPTIONS=detect_leaks=0 ./$(TARGET)

include $(DEP_FILE)
