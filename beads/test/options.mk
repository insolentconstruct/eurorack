TARGET = options_test$(ADVANCED)
CC_FILES       = 		options_test.cc \
		ui.cc \
		options.cc \
		settings.cc \
		switches.cc

STUBS = beads/test/options_stubs beads/test/common_stubs
HOST_SANITIZERS = 0
HOST_FLAGS = -DSTM32H7XX -ffp-contract=off -Wno-int-to-pointer-cast $(if $(ADVANCED),-DENABLE_ADVANCED_FEATURES)
include beads/test/host.mk
ifeq ($(ADVANCED),)
test:
	$(MAKE) -f beads/test/options.mk ADVANCED=_advanced test
endif
