PACKAGES       =  beads/dsp beads/test stmlib/utils stmlib/dsp beads

VPATH          = $(PACKAGES)

TARGET         = granular_test
BUILD_ROOT     = build/
BUILD_DIR      = $(BUILD_ROOT)$(TARGET)/
CC_FILES       = 		granular_test.cc \
		resources.cc \
		units.cc \
		random.cc
OBJ_FILES      = $(CC_FILES:.cc=.o)
OBJS           = $(patsubst %,$(BUILD_DIR)%,$(OBJ_FILES)) $(STARTUP_OBJ)
DEPS           = $(OBJS:.o=.d)
DEP_FILE       = $(BUILD_DIR)depends.mk

all:  granular_test

$(BUILD_DIR):
	mkdir -p $(BUILD_DIR)

$(BUILD_DIR)%.o: %.cc
	g++ -c -DTEST -Os -g -Wall -Werror -Wno-unused-local-typedef \
		-Wno-unused-local-typedefs -I. \
		-fsanitize=address,undefined,integer-divide-by-zero \
		-fno-sanitize-recover=all -fno-pie $< -o $@

$(BUILD_DIR)%.d: %.cc
	g++ -MM -DTEST -I. -fsanitize=address,undefined,integer-divide-by-zero \
		-fno-sanitize-recover=all -fno-pie $< -MF $@ -MT $(@:.d=.o)

granular_test:  $(OBJS)
	g++ -o $(TARGET) $(OBJS) \
		-fsanitize=address,undefined,integer-divide-by-zero -no-pie

depends:  $(DEPS)
	cat $(DEPS) > $(DEP_FILE)

$(DEP_FILE):  $(BUILD_DIR) $(DEPS)
	cat $(DEPS) > $(DEP_FILE)

.PHONY: test
test:  $(TARGET)
	ASAN_OPTIONS=detect_leaks=0 ./$(TARGET)

include $(DEP_FILE)
