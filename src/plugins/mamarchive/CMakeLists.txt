project(mamarchive)

set(PLUGIN_NAME "mamarchive")
set(PLUGIN_DISPLAY_NAME "Message Archive Management")
set(PLUGIN_DESCRIPTION "Downloads server message history into the local archive")
set(PLUGIN_DEPENDENCIES xmppstreams stanzaprocessor servicediscovery messagearchiver filemessagearchive) # used only in CPack

include("mamarchive.cmake")
include("${CMAKE_SOURCE_DIR}/src/plugins/plugins.cmake")
