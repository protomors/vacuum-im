vacuum_find_hunspell()
add_definitions(-DHAVE_HUNSPELL)
set(ADD_LIBS Vacuum::Hunspell)

set(SOURCES ${SOURCES} "hunspellchecker.cpp")
set(HEADERS ${HEADERS} "hunspellchecker.h")
