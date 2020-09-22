
TEMPLATE = lib

INCLUDEPATH += ../vamp-plugin-sdk

exists(config.pri) {
    include(config.pri)
}

CONFIG += staticlib qt thread warn_on stl rtti exceptions c++11
QT += network xml gui widgets svg

TARGET = svgui

DEPENDPATH += . ../svcore
INCLUDEPATH += . ../svcore
OBJECTS_DIR = o
MOC_DIR = o

include(files.pri)

HEADERS = $$(SVGUI_HEADERS)
SOURCES = $$(SVGUI_SOURCES)
