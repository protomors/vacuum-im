cmake_minimum_required(VERSION 3.21)

include("src/make/version.cmake")
project(vacuum-im VERSION "${VACUUM_VERSION}" LANGUAGES C CXX)
set(VERSION "${PROJECT_VERSION}") # also used by the macOS bundle
include(CPackComponent)

set(CMAKE_INCLUDE_CURRENT_DIR ON)

if(APPLE)
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

set(INSTALL_SDK YES CACHE BOOL "Install files needed to build custom plugins")
option(BUILD_TRANSLATION_TOOLS "Build the autotranslate and txprepare maintainer utilities" OFF)
set(INSTALL_DOCS YES CACHE BOOL "Install project documentation")
set(LANGS "" CACHE STRING "List of languages to build localization for (this variable overrides any of LANG_*)")
set(USED_LANGS "" CACHE INTERNAL "List of languages actually used for generating targets")
add_lang_options()

include("src/make/dependencies.cmake")

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

option(BUILD_TESTING "Build regression tests" OFF)
if (BUILD_TESTING)
	enable_testing()
	add_subdirectory(tests)
endif ()


if (INSTALL_SDK)
	cpack_add_component(sdk
		DISPLAY_NAME "SDK"
		DESCRIPTION "Files needed to build custom plugins"
		DISABLED)
endif (INSTALL_SDK)

include("src/packages/package.cmake")
