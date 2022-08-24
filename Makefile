PROJECT := mycbor
BASEDIR := $(shell pwd)
BUILDIR := build

VERBOSE ?= 0
V ?= $(VERBOSE)
ifeq ($(V), 0)
	Q := @
else
	Q :=
endif
export BASEDIR
export BUILDIR
export Q

CBOR_ROOT := ./
include $(CBOR_ROOT)/cbor.mk

SRCS := $(CBOR_SRCS)
INCS := $(CBOR_INCS)
OBJS := $(addprefix $(BUILDIR)/, $(SRCS:.c=.o))
DEPS := $(OBJS:.o=.d)

ifneq ($(CROSS_COMPILE),)
	CROSS_COMPILE_PREFIX := $(CROSS_COMPILE)-
endif
CC := $(CROSS_COMPILE_PREFIX)gcc
LD := $(CROSS_COMPILE_PREFIX)ld
SZ := $(CROSS_COMPILE_PREFIX)size
AR := $(CROSS_COMPILE_PREFIX)ar
OC := $(CROSS_COMPILE_PREFIX)objcopy
OD := $(CROSS_COMPILE_PREFIX)objdump
NM := $(CROSS_COMPILE_PREFIX)nm

CFLAGS := \
	-std=c99 \
	-static \
	-ffreestanding \
	-fno-builtin \
	-fno-common \
	-ffunction-sections \
	-fdata-sections \
	-fstack-usage \
	-Os \
	\
	-Werror \
	-Wall \
	-Wextra \
	-Wc++-compat \
	-Wformat=2 \
	-Wmissing-prototypes \
	-Wstrict-prototypes \
	-Wmissing-declarations \
	-Wcast-align \
	-Wpointer-arith \
	-Wbad-function-cast \
	-Wcast-qual \
	-Wmissing-format-attribute \
	-Wmissing-include-dirs \
	-Wformat-nonliteral \
	-Wdouble-promotion \
	-Wfloat-equal \
	-Winline \
	-Wundef \
	-Wunused-macros \
	-Wshadow \
	-Wwrite-strings \
	-Waggregate-return \
	-Wredundant-decls \
	-Wconversion \
	-Wstrict-overflow=5 \
	-Wno-long-long \
	-Wswitch-default \
	-Wpedantic \
	\
	-Wno-error=pedantic

ifneq ($(CROSS_COMPILE),)
CFLAGS += \
	-mcpu=cortex-m0 \
	-mthumb \
	-mabi=aapcs
endif

VERSION ?= $(shell git describe --long --tags --dirty --always)
version-list := $(subst -, , $(VERSION))
VERSION_TAG := $(strip $(word 1, $(version-list)))

all: $(OBJS)
	$(Q)$(SZ) -t --common $(sort $(OBJS))
	@echo $(VERSION_TAG) > $(BUILDIR)/version.txt

$(BUILDIR)/%.o: %.c $(MAKEFILE_LIST)
	$(info compiling   $<)
	@mkdir -p $(@D)
	$(Q)$(CC) -o $@ -c $*.c -MMD \
		$(addprefix -D, $(DEFS)) \
		$(addprefix -I, $(INCS)) \
		$(CFLAGS)

.PHONY: test fuzz
test:
	$(Q)$(MAKE) -C tests
fuzz:
	$(Q)clang++ -g -fsanitize=address,fuzzer \
		-o tests/build/fuzz_testing \
		$(addprefix -D, $(DEFS)) \
		$(addprefix -I, $(INCS)) \
		$(SRCS) tests/src/parser_fuzz_test.c
	$(Q)./tests/build/fuzz_testing
.PHONY: coverage
coverage:
	$(Q)$(MAKE) -C tests $@
.PHONY: clean
clean:
	$(Q)$(MAKE) -C tests clean
	$(Q)rm -rf $(BUILDIR)
