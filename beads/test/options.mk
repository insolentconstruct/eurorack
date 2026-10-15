PACKAGES       =  beads/drivers beads/dsp beads/test stmlib/utils stmlib/dsp beads

VPATH          = $(PACKAGES)

ADVANCED       =
TARGET         = options_test$(ADVANCED)
BUILD_ROOT     = build/
BUILD_DIR      = $(BUILD_ROOT)$(TARGET)/
CC_FILES       = options_test.cc \
		ui.cc \
		settings.cc \
		switches.cc
OBJ_FILES      = $(CC_FILES:.cc=.o)
OBJS           = $(patsubst %,$(BUILD_DIR)%,$(OBJ_FILES)) $(STARTUP_OBJ)
DEPS           = $(OBJS:.o=.d)
DEP_FILE       = $(BUILD_DIR)depends.mk

all:  $(TARGET)

$(BUILD_DIR):
	mkdir -p $(BUILD_DIR)

$(BUILD_DIR)%.o: %.cc
	g++ -c -DTEST -Os -g -Wall -Werror -Wno-unused-local-typedef -Wno-unused-local-typedefs -DSTM32H7XX -ffp-contract=off -Wno-int-to-pointer-cast -Ibeads/test/options_stubs -I. $(if $(ADVANCED),-DENABLE_ADVANCED_FEATURES) $< -o $@

$(BUILD_DIR)%.d: %.cc
	g++ -MM -DTEST -DSTM32H7XX -ffp-contract=off -Wno-int-to-pointer-cast -Ibeads/test/options_stubs -I. $(if $(ADVANCED),-DENABLE_ADVANCED_FEATURES) $< -MF $@ -MT $(@:.d=.o)

$(TARGET):  $(OBJS)
	g++ -o $(TARGET) $(OBJS)

depends:  $(DEPS)
	cat $(DEPS) > $(DEP_FILE)

$(DEP_FILE):  $(BUILD_DIR) $(DEPS)
	cat $(DEPS) > $(DEP_FILE)

.PHONY: test
test:  $(TARGET)
	./$(TARGET)
ifeq ($(ADVANCED),)
	$(MAKE) -f beads/test/options.mk ADVANCED=_advanced test
endif

include $(DEP_FILE)
