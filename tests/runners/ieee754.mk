# SPDX-License-Identifier: MIT

COMPONENT_NAME = ieee754

SRC_FILES = \
	../src/ieee754.c \

TEST_SRC_FILES = \
	src/ieee754_test.cpp \
	src/test_all.cpp \

INCLUDE_DIRS = \
	../include \
	$(CPPUTEST_HOME)/include \

MOCKS_SRC_DIRS =
CPPUTEST_CPPFLAGS =

include MakefileRunner.mk
