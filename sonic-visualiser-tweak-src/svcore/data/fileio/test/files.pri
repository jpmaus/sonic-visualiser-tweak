
TEST_HEADERS += \
	../../model/test/MockWaveModel.h \
	AudioFileReaderTest.h \
	UnsupportedFormat.h \
	BogusAudioFileReaderTest.h \
	AudioFileWriterTest.h \
	AudioTestData.h \
	EncodingTest.h \
	MIDIFileReaderTest.h \
	CSVFormatTest.h \
	CSVStreamWriterTest.h
     
TEST_SOURCES += \
	../../model/test/MockWaveModel.cpp \
        UnsupportedFormat.cpp \
	svcore-data-fileio-test.cpp
