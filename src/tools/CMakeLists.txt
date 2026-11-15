# Translation maintenance tools are optional and are not installed with the app.
foreach(tool autotranslate txprepare)
	add_executable(${tool} ${tool}/${tool}.cpp)
	target_link_libraries(${tool} PRIVATE Qt6::Core Qt6::Xml)
endforeach()
