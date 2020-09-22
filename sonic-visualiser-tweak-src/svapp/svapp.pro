
TEMPLATE = lib

INCLUDEPATH += ../vamp-plugin-sdk

exists(config.pri) {
    include(config.pri)
}

CONFIG += staticlib qt thread warn_on stl rtti exceptions c++11
QT += network xml gui widgets

TARGET = svapp

DEPENDPATH += . ../bqaudioio ../svcore ../svgui ../piper-cpp
INCLUDEPATH += . ../bqaudioio ../svcore ../svgui ../piper-cpp
OBJECTS_DIR = o
MOC_DIR = o

include(files.pri)

HEADERS = $$(SVAPP_HEADERS)
SOURCES = $$(SVAPP_SOURCES)

