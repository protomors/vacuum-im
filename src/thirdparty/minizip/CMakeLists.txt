add_library(minizip STATIC ioapi.c mztools.c unzip.c zip.c)
if(WIN32)
    target_sources(minizip PRIVATE iowin32.c)
endif()
target_include_directories(minizip PUBLIC "${CMAKE_CURRENT_SOURCE_DIR}")
target_link_libraries(minizip PUBLIC Vacuum::Zlib)
set_target_properties(minizip PROPERTIES POSITION_INDEPENDENT_CODE ON
    ARCHIVE_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/src/libs")
