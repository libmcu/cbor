# SPDX-License-Identifier: MIT

COMPONENT_NAME = encoder

SRC_FILES = \
	../src/common.c \
	../src/encoder.c \
	../src/ieee754.c \

TEST_SRC_FILES = \
	src/encoder_test.cpp \
	src/test_all.cpp \

INCLUDE_DIRS = \
	../include \
	$(CPPUTEST_HOME)/include \

MOCKS_SRC_DIRS =
CPPUTEST_CPPFLAGS =

include MakefileRunner.mk
