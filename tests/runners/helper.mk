# SPDX-License-Identifier: MIT

COMPONENT_NAME = helper

SRC_FILES = \
	../src/helper.c \
	../src/decoder.c \
	../src/parser.c \
	../src/common.c \

TEST_SRC_FILES = \
	src/helper_test.cpp \
	src/test_all.cpp \

INCLUDE_DIRS = \
	../include \
	$(CPPUTEST_HOME)/include \

MOCKS_SRC_DIRS =
CPPUTEST_CPPFLAGS =

include MakefileRunner.mk
