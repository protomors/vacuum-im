project(messagereceipts)

set(PLUGIN_NAME "messagereceipts")
set(PLUGIN_DISPLAY_NAME "Message Receipts")
set(PLUGIN_DEPENDENCIES messageprocessor stanzaprocessor) # used only in CPack

include("messagereceipts.cmake")
include("${CMAKE_SOURCE_DIR}/src/plugins/plugins.cmake")
