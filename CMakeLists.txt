cmake_minimum_required(VERSION 3.21)

project(vacuum-im)

set(CMAKE_INCLUDE_CURRENT_DIR ON)

if(APPLE)
	 set(CMAKE_OSX_DEPLOYMENT_TARGET "10.13")
	 set_property(GLOBAL PROPERTY ALL_PLUGINS_FULLPATHS "")
endif()

include("src/make/qt.cmake")

add_custom_target(updatets)
add_custom_target(translations)

include("src/make/config.cmake")
include("src/translations/languages.cmake")
include_directories("${CMAKE_SOURCE_DIR}/src")

if (APPLE)
	file(COPY "resources/menuicons" DESTINATION "${INSTALL_RESOURCES}")
else (APPLE)
	install(DIRECTORY "resources/menuicons" DESTINATION "${INSTALL_RESOURCES}")
endif (APPLE)

include("src/packages/package.cmake")

set(INSTALL_SDK YES CACHE BOOL "Install files needed to build custom plugins")
option(BUILD_TRANSLATION_TOOLS "Build the autotranslate and txprepare maintainer utilities" OFF)
set(INSTALL_DOCS YES CACHE BOOL "Install project documentation")
set(LANGS "" CACHE STRING "List of languages to build localization for (this variable overrides any of LANG_*)")
set(USED_LANGS "" CACHE INTERNAL "List of languages actually used for generating targets")
add_lang_options()

set(FORCE_BUNDLED_MINIZIP NO CACHE BOOL "Use bundled minizip even if local system have one")

find_library(SYSTEM_ZLIB_FOUND z)
find_library(SYSTEM_IDN_FOUND idn)
find_library(SYSTEM_MINIZIP_FOUND minizip)

if (FORCE_BUNDLED_MINIZIP)
	set(SYSTEM_MINIZIP_FOUND NO)
endif (FORCE_BUNDLED_MINIZIP)

if (SYSTEM_ZLIB_FOUND)
	add_definitions(-DUSE_SYSTEM_ZLIB)
endif (SYSTEM_ZLIB_FOUND)

if (SYSTEM_IDN_FOUND)
	add_definitions(-DUSE_SYSTEM_IDN)
endif (SYSTEM_IDN_FOUND)

if (SYSTEM_MINIZIP_FOUND)
	add_definitions(-DUSE_SYSTEM_MINIZIP)
endif (SYSTEM_MINIZIP_FOUND)

if ("${LANGS}" STREQUAL "")
	process_lang_options()
else ("${LANGS}" STREQUAL "")
	set_enabled_langs("${LANGS}")
endif ("${LANGS}" STREQUAL "")

if (INSTALL_DOCS)
	install(FILES "AUTHORS" "CHANGELOG" "README" "INSTALL" "COPYING" "TRANSLATORS"
		DESTINATION "${INSTALL_DOCUMENTS}")
endif (INSTALL_DOCS)

add_subdirectory(src)


if (INSTALL_SDK)
	cpack_add_component(sdk
		DISPLAY_NAME "SDK"
		DESCRIPTION "Files needed to build custom plugins"
		DISABLED)
endif (INSTALL_SDK)
