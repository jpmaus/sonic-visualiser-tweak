
SOURCES	:= $(wildcard src/*.cpp)
HEADERS	:= $(wildcard src/*.h) $(wildcard bqaudioio/*.h)
OBJECTS	:= $(patsubst %.cpp,%.o,$(SOURCES))
LIBRARY	:= libbqaudioio.a

CXXFLAGS := -std=c++11 -I. -I./bqaudioio -I../bqvec -I../bqresample -DHAVE_JACK -DHAVE_LIBPULSE -DHAVE_PORTAUDIO

all:	$(LIBRARY)

$(LIBRARY):	$(OBJECTS)
	ar cr $@ $^

clean:		
	rm -f $(OBJECTS)

distclean:	clean
	rm -f $(LIBRARY)

depend:
	makedepend -Y -fMakefile -I./bqaudioio $(SOURCES) $(HEADERS)


# DO NOT DELETE

src/SystemRecordSource.o: ./bqaudioio/SystemRecordSource.h
src/SystemRecordSource.o: ./bqaudioio/Suspendable.h
src/SystemRecordSource.o: ./bqaudioio/ApplicationRecordTarget.h
src/AudioFactory.o: ./bqaudioio/AudioFactory.h src/JACKAudioIO.h
src/AudioFactory.o: src/PortAudioIO.h src/PulseAudioIO.h
src/SystemPlaybackTarget.o: ./bqaudioio/SystemPlaybackTarget.h
src/SystemPlaybackTarget.o: ./bqaudioio/Suspendable.h
src/ResamplerWrapper.o: ./bqaudioio/ResamplerWrapper.h
src/ResamplerWrapper.o: ./bqaudioio/ApplicationPlaybackSource.h
bqaudioio/SystemPlaybackTarget.o: ./bqaudioio/Suspendable.h
bqaudioio/ResamplerWrapper.o: ./bqaudioio/ApplicationPlaybackSource.h
bqaudioio/SystemAudioIO.o: ./bqaudioio/SystemRecordSource.h
bqaudioio/SystemAudioIO.o: ./bqaudioio/Suspendable.h
bqaudioio/SystemAudioIO.o: ./bqaudioio/SystemPlaybackTarget.h
bqaudioio/SystemRecordSource.o: ./bqaudioio/Suspendable.h
