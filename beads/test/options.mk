PACKAGES       =  beads/drivers beads/dsp beads/test stmlib/utils stmlib/dsp \
		beads

VPATH          = $(PACKAGES)

ADVANCED       =
TARGET         = options_test$(ADVANCED)
BUILD_ROOT     = build/
BUILD_DIR      = $(BUILD_ROOT)$(TARGET)/
CC_FILES       = 		options_test.cc \
		ui.cc \
		options.cc \
		settings.cc \
		switches.cc
HOST_FLAGS = \
	-DTEST -Os -g -Wall -Werror -Wno-unused-local-typedefs -DSTM32H7XX -ffp-contract=off \
	-Wno-int-to-pointer-cast -Ibeads/test/options_stubs -Ibeads/test/common_stubs -I. $(if $(ADVANCED),-DENABLE_ADVANCED_FEATURES)
HOST_LDFLAGS =
include beads/test/host.mk
ifeq ($(ADVANCED),)
test:
	$(MAKE) -f beads/test/options.mk ADVANCED=_advanced test
endif
