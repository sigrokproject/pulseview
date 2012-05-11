#-------------------------------------------------
#
# Project created by QtCreator 2012-05-10T21:19:20
#
#-------------------------------------------------

QT       += core gui

TARGET = sigrok-qt2
TEMPLATE = app

# The sigrok-qt version number. Define APP_VERSION macro for use in the code.
VERSION       = 0.1.0
DEFINES      += APP_VERSION=\\\"$$VERSION\\\"

SOURCES += main.cpp\
        mainwindow.cpp

HEADERS  += mainwindow.h

FORMS    += mainwindow.ui

RESOURCES    += sigrok-qt2.qrc

# libsigrok and libsigrokdecode
# TODO: Check for the minimum versions of libsigrok/libsigrokdecode we need.
win32 {
	# On Windows/MinGW we need to use '--libs --static'.
	# We also need to strip some stray '\n' characters here.
	QMAKE_CXXFLAGS += $$system(pkg-config --cflags libsigrokdecode \
			  libsigrok | sed s/\n//g)
	LIBS           += $$system(pkg-config --libs --static libsigrokdecode \
			  libsigrok | sed s/\n//g)
} else {
	QMAKE_CXXFLAGS += $$system(pkg-config --cflags libsigrokdecode)
	QMAKE_CXXFLAGS += $$system(pkg-config --cflags libsigrok)
	LIBS           += $$system(pkg-config --libs libsigrokdecode)
	LIBS           += $$system(pkg-config --libs libsigrok)
}
