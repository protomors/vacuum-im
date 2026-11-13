include("${CMAKE_SOURCE_DIR}/src/make/config.cmake")

if (WIN32)
	foreach(LANG ${LOCALIZED_LANGS})
		string(SUBSTRING ${LANG} 0 2 SHORTLANG)
		if(EXISTS "${QT6_INSTALL_PREFIX}/translations/qtbase_${SHORTLANG}.qm")
			install(FILES "${QT6_INSTALL_PREFIX}/translations/qtbase_${SHORTLANG}.qm"
					DESTINATION "${CMAKE_INSTALL_PREFIX}/${INSTALL_TRANSLATIONS}/${LANG}" RENAME "qt_${SHORTLANG}.qm" COMPONENT Runtime)
		elseif(EXISTS "${QT6_INSTALL_PREFIX}/translations/qt_${SHORTLANG}.qm")
			install(FILES "${QT6_INSTALL_PREFIX}/translations/qt_${SHORTLANG}.qm"
					DESTINATION "${CMAKE_INSTALL_PREFIX}/${INSTALL_TRANSLATIONS}/${LANG}" COMPONENT Runtime)
		endif()
	endforeach(LANG)

	foreach(_candidate Qt6::QWindowsIntegrationPlugin Qt6::QWindowsVistaStylePlugin Qt6::QModernWindowsStylePlugin
			Qt6::QSvgPlugin Qt6::QSvgIconPlugin Qt6::QSQLiteDriverPlugin
			Qt6::QWindowsMediaPlugin Qt6::QFFmpegMediaPlugin
			Qt6::QSchannelBackendPlugin Qt6::QTlsBackendOpenSSLPlugin)
		if(TARGET ${_candidate})
			list(APPEND QtBinaries ${_candidate})
		endif()
	endforeach()

	foreach(_qtbinary ${QtBinaries})
		get_target_property(_qtbinary_fullpath ${_qtbinary} LOCATION)
		get_filename_component(_qtbinary_dirpath ${_qtbinary_fullpath} PATH)
		get_filename_component(_qtbinary_dirname ${_qtbinary_dirpath} NAME)
		get_filename_component(_qtbinary_file ${_qtbinary_fullpath} NAME)
		if(${_qtbinary_dirname} MATCHES "bin")
			set(_qtbinary_dirname "")
		endif()
		list(APPEND DLL_FILES "${CMAKE_INSTALL_PREFIX}/${INSTALL_APP_DIR}/${_qtbinary_dirname}/${_qtbinary_file}")
		install(FILES ${_qtbinary_fullpath} DESTINATION "${CMAKE_INSTALL_PREFIX}/${INSTALL_APP_DIR}/${_qtbinary_dirname}" COMPONENT Runtime)
	endforeach()

	get_property(_plugins GLOBAL PROPERTY ALL_PLUGINS_FULLPATHS)
	list(APPEND DLL_FILES "${_plugins}")

	list(APPEND DLL_DIRS ${QT6_INSTALL_PREFIX}/bin)
	list(APPEND DLL_DIRS ${CMAKE_INSTALL_PREFIX}/${INSTALL_APP_DIR})
	list(APPEND DLL_DIRS ${CMAKE_INSTALL_PREFIX}/${INSTALL_PLUGINS})

	set(CMAKE_INSTALL_UCRT_LIBRARIES ON)
	set(CMAKE_INSTALL_SYSTEM_RUNTIME_DESTINATION "${INSTALL_APP_DIR}")
	include(InstallRequiredSystemLibraries)

	install(CODE "
		set(BU_CHMOD_BUNDLE_ITEMS ON)
		include(BundleUtilities)
		fixup_bundle(\"\${CMAKE_INSTALL_PREFIX}/${INSTALL_APP_DIR}/vacuum.exe\" \"${DLL_FILES}\" \"${DLL_DIRS}\")" COMPONENT Runtime)

endif (WIN32)
