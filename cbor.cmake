list(APPEND CBOR_SRCS
	${CBOR_ROOT}/src/common.c
	${CBOR_ROOT}/src/parser.c
	${CBOR_ROOT}/src/decoder.c
	${CBOR_ROOT}/src/encoder.c
	${CBOR_ROOT}/src/helper.c
	${CBOR_ROOT}/src/ieee754.c
)
list(APPEND CBOR_INCS ${CBOR_ROOT}/include)
