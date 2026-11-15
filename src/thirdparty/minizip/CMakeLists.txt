include("${CMAKE_SOURCE_DIR}/src/make/config.cmake")

project(minizip C)

file(GLOB SOURCES "*.c")

add_library(minizip STATIC ${SOURCES})
target_include_directories(minizip PUBLIC "${CMAKE_CURRENT_SOURCE_DIR}")
target_link_libraries(minizip PUBLIC Vacuum::Zlib)
set_target_properties(minizip PROPERTIES ARCHIVE_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/src/libs"
	COMPILE_FLAGS "${CMAKE_SHARED_LIBRARY_C_FLAGS}"
	LINK_FLAGS "${CMAKE_SHARED_LIBRARY_C_FLAGS}")
