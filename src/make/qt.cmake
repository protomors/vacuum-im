find_package(Qt6 REQUIRED COMPONENTS
	Widgets
	Network
	Xml
	Concurrent
	LinguistTools
	PrintSupport
	Multimedia
	Sql)
find_package(Qt6 QUIET COMPONENTS Svg)

# QX11Info lives in GuiPrivate. Qt 6 has no X11Extras module.
if(UNIX AND NOT APPLE)
	find_package(Qt6 REQUIRED COMPONENTS GuiPrivate)
endif()

foreach(_qt_tool lupdate lrelease)
	get_target_property(_qt_tool_path Qt6::${_qt_tool} IMPORTED_LOCATION)
	if(NOT _qt_tool_path)
		get_target_property(_qt_tool_path Qt6::${_qt_tool} IMPORTED_LOCATION_RELEASE)
	endif()
	string(TOUPPER ${_qt_tool} _qt_tool_upper)
	set(QT_${_qt_tool_upper}_EXECUTABLE "${_qt_tool_path}")
endforeach()

macro(qt_wrap_cpp out)
	qt6_wrap_cpp(${out} ${ARGN})
endmacro()

macro(qt_wrap_ui out)
	qt6_wrap_ui(${out} ${ARGN})
endmacro()

macro(qt_add_resources out)
	qt6_add_resources(${out} ${ARGN})
endmacro()

message(STATUS "Building Vacuum-IM against Qt 6")
