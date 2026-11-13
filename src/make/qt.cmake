# Qt 6 is the default. Pass -DQT_VERSION_MAJOR=5 to build against Qt 5.
set(QT_VERSION_MAJOR 6 CACHE STRING "Major Qt version to build against (5 or 6)")
set_property(CACHE QT_VERSION_MAJOR PROPERTY STRINGS 5 6)
if(NOT QT_VERSION_MAJOR MATCHES "^[56]$")
	message(FATAL_ERROR "QT_VERSION_MAJOR must be 5 or 6, not '${QT_VERSION_MAJOR}'")
endif()

set(_qt_required_components
	Widgets
	Network
	Xml
	Concurrent
	LinguistTools
	PrintSupport
	Multimedia
	Sql)
if(QT_VERSION_MAJOR EQUAL 6)
	# QTextCodec and the other Qt 5 text classes live here.
	list(APPEND _qt_required_components Core5Compat)
endif()
find_package(Qt${QT_VERSION_MAJOR} REQUIRED COMPONENTS ${_qt_required_components})
find_package(Qt${QT_VERSION_MAJOR} QUIET COMPONENTS Svg)

# Qt 6 has no X11Extras module. QX11Info lives in GuiPrivate.
if(UNIX AND NOT APPLE)
	if(QT_VERSION_MAJOR EQUAL 6)
		find_package(Qt6 REQUIRED COMPONENTS GuiPrivate)
		if(NOT TARGET Qt6::X11Extras)
			add_library(Qt6::X11Extras INTERFACE IMPORTED)
			target_link_libraries(Qt6::X11Extras INTERFACE Qt6::GuiPrivate)
		endif()
	else()
		find_package(Qt5 REQUIRED COMPONENTS X11Extras)
	endif()
endif()

if(QT_VERSION_MAJOR EQUAL 6)
	foreach(_qt_tool lupdate lrelease)
		get_target_property(_qt_tool_path Qt6::${_qt_tool} IMPORTED_LOCATION)
		if(NOT _qt_tool_path)
			get_target_property(_qt_tool_path Qt6::${_qt_tool} IMPORTED_LOCATION_RELEASE)
		endif()
		string(TOUPPER ${_qt_tool} _qt_tool_upper)
		set(QT_${_qt_tool_upper}_EXECUTABLE "${_qt_tool_path}")
	endforeach()
else()
	set(QT_LUPDATE_EXECUTABLE "${Qt5_LUPDATE_EXECUTABLE}")
	set(QT_LRELEASE_EXECUTABLE "${Qt5_LRELEASE_EXECUTABLE}")
endif()

macro(qt_wrap_cpp out)
	if(QT_VERSION_MAJOR EQUAL 6)
		qt6_wrap_cpp(${out} ${ARGN})
	else()
		qt5_wrap_cpp(${out} ${ARGN})
	endif()
endmacro()

macro(qt_wrap_ui out)
	if(QT_VERSION_MAJOR EQUAL 6)
		qt6_wrap_ui(${out} ${ARGN})
	else()
		qt5_wrap_ui(${out} ${ARGN})
	endif()
endmacro()

macro(qt_add_resources out)
	if(QT_VERSION_MAJOR EQUAL 6)
		qt6_add_resources(${out} ${ARGN})
	else()
		qt5_add_resources(${out} ${ARGN})
	endif()
endmacro()

message(STATUS "Building Vacuum-IM against Qt ${QT_VERSION_MAJOR}")
