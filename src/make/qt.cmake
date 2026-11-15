find_package(Qt6 6.10 REQUIRED COMPONENTS
	Widgets
	Network
	Xml
	Concurrent
	LinguistTools
	PrintSupport
	Multimedia
	Sql)
find_package(Qt6 QUIET COMPONENTS Svg)

message(STATUS "Building Vacuum-IM against Qt 6")
