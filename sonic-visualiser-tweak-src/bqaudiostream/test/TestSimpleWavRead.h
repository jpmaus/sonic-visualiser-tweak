/* -*- c-basic-offset: 4 indent-tabs-mode: nil -*-  vi:set ts=8 sts=4 sw=4: */
/* Copyright Chris Cannam - All Rights Reserved */

#ifndef TEST_SIMPLE_WAV_READ_H
#define TEST_SIMPLE_WAV_READ_H

#include <QObject>
#include <QtTest>

#include "bqaudiostream/AudioReadStreamFactory.h"
#include "bqaudiostream/AudioReadStream.h"

namespace breakfastquay {

class TestSimpleWavRead : public QObject
{
    Q_OBJECT

    // This is a 44.1KHz 16-bit mono WAV file with 20 samples in it,
    // with a 1 at the start, -1 at the end and 0 elsewhere
    static const char *testsound() { 
	static const char *f = "testfiles/20samples.wav";
	return f;
    }

private slots:

    void supported() {
	// We should *always* be able to read WAV files
	QVERIFY(AudioReadStreamFactory::isExtensionSupportedFor(testsound()));
    }
    
    void open() {
	AudioReadStream *s = AudioReadStreamFactory::createReadStream(testsound());
	QVERIFY(s);
	QCOMPARE(s->getError(), std::string());
	QCOMPARE(s->getChannelCount(), size_t(1));
	QCOMPARE(s->getSampleRate(), size_t(44100));
	delete s;
    }

    void length() {
	AudioReadStream *s = AudioReadStreamFactory::createReadStream(testsound());
	QVERIFY(s);
	float frames[22];
	size_t n = s->getInterleavedFrames(22, frames);
	QCOMPARE(n, size_t(20));
	delete s;
    }
    
    void read() {
	AudioReadStream *s = AudioReadStreamFactory::createReadStream(testsound());
	QVERIFY(s);
	float frames[4];
	size_t n = s->getInterleavedFrames(4, frames);
	QCOMPARE(n, size_t(4));
	QCOMPARE(frames[0], 32767.f/32768.f); // 16 bit file, so never quite 1
	QCOMPARE(frames[1], 0.f);
	QCOMPARE(frames[2], 0.f);
	QCOMPARE(frames[3], 0.f);
	delete s;
    }
    
    void readEnd() {
	AudioReadStream *s = AudioReadStreamFactory::createReadStream(testsound());
	QVERIFY(s);
	float frames[20];
	size_t n = s->getInterleavedFrames(20, frames);
	QCOMPARE(n, size_t(20));
	QCOMPARE(frames[17], 0.f);
	QCOMPARE(frames[18], 0.f);
	QCOMPARE(frames[19], -1.f);
	delete s;
    }

    void resampledLength() {
	AudioReadStream *s = AudioReadStreamFactory::createReadStream(testsound());
	QVERIFY(s);
	s->setRetrievalSampleRate(22050);
	float frames[22];
	size_t n = s->getInterleavedFrames(22, frames);
	QCOMPARE(n, size_t(10));
	delete s;
    }
};

}

#endif

	
