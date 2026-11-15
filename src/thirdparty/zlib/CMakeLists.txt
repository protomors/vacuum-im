add_library(zlib STATIC
    adler32.c compress.c crc32.c deflate.c gzclose.c gzlib.c gzread.c gzwrite.c
    infback.c inffast.c inflate.c inftrees.c trees.c uncompr.c zutil.c)
target_include_directories(zlib PUBLIC "${CMAKE_CURRENT_SOURCE_DIR}")
if(UNIX)
    target_compile_definitions(zlib PUBLIC HAVE_UNISTD_H=1)
endif()
set_target_properties(zlib PROPERTIES POSITION_INDEPENDENT_CODE ON
    ARCHIVE_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/src/libs")
