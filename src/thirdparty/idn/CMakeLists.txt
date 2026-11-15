include("${CMAKE_SOURCE_DIR}/src/make/config.cmake")

project(idn C)

file(GLOB SOURCES "*.c")

add_library(idn STATIC ${SOURCES})
target_include_directories(idn PUBLIC "${CMAKE_CURRENT_SOURCE_DIR}")
set_target_properties(idn PROPERTIES ARCHIVE_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/src/libs"
	COMPILE_FLAGS "${CMAKE_SHARED_LIBRARY_C_FLAGS}"
	LINK_FLAGS "${CMAKE_SHARED_LIBRARY_C_FLAGS}")
