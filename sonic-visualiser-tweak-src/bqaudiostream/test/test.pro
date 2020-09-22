
TEMPLATE = app
CONFIG += debug
TARGET = test-audiostream
win*: TARGET = "TestAudiostream"

QT += testlib
QT -= gui

DESTDIR = .
QMAKE_LIBDIR += . ..

LIBS += -L.. -lbqaudiostream -L../../bqresample -lbqresample -L../../bqvec -lbqvec -lsndfile -loggz -lfishsound -lopusfile -lopus -logg

INCLUDEPATH += . .. ../../bqvec ../../bqresample ../../bqthingfactory
DEPENDPATH += . .. ../../bqvec ../../bqresample ../../bqthingfactory

HEADERS += AudioStreamTestData.h TestAudioStreamRead.h TestSimpleWavRead.h TestWavReadWrite.h
SOURCES += main.cpp

!win32 {
    !macx* {
        QMAKE_POST_LINK=$${DESTDIR}/$${TARGET}
    }
    macx* {
        QMAKE_POST_LINK=$${DESTDIR}/$${TARGET}.app/Contents/MacOS/$${TARGET}
    }
}

win32-g++:QMAKE_POST_LINK=$${DESTDIR}$${TARGET}.exe

