# SPDX-License-Identifier: MIT

COMPONENT_NAME = stream

SRC_FILES = \
	../src/stream.c \
	../src/common.c \
	../src/ieee754.c \

TEST_SRC_FILES = \
	src/stream_test.cpp \
	src/test_all.cpp \

INCLUDE_DIRS = \
	../include \
	$(CPPUTEST_HOME)/include \

MOCKS_SRC_DIRS =

include MakefileRunner.mk
