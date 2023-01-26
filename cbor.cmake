# SPDX-License-Identifier: MIT

list(APPEND CBOR_SRCS
	${CMAKE_CURRENT_LIST_DIR}/src/common.c
	${CMAKE_CURRENT_LIST_DIR}/src/parser.c
	${CMAKE_CURRENT_LIST_DIR}/src/decoder.c
	${CMAKE_CURRENT_LIST_DIR}/src/encoder.c
	${CMAKE_CURRENT_LIST_DIR}/src/helper.c
	${CMAKE_CURRENT_LIST_DIR}/src/ieee754.c
)
list(APPEND CBOR_INCS ${CMAKE_CURRENT_LIST_DIR}/include)
