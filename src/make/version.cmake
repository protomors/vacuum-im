# Keep the application header authoritative for application and package versions.
set_property(DIRECTORY APPEND PROPERTY CMAKE_CONFIGURE_DEPENDS
    "${CMAKE_CURRENT_LIST_DIR}/../definitions/version.h")
file(STRINGS "${CMAKE_CURRENT_LIST_DIR}/../definitions/version.h" _version_line
    REGEX "^#define[ \t]+CLIENT_VERSION[ \t]+\"")
string(REGEX MATCH "\"([0-9]+\\.[0-9]+\\.[0-9]+)\"" _match "${_version_line}")
if(NOT _match)
    message(FATAL_ERROR "Cannot read CLIENT_VERSION from src/definitions/version.h")
endif()
set(VACUUM_VERSION "${CMAKE_MATCH_1}")
file(STRINGS "${CMAKE_CURRENT_LIST_DIR}/../definitions/version.h" _suffix_line
    REGEX "^#define[ \t]+CLIENT_VERSION_SUFFIX[ \t]+\"")
string(REGEX MATCH "\"([^\"]*)\"" _match "${_suffix_line}")
string(TOLOWER "${CMAKE_MATCH_1}" VACUUM_VERSION_SUFFIX)

set(GIT_HASH "" CACHE STRING "Source commit (detected from Git when empty)")
set(GIT_DATE "" CACHE STRING "Source commit Unix timestamp (detected from Git when empty)")
find_package(Git QUIET)
if(GIT_FOUND)
    execute_process(COMMAND "${GIT_EXECUTABLE}" symbolic-ref -q HEAD
        WORKING_DIRECTORY "${CMAKE_CURRENT_LIST_DIR}/../.."
        OUTPUT_VARIABLE _head_ref OUTPUT_STRIP_TRAILING_WHITESPACE ERROR_QUIET)
    foreach(_git_file HEAD logs/HEAD packed-refs ${_head_ref})
        execute_process(COMMAND "${GIT_EXECUTABLE}" rev-parse --git-path "${_git_file}"
            WORKING_DIRECTORY "${CMAKE_CURRENT_LIST_DIR}/../.."
            OUTPUT_VARIABLE _git_path OUTPUT_STRIP_TRAILING_WHITESPACE ERROR_QUIET)
        if(_git_path)
            get_filename_component(_git_path "${_git_path}" ABSOLUTE
                BASE_DIR "${CMAKE_CURRENT_LIST_DIR}/../..")
            if(EXISTS "${_git_path}")
                set_property(DIRECTORY APPEND PROPERTY CMAKE_CONFIGURE_DEPENDS "${_git_path}")
            endif()
        endif()
    endforeach()
    if(GIT_HASH STREQUAL "")
        execute_process(COMMAND "${GIT_EXECUTABLE}" log -1 --format=%H
            WORKING_DIRECTORY "${CMAKE_CURRENT_LIST_DIR}/../.."
            OUTPUT_VARIABLE GIT_HASH OUTPUT_STRIP_TRAILING_WHITESPACE ERROR_QUIET)
    endif()
    if(GIT_DATE STREQUAL "")
        execute_process(COMMAND "${GIT_EXECUTABLE}" log -1 --format=%ct
            WORKING_DIRECTORY "${CMAKE_CURRENT_LIST_DIR}/../.."
            OUTPUT_VARIABLE GIT_DATE OUTPUT_STRIP_TRAILING_WHITESPACE ERROR_QUIET)
    endif()
endif()
