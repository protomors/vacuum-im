find_package(Qt6 6.2 REQUIRED COMPONENTS
	Widgets
	Network
	Xml
	Concurrent
	LinguistTools
	PrintSupport
	Multimedia
	Sql)
find_package(Qt6 QUIET COMPONENTS Svg)

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
