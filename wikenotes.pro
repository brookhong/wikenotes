#-------------------------------------------------
#
# Project created by QtCreator 2011-02-11T16:54:28
#
#-------------------------------------------------

QT       += core gui webkit network

VERSION = 0.4
# Define the preprocessor macro to get the application version in our application.
DEFINES += APP_VERSION=\\\"$$VERSION\\\"

TARGET = wikenotes
TEMPLATE = app
DEFINES += QXT_STATIC SQLITE_HAS_CODEC SQLITE_ENABLE_COLUMN_METADATA

INCLUDEPATH = sqlite/inc D:/works/openssl-0.9.8k_WIN32/include

SOURCES += main.cpp\
        mainwindow.cpp \
    notelist.cpp \
    highlighter.cpp \
    qxt/qxtglobal.cpp \
    qxt/qxtglobalshortcut.cpp \
    qtsingleapplication/qtsingleapplication.cpp \
    qtsingleapplication/qtlocalpeer.cpp \
    sqlite/src/sqlite3.c \
    sqlite/src/KompexSQLiteDatabase.cpp \
    sqlite/src/KompexSQLiteStatement.cpp \
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
    LIBS += D:/works/openssl-0.9.8k_WIN32/lib/libeay32.lib -luser32
}

TRANSLATIONS = zh_CN.ts
