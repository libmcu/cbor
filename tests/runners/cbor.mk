COMPONENT_NAME = cbor

SRC_FILES = \
	../src/cbor.c \

TEST_SRC_FILES = \
	src/cbor_test.cpp \
	src/test_all.cpp \

INCLUDE_DIRS = \
	../include \
	$(CPPUTEST_HOME)/include \

MOCKS_SRC_DIRS =
CPPUTEST_CPPFLAGS =

include MakefileRunner.mk
