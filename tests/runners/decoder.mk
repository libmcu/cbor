# SPDX-License-Identifier: MIT

COMPONENT_NAME = decoder

SRC_FILES = \
	../src/decoder.c \
	../src/parser.c \
	../src/common.c \

TEST_SRC_FILES = \
	src/decoder_test.cpp \
	src/parser_count_test.cpp \
	src/tool_item_count_test.cpp \
	src/test_all.cpp \

INCLUDE_DIRS = \
	../include \
	$(CPPUTEST_HOME)/include \

MOCKS_SRC_DIRS =
CPPUTEST_CPPFLAGS += \
	-DCBOR_PROJECT_ROOT=\"$(abspath ..)\"

include MakefileRunner.mk
