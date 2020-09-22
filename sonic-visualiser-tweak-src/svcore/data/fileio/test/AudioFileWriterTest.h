/* -*- c-basic-offset: 4 indent-tabs-mode: nil -*-  vi:set ts=8 sts=4 sw=4: */

/*
    Sonic Visualiser
    An audio file viewer and annotation editor.
    Centre for Digital Music, Queen Mary, University of London.
    
    This program is free software; you can redistribute it and/or
    modify it under the terms of the GNU General Public License as
    published by the Free Software Foundation; either version 2 of the
    License, or (at your option) any later version.  See the file
    COPYING included with this distribution for more information.
*/

#ifndef TEST_AUDIO_FILE_WRITER_H
#define TEST_AUDIO_FILE_WRITER_H

#include "../AudioFileReaderFactory.h"
#include "../AudioFileReader.h"
#include "../WavFileWriter.h"

#include "AudioTestData.h"

#include "bqvec/VectorOps.h"
#include "bqvec/Allocators.h"

#include <cmath>

#include <QObject>
#include <QtTest>
#include <QDir>

#include <iostream>

using namespace std;
using namespace breakfastquay;

class AudioFileWriterTest : public QObject
{
    Q_OBJECT

private:
    QString testDirBase;
    QString outDir;

    static const int rate = 44100;
    
public:
    AudioFileWriterTest(QString base) {
        if (base == "") {
            base = "svcore/data/fileio/test";
        }
        testDirBase = base;
        outDir = base + "/outfiles";
    }

    const char *strOf(QString s) {
        return strdup(s.toLocal8Bit().data());
    }
    
    QString testName(bool direct, int channels) {
        return QString("%1 %2 %3")
            .arg(channels)
            .arg(channels > 1 ? "channels" : "channel")
            .arg(direct ? "direct" : "via temporary");
    }

private slots:
    void init()
    {
        if (!QDir(outDir).exists() && !QDir().mkpath(outDir)) {
            SVCERR << "ERROR: Audio out directory \"" << outDir << "\" does not exist and could not be created" << endl;
            QVERIFY2(QDir(outDir).exists(), "Audio out directory not found and could not be created");
        }
    }

    void write_data()
    {
        QTest::addColumn<bool>("direct");
        QTest::addColumn<int>("channels");
        for (int direct = 0; direct <= 1; ++direct) {
            for (int channels = 1; channels < 8; ++channels) {
                if (channels == 1 || channels == 2 ||
                    channels == 5 || channels == 8) {
                    QString desc = testName(direct, channels);
                    QTest::newRow(strOf(desc)) << (bool)direct << channels;
                }
            }
        }
    }
    
    void write()
    {
        QFETCH(bool, direct);
        QFETCH(int, channels);

        QString outfile = QString("%1/out-%2ch-%3.wav")
            .arg(outDir).arg(channels).arg(direct ? "direct" : "via-temporary");
        
        WavFileWriter writer(outfile,
                             rate,
                             channels,
                             direct ?
                             WavFileWriter::WriteToTarget :
                             WavFileWriter::WriteToTemporary);
        QVERIFY(writer.isOK());

        AudioTestData data(rate, channels);
        data.generate();

        sv_frame_t frameCount = data.getFrameCount();
        float *interleaved = data.getInterleavedData();
        float **nonInterleaved = allocate_channels<float>(channels, frameCount);
        v_deinterleave(nonInterleaved, interleaved, channels, int(frameCount));
        bool ok = writer.writeSamples(nonInterleaved, frameCount);
        deallocate_channels(nonInterleaved, channels);
        QVERIFY(ok);
        
        ok = writer.close();
        QVERIFY(ok);

        AudioFileReaderFactory::Parameters params;
        AudioFileReader *rereader =
            AudioFileReaderFactory::createReader(outfile, params);
        QVERIFY(rereader != nullptr);
        
        floatvec_t readFrames = rereader->getInterleavedFrames(0, frameCount);
        floatvec_t expected(interleaved, interleaved + frameCount * channels);
        QCOMPARE(readFrames, expected);

        delete rereader;
    }
};

#endif
