CONFIG -= debug
CONFIG += release

# Define these to use the Redland datastore (http://librdf.org/)
DEFINES += USE_REDLAND
QMAKE_CXXFLAGS += -I/usr/include/rasqal -I/usr/include/raptor2
EXTRALIBS += -lrdf

# Define this to use the Sord datastore (http://drobilla.net/software/sord/)
#DEFINES += USE_SORD
#QMAKE_CXXFLAGS += -I/usr/include/sord-0 -I/usr/include/serd-0 -Werror
#EXTRALIBS += -lsord-0 -lserd-0

