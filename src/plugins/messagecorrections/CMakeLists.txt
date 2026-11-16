project(messagecorrections)

set(PLUGIN_NAME "messagecorrections")
set(PLUGIN_DISPLAY_NAME "Message Corrections")
set(PLUGIN_DEPENDENCIES messageprocessor messagewidgets) # used only in CPack

include("messagecorrections.cmake")
include("${CMAKE_SOURCE_DIR}/src/plugins/plugins.cmake")
