option(BUNDLE_WINDOWS_DEPENDENCIES "Deploy dependencies for an extract-and-run Windows archive" ON)
if(NOT BUNDLE_WINDOWS_DEPENDENCIES)
    return()
endif()
if(Qt6Core_VERSION VERSION_LESS 6.5)
    message(FATAL_ERROR "Windows dependency deployment requires Qt 6.5+. Use BUNDLE_WINDOWS_DEPENDENCIES=OFF for a source-only build with older Qt.")
endif()

# Deploy the compiler DLLs beside the executable, rather than requiring users
# to run a redistributable installer after extracting the archive.
set(CMAKE_INSTALL_UCRT_LIBRARIES ON)
set(CMAKE_INSTALL_SYSTEM_RUNTIME_DESTINATION "${INSTALL_BINS}")
include(InstallRequiredSystemLibraries)

get_property(_plugins GLOBAL PROPERTY VACUUM_ENABLED_PLUGIN_TARGETS)
set(_plugin_paths "")
foreach(plugin IN LISTS _plugins)
    string(APPEND _plugin_paths "        \"${INSTALL_PLUGINS}/$<TARGET_FILE_NAME:${plugin}>\"\n")
endforeach()

if(TARGET voicecalls)
    get_property(VACUUM_GSTREAMER_ROOT GLOBAL PROPERTY VACUUM_GSTREAMER_ROOT)
    if(NOT IS_DIRECTORY "${VACUUM_GSTREAMER_ROOT}")
        message(FATAL_ERROR "Set VACUUM_GSTREAMER_ROOT to the matching GStreamer runtime/development prefix")
    endif()
    # MediaSession selects these application-local plugins before initializing
    # GStreamer, so no machine-wide installation or PATH edits are needed.
    set(_gst_plugins coreelements typefindfunctions audioconvert audioresample
        volume audiotestsrc autodetect wasapi wasapi2 opus rtp rtpmanager
        webrtc nice dtls srtp sctp webrtcdsp)
    foreach(plugin IN LISTS _gst_plugins)
        unset(_gst_plugin)
        find_file(_gst_plugin NAMES "gst${plugin}.dll" "libgst${plugin}.dll"
            PATHS "${VACUUM_GSTREAMER_ROOT}/lib/gstreamer-1.0" NO_DEFAULT_PATH NO_CACHE REQUIRED)
        install(FILES "${_gst_plugin}" DESTINATION "${INSTALL_BINS}/gstreamer-1.0")
    endforeach()
    foreach(tool gst-inspect-1.0 gst-launch-1.0)
        install(PROGRAMS "${VACUUM_GSTREAMER_ROOT}/bin/${tool}.exe" DESTINATION "${INSTALL_BINS}")
    endforeach()
    install(PROGRAMS "${VACUUM_GSTREAMER_ROOT}/libexec/gstreamer-1.0/gst-plugin-scanner.exe"
        DESTINATION "${INSTALL_BINS}")
    install(FILES "${VACUUM_GSTREAMER_ROOT}/lib/gio/modules/gioopenssl.dll"
        DESTINATION "${INSTALL_BINS}/gio-modules")
    install(FILES "${VACUUM_GSTREAMER_ROOT}/etc/ssl/certs/ca-certificates.crt"
        DESTINATION "${INSTALL_BINS}/certificates")
    install(DIRECTORY "${VACUUM_GSTREAMER_ROOT}/share/licenses/" DESTINATION "${INSTALL_DOCUMENTS}/licenses/GStreamer")
endif()

foreach(license_directory LICENSES Licenses)
    if(IS_DIRECTORY "${QT6_INSTALL_PREFIX}/${license_directory}")
        install(DIRECTORY "${QT6_INSTALL_PREFIX}/${license_directory}/"
            DESTINATION "${INSTALL_DOCUMENTS}/licenses/Qt")
    endif()
endforeach()

configure_file(deploy-windows.cmake.in "${CMAKE_CURRENT_BINARY_DIR}/deploy-windows.cmake.in" @ONLY)
file(READ "${CMAKE_CURRENT_BINARY_DIR}/deploy-windows.cmake.in" _deploy_content)
# Qt 6.8's versionless macro re-evaluates CONTENT. Call the function directly
# so install-time variables, quotes and semicolons remain intact.
qt6_generate_deploy_script(TARGET ${VACUUM_LOADER_NAME}
    OUTPUT_SCRIPT _deploy_script CONTENT "${_deploy_content}")
install(SCRIPT "${_deploy_script}")
