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
