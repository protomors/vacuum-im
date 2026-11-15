# Only the Stringprep/NFKC API used by JIDs, with upstream's UTF-8 validator.
add_library(idn STATIC stringprep.c profiles.c rfc3454.c nfkc.c u8-check.c)
target_include_directories(idn PUBLIC "${CMAKE_CURRENT_SOURCE_DIR}"
    PRIVATE "${CMAKE_CURRENT_SOURCE_DIR}/support")
target_compile_definitions(idn PUBLIC LIBIDN_STATIC PRIVATE LIBIDN_BUILDING HAVE_CONFIG_H)
set_target_properties(idn PROPERTIES POSITION_INDEPENDENT_CODE ON
    ARCHIVE_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/src/libs")
