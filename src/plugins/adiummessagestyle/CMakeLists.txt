project(adiummessagestyle)

# Qt WebKit is not part of Qt 6, so Adium styles cannot be built.
set(PLUGIN_adiummessagestyle OFF CACHE BOOL "Adium message styles require Qt WebKit, which Qt 6 does not ship" FORCE)

set(PLUGIN_NAME "adiummessagestyle")
set(PLUGIN_DISPLAY_NAME "Adium message style")

include("adiummessagestyle.cmake")
include("${CMAKE_SOURCE_DIR}/src/plugins/plugins.cmake")
