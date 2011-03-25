#-------------------------------------------------
#
# Project created by QtCreator 2011-02-11T16:54:28
#
#-------------------------------------------------

QT       += core gui sql webkit network

VERSION = 0.3
# Define the preprocessor macro to get the application version in our application.
DEFINES += APP_VERSION=\\\"$$VERSION\\\"

TARGET = wikenotes
TEMPLATE = app
DEFINES += QXT_STATIC

SOURCES += main.cpp\
        mainwindow.cpp \
    notelist.cpp \
    highlighter.cpp \
    qxt/qxtglobal.cpp \
    qxt/qxtglobalshortcut.cpp \
    qtsingleapplication/qtsingleapplication.cpp \
    qtsingleapplication/qtlocalpeer.cpp \
    hotkeysettings.cpp \
    noteitem.cpp

HEADERS  += mainwindow.h \
    notelist.h \
    highlighter.h \
    qxt/qxtglobalshortcut.h \
    qtsingleapplication/qtsingleapplication.h \
    qtsingleapplication/qtlocalpeer.h \
    hotkeysettings.h \
    noteitem.h

FORMS    += mainwindow.ui \
    hotkeysettings.ui

RESOURCES += resources/wike.qrc

win32 {
    SOURCES += qxt/qxtglobalshortcut_win.cpp
    RC_FILE = resources/wike.rc
}

TRANSLATIONS = chinese.ts
