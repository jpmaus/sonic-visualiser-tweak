
TEMPLATE = app

include(checker.pri)

# Using the "console" CONFIG flag above should ensure this happens for
# normal Windows builds, but this may be necessary when cross-compiling
win32-x-g++:QMAKE_LFLAGS += -Wl,-subsystem,console

macx*: CONFIG -= app_bundle
    
TARGET = checker-client

SOURCES += \
	src/checker-client.cpp

