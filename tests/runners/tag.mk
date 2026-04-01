# SPDX-License-Identifier: MIT

COMPONENT_NAME = tag

SRC_FILES = \
	../src/common.c \
	../src/encoder.c \
	../src/ieee754.c \
	../src/parser.c \
	../src/decoder.c \
	../src/helper.c \

TEST_SRC_FILES = \
	src/tag_test.cpp \
	src/test_all.cpp \

INCLUDE_DIRS = \
	../include \
	$(CPPUTEST_HOME)/include \

MOCKS_SRC_DIRS =
CPPUTEST_CPPFLAGS =

include MakefileRunner.mk
