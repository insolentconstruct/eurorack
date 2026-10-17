PACKAGES       =  beads/drivers beads/dsp beads/test stmlib/utils stmlib/dsp \
		beads

VPATH          = $(PACKAGES)

TARGET         = processor_test
BUILD_ROOT     = build/
BUILD_DIR      = $(BUILD_ROOT)$(notdir $(TARGET))/
CC_FILES       = 		processor_test.cc \
		processor.cc \
		mu_law.cc \
		delay_controller.cc \
		delay_clock.cc \
		ui.cc \
		settings.cc \
		switches.cc \
		trigger_generator.cc \
		resources.cc \
		units.cc \
		random.cc
OBJ_FILES      = $(CC_FILES:.cc=.o)
OBJS           = $(patsubst %,$(BUILD_DIR)%,$(OBJ_FILES)) $(STARTUP_OBJ)
DEPS           = $(OBJS:.o=.d)
DEP_FILE       = $(BUILD_DIR)depends.mk

all:  $(TARGET)

$(BUILD_DIR):
	mkdir -p $(BUILD_DIR)

$(BUILD_DIR)%.o: %.cc
	g++ -c -DTEST -Os -g -Wall -Werror -Wno-unused-local-typedef \
		-Wno-unused-local-typedefs -include cstdio -DSTM32H7XX \
		-Wno-int-to-pointer-cast \
		-Ibeads/test/hardware_stubs -I. \
		-fsanitize=address,undefined,integer-divide-by-zero \
		-fno-sanitize-recover=all -fno-pie $< -o $@

$(BUILD_DIR)%.d: %.cc
	g++ -MM -DTEST -include cstdio -DSTM32H7XX -Wno-int-to-pointer-cast \
		-Ibeads/test/hardware_stubs -I. \
		-fsanitize=address,undefined,integer-divide-by-zero \
		-fno-sanitize-recover=all -fno-pie $< -MF $@ -MT $(@:.d=.o)

$(TARGET):  $(OBJS)
	g++ -o $(TARGET) $(OBJS) \
		-fsanitize=address,undefined,integer-divide-by-zero -no-pie

depends:  $(DEPS)
	cat $(DEPS) > $(DEP_FILE)

$(DEP_FILE):  $(BUILD_DIR) $(DEPS)
	cat $(DEPS) > $(DEP_FILE)

.PHONY: test
test:  $(TARGET)
	ASAN_OPTIONS=detect_leaks=0 $(abspath $(TARGET))

include $(DEP_FILE)
