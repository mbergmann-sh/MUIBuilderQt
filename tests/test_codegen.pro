QT += core
QT -= gui
CONFIG += c++17 console
CONFIG -= app_bundle
TEMPLATE = app
TARGET = test_codegen

INCLUDEPATH += ../core

HEADERS += \
    ../core/muibobject.h \
    ../core/bytecursor.h \
    ../core/muibloader.h \
    ../core/muicodegen.h \
    ../core/notifytables.h \
    ../core/muibqtextras.h

SOURCES += \
    test_codegen.cpp \
    ../core/muibloader.cpp \
    ../core/muicodegen.cpp \
    ../core/notifytables.cpp \
    ../core/muibqtextras.cpp
