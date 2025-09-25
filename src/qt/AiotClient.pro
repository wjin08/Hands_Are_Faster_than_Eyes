QT       += core gui network
greaterThan(QT_MAJOR_VERSION, 4): QT += widgets
CONFIG += c++17
TARGET = AiotClient
TEMPLATE = app

SOURCES += \
    main.cpp \
    mainwidget.cpp \
    socketclient.cpp \
    tab1devcontrol.cpp \
    tab2socketclient.cpp

HEADERS += \
    mainwidget.h \
    socketclient.h \
    tab1devcontrol.h \
    tab2socketclient.h

FORMS += \
    tab1devcontrol.ui \
    tab2socketclient.ui
