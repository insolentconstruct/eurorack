PACKAGES       =  beads/dsp beads/test stmlib/utils stmlib/dsp beads

VPATH          = $(PACKAGES)

TARGET         = backup_test
BUILD_ROOT     = build/
BUILD_DIR      = $(BUILD_ROOT)$(TARGET)/
CC_FILES       = backup_test.cc \
		background_buffer_backup.cc \
		settings.cc
OBJ_FILES      = $(CC_FILES:.cc=.o)
OBJS           = $(patsubst %,$(BUILD_DIR)%,$(OBJ_FILES)) $(STARTUP_OBJ)
DEPS           = $(OBJS:.o=.d)
DEP_FILE       = $(BUILD_DIR)depends.mk

all:  backup_test

$(BUILD_DIR):
	mkdir -p $(BUILD_DIR)

$(BUILD_DIR)%.o: %.cc
	g++ -c -DTEST -Os -g -Wall -Werror -Wno-unused-local-typedef -Wno-unused-local-typedefs -DSTM32H7XX -ffp-contract=off -Wno-int-to-pointer-cast -Ibeads/test/backup_stubs -I. $< -o $@

$(BUILD_DIR)%.d: %.cc
	g++ -MM -DTEST -DSTM32H7XX -ffp-contract=off -Wno-int-to-pointer-cast -Ibeads/test/backup_stubs -I. $< -MF $@ -MT $(@:.d=.o)

backup_test:  $(OBJS)
	g++ -o $(TARGET) $(OBJS)

depends:  $(DEPS)
	cat $(DEPS) > $(DEP_FILE)

$(DEP_FILE):  $(BUILD_DIR) $(DEPS)
	cat $(DEPS) > $(DEP_FILE)

.PHONY: test
test:  $(TARGET)
	./$(TARGET)

include $(DEP_FILE)
