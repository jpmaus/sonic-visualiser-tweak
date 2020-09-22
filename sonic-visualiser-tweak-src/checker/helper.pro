
TEMPLATE = app

CONFIG += stl c++11 exceptions console warn_on
CONFIG -= qt

# Using the "console" CONFIG flag above should ensure this happens for
# normal Windows builds, but this may be necessary when cross-compiling
win32-x-g++:QMAKE_LFLAGS += -Wl,-subsystem,console

macx*: CONFIG -= app_bundle

!win32* {
    QMAKE_CXXFLAGS_DEBUG += -Werror
}

linux* {
    LIBS += -ldl
}

TARGET = vamp-plugin-load-checker

OBJECTS_DIR = o
MOC_DIR = o

SOURCES += \
	src/helper.cpp

exists(../platform-helpers.pri) {
	include(../platform-helpers.pri)
}

