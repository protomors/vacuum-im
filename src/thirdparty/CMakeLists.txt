vacuum_find_core_dependencies()

# Build the Qt helpers against the same Qt as the application.
add_subdirectory(qtlockedfile)
add_subdirectory(qxtglobalshortcut)

add_subdirectory(idle)

if(INSTALL_DOCS)
    install(FILES README.md DESTINATION "${INSTALL_DOCUMENTS}" RENAME THIRD-PARTY.md)
    foreach(dependency zlib hunspell idn)
        file(GLOB licenses "${CMAKE_CURRENT_SOURCE_DIR}/${dependency}/COPYING*"
            "${CMAKE_CURRENT_SOURCE_DIR}/${dependency}/LICENSE")
        install(FILES ${licenses} DESTINATION "${INSTALL_DOCUMENTS}/licenses/${dependency}")
    endforeach()
endif()
