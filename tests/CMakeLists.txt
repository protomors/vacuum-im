find_package(Qt6 REQUIRED COMPONENTS Test)

# Compile the implementation directly; plugin classes are not exported on Windows.
add_executable(security_tests
	security_tests.cpp
	../src/plugins/defaultconnection/defaultconnection.cpp
	../src/plugins/defaultconnection/defaultconnection.h
	../src/plugins/defaultconnection/defaultconnectionengine.cpp
	../src/plugins/defaultconnection/defaultconnectionengine.h
	../src/plugins/defaultconnection/connectionoptionswidget.cpp
	../src/plugins/defaultconnection/connectionoptionswidget.h
	../src/plugins/defaultconnection/connectionoptionswidget.ui
	../src/plugins/saslauth/saslauthfeature.cpp
	../src/plugins/saslauth/saslauthfeature.h)
set_target_properties(security_tests PROPERTIES AUTOMOC ON AUTOUIC ON)
target_link_libraries(security_tests PRIVATE vacuumutils Qt6::Test Qt6::Widgets Qt6::Network Qt6::Xml)
add_test(NAME security COMMAND security_tests)
set_tests_properties(security PROPERTIES ENVIRONMENT "QT_QPA_PLATFORM=offscreen")
