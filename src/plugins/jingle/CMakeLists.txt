project(jingle)

set(PLUGIN_NAME "jingle")
set(PLUGIN_DISPLAY_NAME "Jingle")
set(PLUGIN_DESCRIPTION "Allows to negotiate peer-to-peer sessions with contacts")
set(PLUGIN_DEPENDENCIES xmppstreams stanzaprocessor) # used only in CPack

include("jingle.cmake")
include("${CMAKE_SOURCE_DIR}/src/plugins/plugins.cmake")
