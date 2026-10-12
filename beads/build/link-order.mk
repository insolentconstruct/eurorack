# Additive wrapper: upstream recipes and flags remain unchanged.
include $(KIT_MAKEFILE)
# Extension groups first, then declared package order, then alphabetic filenames.
# Do not globally sort the entire source/object list.
CC_FILES = $(notdir $(foreach p,$(PACKAGES),$(sort $(wildcard $(p)/*.cc))))
C_FILES = $(notdir $(foreach p,$(PACKAGES),$(sort $(wildcard $(p)/*.c)))) $(sort $(CORE_C_FILES)) $(sort $(DRIVERS_C_FILES))
AS_FILES = $(notdir $(foreach p,$(PACKAGES),$(sort $(wildcard $(p)/*.s))))
ifneq ($(words $(OBJ_FILES)),$(words $(sort $(OBJ_FILES))))
$(error Duplicate object basenames: cannot safely use upstream VPATH)
endif
ifneq ($(filter $(notdir $(STARTUP_OBJ)),$(OBJ_FILES)),)
$(error Startup appears in package objects)
endif
.PHONY: kit-info
kit-info:
	@printf '%s\n' '$(OBJS)' > '$(BUILD_DIR)objects.txt'
	@printf '%s\n' '$(CC) $(LDFLAGS) -o $(TARGET_ELF) $(OBJS)' > '$(BUILD_DIR)link.txt'

# Complete dependency inventory for notices, including toolchain/system headers.
# This supplements the upstream -MM build dependencies; compile flags are identical.
.PHONY: notice-deps
notice-deps: $(OBJS:.o=.nd)
$(BUILD_DIR)%.nd: %.s
	$(CC) -c -x assembler-with-cpp -M $(ASFLAGS) $< -MF $@ -MT $(@:.nd=.o)
$(BUILD_DIR)%.nd: %.c
	$(CC) -M $(CCFLAGS) $< -MF $@ -MT $(@:.nd=.o)
$(BUILD_DIR)%.nd: %.cc
	$(CXX) -M $(CCFLAGS) $(CPPFLAGS) $< -MF $@ -MT $(@:.nd=.o)
