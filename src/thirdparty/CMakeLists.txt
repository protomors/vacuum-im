vacuum_find_core_dependencies()

# Build Qt-based helpers against the application's Qt, rather than picking up
# unversioned system libraries that may still be linked against Qt 5.
add_subdirectory(qtlockedfile)
add_subdirectory(qxtglobalshortcut)

add_subdirectory(idle)
