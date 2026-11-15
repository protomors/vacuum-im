project(httpupload)

set(PLUGIN_NAME "httpupload")
set(PLUGIN_DISPLAY_NAME "HTTP File Upload")
set(PLUGIN_DESCRIPTION "Allows to share files by uploading them to the server")
set(PLUGIN_DEPENDENCIES xmppstreams stanzaprocessor servicediscovery) # used only in CPack

include("httpupload.cmake")
include("${CMAKE_SOURCE_DIR}/src/plugins/plugins.cmake")
