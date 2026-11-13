project(adiummessagestyle)

# Qt WebKit is not part of Qt 6.
if(QT_VERSION_MAJOR EQUAL 6)
	set(PLUGIN_adiummessagestyle OFF CACHE BOOL "Adium message styles require Qt WebKit, which Qt 6 does not ship" FORCE)
endif()

set(PLUGIN_NAME "adiummessagestyle")
set(PLUGIN_DISPLAY_NAME "Adium message style")

include("adiummessagestyle.cmake")
include("${CMAKE_SOURCE_DIR}/src/plugins/plugins.cmake")

if (IS_ENABLED)
	find_package(Qt5WebKit REQUIRED)
	find_package(Qt5WebKitWidgets REQUIRED)
	target_link_libraries(${PLUGIN_NAME} Qt5::WebKitWidgets)
	if (APPLE)
		file(COPY "${CMAKE_SOURCE_DIR}/resources/adiummessagestyles"
			DESTINATION "${CMAKE_BINARY_DIR}/${INSTALL_RESOURCES}")
	else (APPLE)
		install(DIRECTORY "${CMAKE_SOURCE_DIR}/resources/adiummessagestyles"
			DESTINATION "${INSTALL_RESOURCES}"
			COMPONENT ${PLUGIN_NAME})
	endif (APPLE)
endif (IS_ENABLED)
