TARGET = gain_test
CC_FILES = gain_test.cc gain_control.cc
STUBS = beads/test/gain_stubs
include beads/test/suite.mk
