# needed for languages.cmake to correctly generate CPack components
set(PLUGIN_NAME ${VACUUM_LOADER_NAME})
set(PLUGIN_DISPLAY_NAME "loader")

include_directories("${CMAKE_SOURCE_DIR}/src" "${CMAKE_BINARY_DIR}/src/loader")

include("${CMAKE_SOURCE_DIR}/src/make/config.cmake")
include("${CMAKE_SOURCE_DIR}/src/translations/languages.cmake") 
include("loader.cmake")

project(loader CXX)

if (UNIX AND NOT APPLE)
	set(CMAKE_INSTALL_RPATH "$ORIGIN/../${INSTALL_LIB_DIR}/")
endif (UNIX AND NOT APPLE)

if (GIT_DATE MATCHES "^[0-9]+$")
	configure_file(gitinfo.h.in gitinfo.h @ONLY)
	add_definitions(-DGITINFO)
endif ()

if (WIN32)
	enable_language(RC)
	list(APPEND SOURCES loader.rc)
endif ()

add_translations(TRANSLATIONS "${VACUUM_LOADER_NAME}" ${SOURCES} ${UIS})
add_executable(${VACUUM_LOADER_NAME} WIN32 MACOSX_BUNDLE ${SOURCES} ${MOC_SOURCES} ${UI_HEADERS} ${TRANSLATIONS})
set_target_properties(${VACUUM_LOADER_NAME} PROPERTIES RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}")
target_link_libraries(${VACUUM_LOADER_NAME} ${VACUUM_UTILS_NAME} Qt6::Widgets Qt6::Xml)

install(TARGETS ${VACUUM_LOADER_NAME}
	DESTINATION "${INSTALL_BINS}"
	COMPONENT ${VACUUM_LOADER_NAME})
if (UNIX AND NOT APPLE AND NOT HAIKU)
	install(FILES "${CMAKE_SOURCE_DIR}/resources/menuicons/shared/vacuum.png"
		DESTINATION "${INSTALL_RES_DIR}/pixmaps")
	install(FILES "${CMAKE_SOURCE_DIR}/src/packages/linux/vacuum.desktop"
		DESTINATION "${INSTALL_RES_DIR}/applications")
    install(FILES "${CMAKE_SOURCE_DIR}/src/packages/linux/vacuum-im.metainfo.xml"
		DESTINATION "${INSTALL_RES_DIR}/metainfo")
endif (UNIX AND NOT APPLE AND NOT HAIKU)

cpack_add_component(${VACUUM_LOADER_NAME}
	DISPLAY_NAME "Loader"
	REQUIRED
	GROUP core)
