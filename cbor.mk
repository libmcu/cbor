ifneq ($(CBOR_ROOT),)
cbor-basedir := $(CBOR_ROOT)/
endif

CBOR_SRCS := \
	$(cbor-basedir)src/common.c \
	$(cbor-basedir)src/parser.c \
	$(cbor-basedir)src/decoder.c \
	$(cbor-basedir)src/encoder.c \
	$(cbor-basedir)src/helper.c \
	$(cbor-basedir)src/ieee754.c \

CBOR_INCS := $(cbor-basedir)include
