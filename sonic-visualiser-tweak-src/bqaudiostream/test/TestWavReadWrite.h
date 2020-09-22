/* -*- c-basic-offset: 4 indent-tabs-mode: nil -*-  vi:set ts=8 sts=4 sw=4: */
/* Copyright Chris Cannam - All Rights Reserved */

#ifndef TEST_WAV_READ_WRITE_H
#define TEST_WAV_READ_WRITE_H

#include <QObject>
#include <QtTest>

#include "bqaudiostream/AudioReadStreamFactory.h"
#include "bqaudiostream/AudioReadStream.h"
#include "bqaudiostream/AudioWriteStreamFactory.h"
#include "bqaudiostream/AudioWriteStream.h"

#include "bqvec/Allocators.h"

namespace breakfastquay {

static const float DB_FLOOR = -1000.0;

static float to_dB(float ratio)
{
    if (ratio == 0.0) return DB_FLOOR;
    float dB = 10 * log10f(ratio);
    return dB;
}

static float from_dB(float dB)
{
    if (dB == DB_FLOOR) return 0.0;
    float m = powf(10.0, dB / 10.0);
    return m;
}

class TestWavReadWrite : public QObject
{
    Q_OBJECT

    // This is a 44.1KHz, 16-bit, 2-channel PCM WAV containing our
    // 2-second test signal
    static const char *testfile() { 
	static const char *f = "testfiles/44100-2-16.wav";
	return f;
    }
    static const char *outfile() { 
	static const char *f = "test-audiostream-out.wav";
	return f;
    }
    static const char *outfile_origrate() { 
	static const char *f = "test-audiostream-out-origrate.wav";
	return f;
    }

private slots:
    void readWriteResample() {
	
	// First read file into memory at normal sample rate

	AudioReadStream *rs = AudioReadStreamFactory::createReadStream(testfile());
	QVERIFY(rs);

	int cc = rs->getChannelCount();
	QCOMPARE(cc, 2);

	int rate = rs->getSampleRate();
	QCOMPARE(rate, 44100);

	int bs = 2048;
	int count = 0;
	int bufsiz = bs;
	float *buffer = allocate<float>(bs);

	while (1) {
	    if (count + bs > bufsiz) {
		buffer = reallocate<float>(buffer, bufsiz, bufsiz * 2);
		bufsiz *= 2;
	    }
	    int got = cc * rs->getInterleavedFrames(bs / cc, buffer + count);
	    count += got;
	    if (got < bs) break;
	}
	
	delete rs;
    
	// Re-open with resampling

	rs = AudioReadStreamFactory::createReadStream(testfile());

	QVERIFY(rs);

	rs->setRetrievalSampleRate(rate * 2);
    
	// Write resampled test file
    
	AudioWriteStream *ws = AudioWriteStreamFactory::createWriteStream
	    (outfile(), cc, rate * 2);

	QVERIFY(ws);
	
	float *block = allocate<float>(bs);

	while (1) {
	    int got = rs->getInterleavedFrames(bs / cc, block);
	    ws->putInterleavedFrames(got, block);
	    if (got < bs / cc) break;
	}

	delete ws;
	delete rs;
	ws = 0;

	// Read back resampled file at original rate and compare
    
        rs = AudioReadStreamFactory::createReadStream(outfile());

        QVERIFY(rs);
        QCOMPARE(rs->getSampleRate(), size_t(rate * 2));

        rs->setRetrievalSampleRate(rate);

        ws = AudioWriteStreamFactory::createWriteStream
            (outfile_origrate(), cc, rate);

        QVERIFY(ws);
    
        float error = from_dB(-10);
        float warning = from_dB(-25);
        float maxdiff = 0.f;
        float mda = 0.f, mdb = 0.f;
        int maxdiffindex = -1;

        count = 0;

        while (1) {
            int got = rs->getInterleavedFrames(bs / cc, block);
            for (int i = 0; i < got * cc; ++i) {
                float a = block[i];
                float b = buffer[count + i];
                float diff = fabsf(a - b);
                if (diff > maxdiff &&
                    (count + i) > 10) { // first few samples are generally shaky
                    maxdiff = diff;
                    maxdiffindex = count + i;
                    mda = a;
                    mdb = b;
                }
            }
            count += got * cc;
            ws->putInterleavedFrames(got, block);
            if (got < bs / cc) break;
        }
        
        delete ws;
        delete rs;
        deallocate(block);
        deallocate(buffer);

        QString message = QString("Max diff is %1 (%2 dB) at index %3 (a = %4, b = %5) [error threshold %6 (%7 dB), warning threshold %8 (%9 dB)]")
            .arg(maxdiff)
            .arg(to_dB(maxdiff))
            .arg(maxdiffindex)
            .arg(mda)
            .arg(mdb)
            .arg(error)
            .arg(to_dB(error))
            .arg(warning)
            .arg(to_dB(warning));

        QVERIFY2(maxdiff < error, message.toLocal8Bit().data());

        if (maxdiff > warning) {
            QWARN(message.toLocal8Bit().data());
        }	
    }
};

}

#endif
