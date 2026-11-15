if (NOT SYSTEM_ZLIB_FOUND)
	add_subdirectory(zlib)
endif (NOT SYSTEM_ZLIB_FOUND)

if (NOT SYSTEM_MINIZIP_FOUND)
	add_subdirectory(minizip)
endif (NOT SYSTEM_MINIZIP_FOUND)

if (NOT SYSTEM_IDN_FOUND)
	add_subdirectory(idn)
endif (NOT SYSTEM_IDN_FOUND)

# Build Qt-based helpers against the application's Qt, rather than picking up
# unversioned system libraries that may still be linked against Qt 5.
add_subdirectory(qtlockedfile)
add_subdirectory(qxtglobalshortcut)

add_subdirectory(idle)
