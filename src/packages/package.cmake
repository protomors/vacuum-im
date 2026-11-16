# Archive packaging uses the same install rules as a normal staged installation.
if(WIN32)
    set(CPACK_GENERATOR ZIP)
elseif(APPLE)
    set(CPACK_GENERATOR DragNDrop)
    set(CPACK_DMG_FORMAT UDBZ)
    set(CPACK_DMG_BACKGROUND_IMAGE "${CMAKE_SOURCE_DIR}/src/packages/macosx/background.png")
    set(CPACK_DMG_VOLUME_NAME "Vacuum-IM")
    set(CPACK_DMG_DS_STORE "${CMAKE_SOURCE_DIR}/src/packages/macosx/DS_Store")
    set(CPACK_PACKAGE_ICON "${CMAKE_SOURCE_DIR}/vacuum.icns")
else()
    set(CPACK_GENERATOR TGZ)
endif()

set(CPACK_PACKAGE_NAME "Vacuum-IM")
set(CPACK_PACKAGE_VERSION "${PROJECT_VERSION}")
set(CPACK_PACKAGE_VENDOR "Vacuum-IM contributors")
set(CPACK_PACKAGE_HOMEPAGE_URL "https://github.com/protomors/vacuum-im")
set(CPACK_RESOURCE_FILE_LICENSE "${CMAKE_SOURCE_DIR}/COPYING")
set(CPACK_RESOURCE_FILE_README "${CMAKE_SOURCE_DIR}/README")
set(CPACK_ARCHIVE_COMPONENT_INSTALL OFF)
set(CPACK_INCLUDE_TOPLEVEL_DIRECTORY ON)
set(CPACK_PACKAGE_DIRECTORY "${CMAKE_BINARY_DIR}/packages")

string(TOLOWER "${CMAKE_SYSTEM_NAME}" _package_system)
string(TOLOWER "${CMAKE_SYSTEM_PROCESSOR}" _package_arch)
if(_package_arch MATCHES "^(amd64|x64|x86_64)$")
    set(_package_arch x86_64)
endif()
set(_package_version "${PROJECT_VERSION}")
if(VACUUM_VERSION_SUFFIX)
    string(APPEND _package_version "-${VACUUM_VERSION_SUFFIX}")
endif()
if(GIT_HASH)
    string(SUBSTRING "${GIT_HASH}" 0 12 _package_revision)
    string(APPEND _package_version "-g${_package_revision}")
endif()
set(CPACK_PACKAGE_FILE_NAME "vacuum-im-${_package_version}-${_package_system}-${_package_arch}")

get_property(_enabled_plugins GLOBAL PROPERTY VACUUM_ENABLED_PLUGIN_TARGETS)
get_property(VACUUM_GSTREAMER_VERSION GLOBAL PROPERTY VACUUM_GSTREAMER_VERSION)
list(SORT _enabled_plugins)
set(_plugin_json "")
set(_installed_plugin_paths "")
foreach(plugin IN LISTS _enabled_plugins)
    if(_plugin_json)
        string(APPEND _plugin_json ",\n")
    endif()
    string(APPEND _plugin_json "    \"$<TARGET_FILE_NAME:${plugin}>\"")
    list(APPEND _installed_plugin_paths "${INSTALL_PLUGINS}/$<TARGET_FILE_NAME:${plugin}>")
endforeach()

option(INSTALL_ARCHIVE_INFO
    "Install build-info.json and RUNTIME-REQUIREMENTS.txt (portable archives only)" ON)

# Keep machine-readable release contents next to the installed documentation.
set(_build_info "${CMAKE_BINARY_DIR}/build-info-$<CONFIG>.json")
file(GENERATE OUTPUT "${_build_info}" CONTENT "{
  \"version\": \"${PROJECT_VERSION}\",
  \"suffix\": \"${VACUUM_VERSION_SUFFIX}\",
  \"commit\": \"${GIT_HASH}\",
  \"configuration\": \"$<CONFIG>\",
  \"system\": \"${CMAKE_SYSTEM_NAME}\",
  \"architecture\": \"${_package_arch}\",
  \"qt_version\": \"${Qt6Core_VERSION}\",
  \"gstreamer_version\": \"${VACUUM_GSTREAMER_VERSION}\",
  \"executable\": \"${INSTALL_BINS}/$<TARGET_FILE_NAME:${VACUUM_LOADER_NAME}>\",
  \"library\": \"${INSTALL_LIBS}/$<TARGET_FILE_NAME:${VACUUM_UTILS_NAME}>\",
  \"plugins_directory\": \"${INSTALL_PLUGINS}\",
  \"plugins\": [\n${_plugin_json}\n  ]
}
")
if(INSTALL_ARCHIVE_INFO)
    install(FILES "${_build_info}" DESTINATION "${INSTALL_DOCUMENTS}" RENAME build-info.json)
endif()

if(INSTALL_ARCHIVE_INFO AND UNIX AND NOT APPLE)
    execute_process(COMMAND getconf GNU_LIBC_VERSION
        OUTPUT_VARIABLE _libc_version OUTPUT_STRIP_TRAILING_WHITESPACE)
    if(EXISTS /etc/os-release)
        file(STRINGS /etc/os-release _os_release REGEX "^PRETTY_NAME=")
        string(REPLACE "PRETTY_NAME=" "" _os_release "${_os_release}")
        string(REGEX REPLACE "^\"|\"$" "" _os_release "${_os_release}")
    endif()
    set(_requirements "${CMAKE_BINARY_DIR}/runtime-requirements-$<CONFIG>.cmake")
    configure_file("${CMAKE_CURRENT_LIST_DIR}/runtime-requirements.cmake.in"
        "${CMAKE_BINARY_DIR}/runtime-requirements.cmake.in" @ONLY)
    file(GENERATE OUTPUT "${_requirements}"
        INPUT "${CMAKE_BINARY_DIR}/runtime-requirements.cmake.in")
    install(SCRIPT "${_requirements}")
endif()

cpack_add_component_group(core DISPLAY_NAME "Core components")
cpack_add_component_group(essential_plugins DISPLAY_NAME "Essential plugins")
cpack_add_component_group(optional_plugins DISPLAY_NAME "Optional plugins")
cpack_add_component_group(translations DISPLAY_NAME "Translations")
foreach(lang IN LISTS LOCALIZED_LANGS)
    lang_display_name(_language_name "${lang}")
    cpack_add_component_group(${lang}_translation DISPLAY_NAME "${_language_name} translation"
        PARENT_GROUP translations)
endforeach()
include(CPack)
