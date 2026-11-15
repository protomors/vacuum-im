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
