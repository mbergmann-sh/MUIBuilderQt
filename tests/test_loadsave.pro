QT += core
QT -= gui
CONFIG += c++17 console
CONFIG -= app_bundle
TEMPLATE = app
TARGET = test_loadsave

INCLUDEPATH += ../core

HEADERS += \
    ../core/muibobject.h \
    ../core/bytecursor.h \
    ../core/muibloader.h \
    ../core/muibsaver.h

SOURCES += \
    test_loadsave.cpp \
    ../core/muibloader.cpp \
    ../core/muibsaver.cpp
