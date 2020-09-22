
TEMPLATE = lib

INCLUDEPATH += ../vamp-plugin-sdk

exists(config.pri) {
    include(config.pri)
}

CONFIG += staticlib qt thread warn_on stl rtti exceptions c++11
QT += network xml
QT -= gui

TARGET = svcore

DEPENDPATH += . data plugin plugin/api/alsa ../dataquay ../checker ../piper-cpp
INCLUDEPATH += . data plugin plugin/api/alsa ../dataquay ../checker ../piper-cpp
OBJECTS_DIR = o
MOC_DIR = o

# Doesn't work with this library, which contains C99 as well as C++
PRECOMPILED_HEADER =

# Set up suitable platform defines for RtMidi
linux*:   DEFINES += __LINUX_ALSASEQ__
macx*:    DEFINES += __MACOSX_CORE__
win*:     DEFINES += __WINDOWS_MM__
solaris*: DEFINES += __RTMIDI_DUMMY_ONLY__

include(files.pri)

HEADERS = $$SVCORE_HEADERS
SOURCES = $$SVCORE_SOURCES

