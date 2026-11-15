set(SOURCES "main.cpp"
			"aboutbox.cpp"
			"pluginmanager.cpp"
			"setuppluginsdialog.cpp")

set(UIS "aboutbox.ui"
		"setuppluginsdialog.ui")

set(HEADERS "aboutbox.h"
			"pluginmanager.h"
			"setuppluginsdialog.h")

qt6_wrap_cpp(MOC_SOURCES ${HEADERS})
qt6_wrap_ui(UI_HEADERS ${UIS})
