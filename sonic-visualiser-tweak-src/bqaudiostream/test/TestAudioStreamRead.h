/* -*- c-basic-offset: 4 indent-tabs-mode: nil -*-  vi:set ts=8 sts=4 sw=4: */
/* Copyright Chris Cannam - All Rights Reserved */

#ifndef TEST_AUDIOSTREAM_READ_H
#define TEST_AUDIOSTREAM_READ_H

#include "bqaudiostream/AudioReadStreamFactory.h"
#include "bqaudiostream/AudioReadStream.h"
#include "bqaudiostream/Exceptions.h"

#include "AudioStreamTestData.h"

#include <cmath>

#include <QObject>
#include <QtTest>
#include <QDir>

#include <iostream>

using namespace std;

namespace breakfastquay {

static QString audioDir = "testfiles";

class TestAudioStreamRead : public QObject
{
    Q_OBJECT

    const char *strOf(QString s) {
        return strdup(s.toLocal8Bit().data());
    }

private slots:
    void init()
    {
        if (!QDir(audioDir).exists()) {
            cerr << "ERROR: Audio test file directory \"" << audioDir.toLocal8Bit().data() << "\" does not exist" << endl;
            QVERIFY2(QDir(audioDir).exists(), "Audio test file directory not found");
        }
    }

    void read_data()
    {
        QTest::addColumn<QString>("audiofile");
        QStringList files = QDir(audioDir).entryList(QDir::Files);
        foreach (QString filename, files) {
            // Our test audio files are all named
            // RATE-CHANNELS-BITDEPTH.ext (for PCM data) or
            // RATE-CHANNELS.ext (for lossy data)
            QStringList fileAndExt = filename.split(".");
            QStringList bits = fileAndExt[0].split("-");
            if (bits.size() < 2 || bits.size() > 3) continue;
            QTest::newRow(strOf(filename)) << filename;
        }
    }

    void read()
    {
        QFETCH(QString, audiofile);

        cerr << "\n\n*** audiofile = " << audiofile.toLocal8Bit().data() << "\n\n" << endl;

        try {

            int readRate = 48000;

            string filename = (audioDir + "/" + audiofile).toLocal8Bit().data();
            AudioReadStream *stream =
                AudioReadStreamFactory::createReadStream(filename);

            stream->setRetrievalSampleRate(readRate);
            int channels = stream->getChannelCount();
            AudioStreamTestData tdata(readRate, channels);

            // Our test audio files are all named
            // RATE-CHANNELS-BITDEPTH.ext (for PCM data) or
            // RATE-CHANNELS.ext (for lossy data)
            QStringList fileAndExt = audiofile.split(".");
            QStringList bits = fileAndExt[0].split("-");
            QString extension = fileAndExt[1];
            int nominalRate = bits[0].toInt();
            int nominalChannels = bits[1].toInt();
            int nominalDepth = 16;
            if (bits.length() > 2) nominalDepth = bits[2].toInt();

            QCOMPARE(channels, nominalChannels);

            QCOMPARE((int)stream->getSampleRate(), nominalRate);
            QCOMPARE((int)stream->getRetrievalSampleRate(), readRate);

            float *reference = tdata.getInterleavedData();
            int refFrames = tdata.getFrameCount();
            
            // The reader should give us exactly the expected number of
            // frames, except for mp3/aac files. We ask for quite a lot
            // more, though, so we can (a) check that we only get the
            // expected number back (if this is not mp3/aac) or (b) take
            // into account silence at beginning and end (if it is).
            int testFrames = refFrames + 5000;
            int testsize = testFrames * channels;
            
            float *test = new float[testsize];
	
            int read = stream->getInterleavedFrames(testFrames, test);

            if (extension == "mp3" || extension == "aac" || extension == "m4a") {
                // mp3s and aacs can have silence at start and end
                QVERIFY(read >= refFrames);
            } else {
                QCOMPARE(read, refFrames);
            }

            // Our limits are pretty relaxed -- we're not testing decoder
            // or resampler quality here, just whether the results are
            // plainly wrong (e.g. at wrong samplerate or with an offset)

            float limit = 0.01;
            float edgeLimit = limit * 10; // in first or final edgeSize frames
            int edgeSize = 100; 

            if (nominalDepth < 16) {
                limit = 0.02;
            }
            if (extension == "ogg" || extension == "mp3" ||
                extension == "aac" || extension == "m4a" ||
                extension == "opus") {
                limit = 0.2;
                edgeLimit = limit * 3;
            }

            // And we ignore completely the last few frames when upsampling
            int discard = 1 + readRate / nominalRate;

            int offset = 0;

            if (extension == "aac" || extension == "m4a") {
                // our m4a file appears to have a fixed offset of 1024 (at
                // file sample rate)
                offset = (1024 / float(nominalRate)) * readRate;
            }

            if (extension == "mp3") {
                // while mp3s appear to vary
                for (int i = 0; i < read; ++i) {
                    bool any = false;
                    float thresh = 0.01;
                    for (int c = 0; c < channels; ++c) {
                        if (fabsf(test[i * channels + c]) > thresh) {
                            any = true;
                            break;
                        }
                    }
                    if (any) {
                        offset = i;
                        break;
                    }
                }
            }

            for (int c = 0; c < channels; ++c) {
                float maxdiff = 0.f;
                int maxAt = 0;
                float totdiff = 0.f;
                for (int i = 0; i < read - offset - discard && i < refFrames; ++i) {
                    float diff = fabsf(test[(i + offset) * channels + c] -
                                       reference[i * channels + c]);
                    totdiff += diff;
                    // in edge areas, record this only if it exceeds edgeLimit
                    if (i < edgeSize || i + edgeSize >= read - offset) {
                        if (diff > edgeLimit) {
                            maxdiff = diff;
                            maxAt = i;
                        }
                    } else {
                        if (diff > maxdiff) {
                            maxdiff = diff;
                            maxAt = i;
                        }
                    }
                }
                float meandiff = totdiff / read;
                if (meandiff >= limit) {
                    cerr << "ERROR: for audiofile " << audiofile.toLocal8Bit().data() << ": mean diff = " << meandiff << " for channel " << c << endl;
                    QVERIFY(meandiff < limit);
                }
                if (maxdiff >= limit) {
                    cerr << "ERROR: for audiofile " << audiofile.toLocal8Bit().data() << ": max diff = " << maxdiff << " at frame " << maxAt << " of " << read << " on channel " << c << " (mean diff = " << meandiff << ")" << endl;
                    QVERIFY(maxdiff < limit);
                }
            }

            delete[] test;

        } catch (UnknownFileType &t) {
#if (QT_VERSION >= 0x050000)
            QSKIP(strOf(QString("File format for \"%1\" not supported, skipping").arg(audiofile)));
#else
            QSKIP(strOf(QString("File format for \"%1\" not supported, skipping").arg(audiofile)), SkipSingle);
#endif
        }
    }
};

}

#endif

            
