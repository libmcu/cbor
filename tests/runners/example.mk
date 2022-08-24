COMPONENT_NAME = example

SRC_FILES = \
	../examples/simple.c \
	../examples/complex.c \
	../src/encoder.c \
	../src/ieee754.c \
	../src/decoder.c \
	../src/parser.c \
	../src/helper.c \
	../src/common.c \

TEST_SRC_FILES = \
	src/example_test.cpp \
	src/test_all.cpp \

INCLUDE_DIRS = \
	../examples \
	../include \
	$(CPPUTEST_HOME)/include \

MOCKS_SRC_DIRS =
CPPUTEST_CPPFLAGS = -DCBOR_RECURSION_MAX_LEVEL=8

include MakefileRunner.mk
