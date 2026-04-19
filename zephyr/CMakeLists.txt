# SPDX-License-Identifier: MIT

if(CONFIG_LIBMCU_CBOR)
  include(${CMAKE_CURRENT_LIST_DIR}/../cbor.cmake)
  zephyr_library()
  zephyr_library_sources(${CBOR_SRCS})
  zephyr_include_directories(${CBOR_INCS})
endif()
