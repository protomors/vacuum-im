add_library(hunspell STATIC
    affentry.cxx affixmgr.cxx csutil.cxx filemgr.cxx hashmgr.cxx hunspell.cxx
    hunspelltrace.cxx hunzip.cxx phonet.cxx replist.cxx suggestmgr.cxx)
target_compile_features(hunspell PUBLIC cxx_std_14)
target_include_directories(hunspell PUBLIC "${CMAKE_CURRENT_SOURCE_DIR}")
target_compile_definitions(hunspell PUBLIC HUNSPELL_STATIC PRIVATE BUILDING_LIBHUNSPELL)
set_target_properties(hunspell PROPERTIES POSITION_INDEPENDENT_CODE ON
    ARCHIVE_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/src/libs")
