/* -*- c-basic-offset: 4 indent-tabs-mode: nil -*-  vi:set ts=8 sts=4 sw=4: */

/*
    Sonic Visualiser
    An audio file viewer and annotation editor.
    Centre for Digital Music, Queen Mary, University of London.
    This file copyright 2013 Chris Cannam.
    
    This program is free software; you can redistribute it and/or
    modify it under the terms of the GNU General Public License as
    published by the Free Software Foundation; either version 2 of the
    License, or (at your option) any later version.  See the file
    COPYING included with this distribution for more information.
*/

#ifndef TEST_AUDIO_FILE_READER_H
#define TEST_AUDIO_FILE_READER_H

#include "../AudioFileReaderFactory.h"
#include "../AudioFileReader.h"
#include "../WavFileWriter.h"

#include "AudioTestData.h"
#include "UnsupportedFormat.h"

#include <cmath>

#include <QObject>
#include <QtTest>
#include <QDir>

#include <iostream>

using namespace std;

class AudioFileReaderTest : public QObject
{
    Q_OBJECT

private:
    QString testDirBase;
    QString audioDir;
    QString diffDir;

public:
    AudioFileReaderTest(QString base) {
        if (base == "") {
            base = "svcore/data/fileio/test";
        }
        testDirBase = base;
        audioDir = base + "/audio";
        diffDir = base + "/diffs";
    }

private:
    const char *strOf(QString s) {
        return strdup(s.toLocal8Bit().data());
    }

    void getFileMetadata(QString filename,
                         QString &extension,
                         sv_samplerate_t &rate,
                         int &channels,
                         int &bitdepth) {

        QStringList fileAndExt = filename.split(".");
        QStringList bits = fileAndExt[0].split("-");

        extension = fileAndExt[1];
        rate = bits[0].toInt();
        channels = bits[1].toInt();
        bitdepth = 16;
        if (bits.length() > 2) {
            bitdepth = bits[2].toInt();
        }
    }

    void getExpectedThresholds(QString format,
                               QString filename,
                               bool resampled,
                               bool gapless,
                               bool normalised,
                               double &maxLimit,
                               double &rmsLimit) {

        QString extension;
        sv_samplerate_t fileRate;
        int channels;
        int bitdepth;
        getFileMetadata(filename, extension, fileRate, channels, bitdepth);
        
        if (normalised) {

            if (format == "ogg") {

                // Our ogg is not especially high quality and is
                // actually further from the original if normalised

                maxLimit = 0.1;
                rmsLimit = 0.03;

            } else if (format == "opus") {

                maxLimit = 0.06;
                rmsLimit = 0.015;

            } else if (format == "aac") {

                // Terrible performance for this test, load of spill
                // from one channel to the other. I guess they know
                // what they're doing, it's perceptual after all, but
                // it does make this check a bit superfluous, you
                // could probably pass it with a signal that sounds
                // nothing like the original
                maxLimit = 0.2;
                rmsLimit = 0.1;

            } else if (format == "wma") {

                maxLimit = 0.05;
                rmsLimit = 0.01;

            } else if (format == "mp3") {

                if (resampled && !gapless) {

                    // We expect worse figures here, because the
                    // combination of uncompensated encoder delay +
                    // resampling results in a fractional delay which
                    // means the decoded signal is slightly out of
                    // phase compared to the test signal

                    maxLimit = 0.1;
                    rmsLimit = 0.05;

                } else {

                    maxLimit = 0.05;
                    rmsLimit = 0.01;
                }

            } else {

                // lossless formats (wav, aiff, flac, apple_lossless)
                
                if (bitdepth >= 16 && !resampled) {
                    maxLimit = 1e-3;
                    rmsLimit = 3e-4;
                } else {
                    maxLimit = 0.01;
                    rmsLimit = 5e-3;
                }
            }
            
        } else { // !normalised
            
            if (format == "ogg") {

                maxLimit = 0.06;
                rmsLimit = 0.03;

            } else if (format == "opus") {

                maxLimit = 0.06;
                rmsLimit = 0.015;

            } else if (format == "aac") {

                maxLimit = 0.2;
                rmsLimit = 0.1;

            } else if (format == "wma") {

                maxLimit = 0.05;
                rmsLimit = 0.01;

            } else if (format == "mp3") {

                // all mp3 figures are worse when not normalising
                maxLimit = 0.1;
                rmsLimit = 0.05;

            } else {

                // lossless formats (wav, aiff, flac, apple_lossless)
                
                if (bitdepth >= 16 && !resampled) {
                    maxLimit = 1e-3;
                    rmsLimit = 3e-4;
                } else {
                    maxLimit = 0.02;
                    rmsLimit = 0.01;
                }
            }
        }
    }

    QString testName(QString format, QString filename, int rate, bool norm, bool gapless) {
        return QString("%1/%2 at %3%4%5")
            .arg(format)
            .arg(filename)
            .arg(rate)
            .arg(norm ? " normalised": "")
            .arg(gapless ? "" : " non-gapless");
    }

private slots:
    void init()
    {
        if (!QDir(audioDir).exists()) {
            QString cwd = QDir::currentPath();
            SVCERR << "ERROR: Audio test file directory \"" << audioDir << "\" does not exist (cwd = " << cwd << ")" << endl;
            QVERIFY2(QDir(audioDir).exists(), "Audio test file directory not found");
        }
        if (!QDir(diffDir).exists() && !QDir().mkpath(diffDir)) {
            SVCERR << "ERROR: Audio diff directory \"" << diffDir << "\" does not exist and could not be created" << endl;
            QVERIFY2(QDir(diffDir).exists(), "Audio diff directory not found and could not be created");
        }
    }

    void read_data()
    {
        QTest::addColumn<QString>("format");
        QTest::addColumn<QString>("audiofile");
        QTest::addColumn<int>("rate");
        QTest::addColumn<bool>("normalised");
        QTest::addColumn<bool>("gapless");
        QStringList dirs = QDir(audioDir).entryList(QDir::Dirs |
                                                    QDir::NoDotAndDotDot);
        for (QString format: dirs) {
            QStringList files = QDir(QDir(audioDir).filePath(format))
                .entryList(QDir::Files);
            int readRates[] = { 44100, 48000 };
            bool norms[] = { false, true };
            bool gaplesses[] = { true, false };
            foreach (QString filename, files) {
                for (int rate: readRates) {
                    for (bool norm: norms) {
                        for (bool gapless: gaplesses) {

#ifdef Q_OS_WIN
                            if (format == "aac") {
                                if (gapless) {
                                    // Apparently no support for AAC
                                    // encoder delay compensation in
                                    // MediaFoundation, so these tests
                                    // are only available non-gapless
                                    continue;
                                }
                            } else if (format != "mp3") {
                                if (!gapless) {
                                    // All other formats but mp3 are
                                    // intrinsically gapless, so we
                                    // can skip the non-gapless option
                                    continue;
                                }
                            }
#else
                            if (format != "mp3") {
                                if (!gapless) {
                                    // All other formats but mp3 are
                                    // intrinsically gapless
                                    // everywhere except for Windows
                                    // (see above), so we can skip the
                                    // non-gapless option
                                    continue;
                                }                                    
                            }
#endif
                        
                            QString desc = testName
                                (format, filename, rate, norm, gapless);

                            QTest::newRow(strOf(desc))
                                << format << filename << rate << norm << gapless;
                        }
                    }
                }
            }
        }
    }

    void read()
    {
        QFETCH(QString, format);
        QFETCH(QString, audiofile);
        QFETCH(int, rate);
        QFETCH(bool, normalised);
        QFETCH(bool, gapless);

        sv_samplerate_t readRate(rate);
        
//        cerr << "\naudiofile = " << audiofile << endl;

        AudioFileReaderFactory::Parameters params;
        params.targetRate = readRate;
        params.normalisation = (normalised ?
                                AudioFileReaderFactory::Normalisation::Peak :
                                AudioFileReaderFactory::Normalisation::None);
        params.gaplessMode = (gapless ?
                              AudioFileReaderFactory::GaplessMode::Gapless :
                              AudioFileReaderFactory::GaplessMode::Gappy);

        AudioFileReader *reader =
            AudioFileReaderFactory::createReader
            (audioDir + "/" + format + "/" + audiofile, params);
        
        if (!reader) {
            if (UnsupportedFormat::isLegitimatelyUnsupported(format)) {
#if ( QT_VERSION >= 0x050000 )
                QSKIP("Unsupported file, skipping");
#else
                QSKIP("Unsupported file, skipping", SkipSingle);
#endif
            }
        }

        QVERIFY(reader != nullptr);

        QString extension;
        sv_samplerate_t fileRate;
        int channels;
        int fileBitdepth;
        getFileMetadata(audiofile, extension, fileRate, channels, fileBitdepth);
        
        QCOMPARE((int)reader->getChannelCount(), channels);
        QCOMPARE(reader->getNativeRate(), fileRate);
        QCOMPARE(reader->getSampleRate(), readRate);

        AudioTestData tdata(readRate, channels);
        
        float *reference = tdata.getInterleavedData();
        sv_frame_t refFrames = tdata.getFrameCount();
        
        // The reader should give us exactly the expected number of
        // frames, except for mp3/aac files. We ask for quite a lot
        // more, though, so we can (a) check that we only get the
        // expected number back (if this is not mp3/aac) or (b) take
        // into account silence at beginning and end (if it is).
        floatvec_t test = reader->getInterleavedFrames(0, refFrames + 5000);

        delete reader;
        reader = 0;
        
        sv_frame_t read = test.size() / channels;

        bool perceptual = (extension == "mp3" ||
                           extension == "aac" ||
                           extension == "m4a" ||
                           extension == "wma" ||
                           extension == "opus");
        
        if (perceptual && !gapless) {
            // allow silence at start and end
            QVERIFY(read >= refFrames);
        } else {
            QCOMPARE(read, refFrames);
        }

        bool resampled = readRate != fileRate;
        double maxLimit, rmsLimit;
        getExpectedThresholds(format,
                              audiofile,
                              resampled,
                              gapless,
                              normalised,
                              maxLimit, rmsLimit);
        
        double edgeLimit = maxLimit * 3; // in first or final edgeSize frames
        if (resampled && edgeLimit < 0.1) edgeLimit = 0.1;
        int edgeSize = 100; 

        // And we ignore completely the last few frames when upsampling
        int discard = 1 + int(round(readRate / fileRate));

        int offset = 0;

        if (perceptual) {

            // Look for an initial offset.
            //
            // We know the first channel has a sinusoid in it. It
            // should have a peak at 0.4ms (see AudioTestData.h) but
            // that might have been clipped, which would make it
            // imprecise. We can tell if it's clipped, though, as
            // there will be samples having exactly identical
            // values. So what we look for is the peak if it's not
            // clipped and, if it is, the first zero crossing after
            // the peak, which should be at 0.8ms.

            int expectedPeak = int(0.0004 * readRate);
            int expectedZC = int(0.0008 * readRate);
            bool foundPeak = false;
            for (int i = 1; i+1 < read; ++i) {
                float prevSample = test[(i-1) * channels];
                float thisSample = test[i * channels];
                float nextSample = test[(i+1) * channels];
                if (thisSample > 0.8 && nextSample < thisSample) {
                    foundPeak = true;
                    if (thisSample > prevSample) {
                        // not clipped
                        offset = i - expectedPeak - 1;
                        break;
                    }
                }
                if (foundPeak && (thisSample >= 0.0 && nextSample < 0.0)) {
//                    cerr << "thisSample = " << thisSample << ", nextSample = "
//                         << nextSample << endl;
                    offset = i - expectedZC - 1;
                    break;
                }
            }

//            int fileRateEquivalent = int((offset / readRate) * fileRate);
//            std::cerr << "offset = " << offset << std::endl;
//            std::cerr << "at file rate would be " << fileRateEquivalent << std::endl;

            // Previously our m4a test file had a fixed offset of 1024
            // at the file sample rate -- this may be because it was
            // produced by FAAC which did not write in the delay as
            // metadata? We now have an m4a produced by Core Audio
            // which gives a 0 offset. What to do...

            // Anyway, mp3s should have 0 offset in gapless mode and
            // "something else" otherwise.
            
            if (gapless) {
                if (format == "aac"
#ifdef Q_OS_WIN
                    || (format == "mp3" && (readRate != fileRate))
#endif
                    ) {
                    // ouch!
                    if (offset == -1) offset = 0;
                }
                QCOMPARE(offset, 0);
            }
        }

        {
            // Write the diff file now, so that it's already been written
            // even if the comparison fails. We aren't checking anything
            // here except as necessary to avoid buffer overruns etc

            QString diffFile =
                testName(format, audiofile, rate, normalised, gapless);
            diffFile.replace("/", "_");
            diffFile.replace(".", "_");
            diffFile.replace(" ", "_");
            diffFile += ".wav";
            diffFile = QDir(diffDir).filePath(diffFile);
            WavFileWriter diffWriter(diffFile, readRate, channels,
                                     WavFileWriter::WriteToTemporary);
            QVERIFY(diffWriter.isOK());

            vector<vector<float>> diffs(channels);
            for (int c = 0; c < channels; ++c) {
                for (int i = 0; i < refFrames; ++i) {
                    int ix = i + offset;
                    if (ix < read) {
                        float signeddiff =
                            test[ix * channels + c] -
                            reference[i * channels + c];
                        diffs[c].push_back(signeddiff);
                    }
                }
            }
            float **ptrs = new float*[channels];
            for (int c = 0; c < channels; ++c) {
                ptrs[c] = diffs[c].data();
            }
            diffWriter.writeSamples(ptrs, refFrames);
            delete[] ptrs;
        }
            
        for (int c = 0; c < channels; ++c) {

            double maxDiff = 0.0;
            double totalDiff = 0.0;
            double totalSqrDiff = 0.0;
            int maxIndex = 0;

            for (int i = 0; i < refFrames; ++i) {
                int ix = i + offset;
                if (ix >= read) {
                    SVCERR << "ERROR: audiofile " << audiofile << " reads truncated (read-rate reference frames " << i << " onward, of " << refFrames << ", are lost)" << endl;
                    QVERIFY(ix < read);
                }

                if (ix + discard >= read) {
                    // we forgive the very edge samples when
                    // resampling (discard > 0)
                    continue;
                }
                
                double diff = fabs(test[ix * channels + c] -
                                   reference[i * channels + c]);

                totalDiff += diff;
                totalSqrDiff += diff * diff;
                
                // in edge areas, record this only if it exceeds edgeLimit
                if (i < edgeSize || i + edgeSize >= refFrames) {
                    if (diff > edgeLimit && diff > maxDiff) {
                        maxDiff = diff;
                        maxIndex = i;
                    }
                } else {
                    if (diff > maxDiff) {
                        maxDiff = diff;
                        maxIndex = i;
                    }
                }
            }
                
            double meanDiff = totalDiff / double(refFrames);
            double rmsDiff = sqrt(totalSqrDiff / double(refFrames));

            /*
        cerr << "channel " << c << ": mean diff " << meanDiff << endl;
            cerr << "channel " << c << ":  rms diff " << rmsDiff << endl;
            cerr << "channel " << c << ":  max diff " << maxDiff << " at " << maxIndex << endl;
            */            
            if (rmsDiff >= rmsLimit) {
                SVCERR << "ERROR: for audiofile " << audiofile << ": RMS diff = " << rmsDiff << " for channel " << c << " (limit = " << rmsLimit << ")" << endl;
                QVERIFY(rmsDiff < rmsLimit);
            }
            if (maxDiff >= maxLimit) {
                SVCERR << "ERROR: for audiofile " << audiofile << ": max diff = " << maxDiff << " at frame " << maxIndex << " of " << read << " on channel " << c << " (limit = " << maxLimit << ", edge limit = " << edgeLimit << ", mean diff = " << meanDiff << ", rms = " << rmsDiff << ")" << endl;
                QVERIFY(maxDiff < maxLimit);
            }

            // and check for spurious material at end
            
            for (sv_frame_t i = refFrames; i + offset < read; ++i) {
                sv_frame_t ix = i + offset;
                float quiet = 0.1f; //!!! allow some ringing - but let's come back to this, it should tail off
                float mag = fabsf(test[ix * channels + c]);
                if (mag > quiet) {
                    SVCERR << "ERROR: audiofile " << audiofile << " contains spurious data after end of reference (found sample " << test[ix * channels + c] << " at index " << ix << " of channel " << c << " after reference+offset ended at " << refFrames+offset << ")" << endl;
                    QVERIFY(mag < quiet);
                }
            }
        }
    }
};

#endif
