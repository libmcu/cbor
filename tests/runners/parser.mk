COMPONENT_NAME = parser

SRC_FILES = \
	../src/parser.c \
	../src/decoder.c \
	../src/helper.c \
	../src/common.c \

TEST_SRC_FILES = \
	src/parser_test.cpp \
	src/test_all.cpp \

INCLUDE_DIRS = \
	../include \
	$(CPPUTEST_HOME)/include \

MOCKS_SRC_DIRS =
CPPUTEST_CPPFLAGS =

include MakefileRunner.mk
