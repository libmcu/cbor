cmake_minimum_required(VERSION 3.16)

project(cbor LANGUAGES C CXX)

include(${CMAKE_CURRENT_LIST_DIR}/cbor.cmake)

add_library(${PROJECT_NAME} STATIC ${CBOR_SRCS})

target_compile_features(${PROJECT_NAME} PRIVATE c_std_99)

target_include_directories(${PROJECT_NAME}
	PUBLIC ${CBOR_INCS}
)

if(CMAKE_CURRENT_SOURCE_DIR STREQUAL CMAKE_SOURCE_DIR)
	# TODO: build for tests
endif()
