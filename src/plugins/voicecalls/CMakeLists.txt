project(voicecalls)

set(PLUGIN_NAME "voicecalls")
set(PLUGIN_DISPLAY_NAME "Voice Calls")
set(PLUGIN_DESCRIPTION "Allows to make and receive voice calls")
set(PLUGIN_DEPENDENCIES xmppstreams stanzaprocessor) # used only in CPack

# The media side of calls is the WebRTC stack of GStreamer
find_package(PkgConfig)
if (PKG_CONFIG_FOUND)
	pkg_check_modules(VOICECALLS_GST gstreamer-1.0>=1.20 gstreamer-sdp-1.0 gstreamer-webrtc-1.0)
endif (PKG_CONFIG_FOUND)

if (VOICECALLS_GST_FOUND)
	include("voicecalls.cmake")
	include_directories(${VOICECALLS_GST_INCLUDE_DIRS})
	link_directories(${VOICECALLS_GST_LIBRARY_DIRS})
	set(ADD_LIBS ${VOICECALLS_GST_LIBRARIES})
	include("${CMAKE_SOURCE_DIR}/src/plugins/plugins.cmake")

	if (IS_ENABLED)
		target_link_libraries(${PLUGIN_NAME} Qt6::Multimedia)
	endif (IS_ENABLED)
else (VOICECALLS_GST_FOUND)
	message(STATUS "GStreamer WebRTC development files not found, voicecalls plugin will not be built")
endif (VOICECALLS_GST_FOUND)
