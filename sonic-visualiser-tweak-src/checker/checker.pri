
CONFIG += qt stl c++11 exceptions console warn_on 
QT -= xml network gui widgets

!win32 {
    QMAKE_CXXFLAGS_DEBUG += -Werror
}

OBJECTS_DIR = o
MOC_DIR = o

INCLUDEPATH += checker

HEADERS += \
        checker/checkcode.h \
	checker/plugincandidates.h \
	checker/knownplugincandidates.h \
	checker/knownplugins.h

SOURCES += \
	src/plugincandidates.cpp \
	src/knownplugincandidates.cpp \
	src/knownplugins.cpp

        
