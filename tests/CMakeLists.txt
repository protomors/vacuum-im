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

add_executable(jingle_tests
	jingle_tests.cpp
	../src/plugins/jingle/jingle.cpp
	../src/plugins/jingle/jingle.h)
set_target_properties(jingle_tests PROPERTIES AUTOMOC ON)
target_link_libraries(jingle_tests PRIVATE vacuumutils Qt6::Test Qt6::Widgets Qt6::Network Qt6::Xml)
add_test(NAME jingle COMMAND jingle_tests)

add_executable(message_tests
	message_tests.cpp
	../src/plugins/simplemessagestyle/simplemessagestyle.cpp
	../src/plugins/simplemessagestyle/simplemessagestyle.h
	../src/plugins/simplemessagestyle/styleviewer.cpp
	../src/plugins/simplemessagestyle/styleviewer.h)
set_target_properties(message_tests PROPERTIES AUTOMOC ON)
target_compile_definitions(message_tests PRIVATE TEST_STYLE_PATH="${CMAKE_SOURCE_DIR}/resources/simplemessagestyles/chat")
target_link_libraries(message_tests PRIVATE vacuumutils Qt6::Test Qt6::Widgets Qt6::Network Qt6::Xml)
add_test(NAME message COMMAND message_tests)
set_tests_properties(message PROPERTIES ENVIRONMENT "QT_QPA_PLATFORM=offscreen")

add_executable(receipts_tests
	receipts_tests.cpp
	../src/plugins/messagereceipts/messagereceipts.cpp
	../src/plugins/messagereceipts/messagereceipts.h)
set_target_properties(receipts_tests PROPERTIES AUTOMOC ON)
target_link_libraries(receipts_tests PRIVATE vacuumutils Qt6::Test Qt6::Widgets Qt6::Network Qt6::Xml)
add_test(NAME receipts COMMAND receipts_tests)
set_tests_properties(receipts PROPERTIES ENVIRONMENT "QT_QPA_PLATFORM=offscreen")
