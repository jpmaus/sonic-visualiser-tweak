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

#ifndef TEST_WAVEFORM_OVERSAMPLER_H
#define TEST_WAVEFORM_OVERSAMPLER_H

#include "../WaveformOversampler.h"
#include "../WritableWaveFileModel.h"

#include "../../../base/BaseTypes.h"

#include <QObject>
#include <QtTest>

class TestWaveformOversampler : public QObject
{
    Q_OBJECT

public:
    TestWaveformOversampler() {
        m_source = floatvec_t(5000, 0.f);
        m_source[0] = 1.f;
        m_source[2500] = 0.5f;
        m_source[2501] = -0.5f;
        m_source[4999] = -1.f;
        for (int i = 3000; i < 3900; ++i) {
            m_source[i] = float(sin(double(i - 3000) * M_PI / 50.0));
        }
        m_sourceModel = new WritableWaveFileModel(8000, 1);
        const float *d = m_source.data();
        QVERIFY(m_sourceModel->addSamples(&d, m_source.size()));
        m_sourceModel->writeComplete();
    }

    ~TestWaveformOversampler() {
        delete m_sourceModel;
    }

private:
    floatvec_t m_source;
    WritableWaveFileModel *m_sourceModel;

    void compareStrided(floatvec_t obtained, floatvec_t expected, int stride) {
        QCOMPARE(obtained.size(), expected.size() * stride);
        float threshold = 1e-10f;
        for (int i = 0; in_range_for(expected, i); ++i) {
            if (fabsf(obtained[i * stride] - expected[i]) > threshold) {
                std::cerr << "At position " << i * stride << ": "
                          << obtained[i * stride] << " != " << expected[i]
                          << std::endl;
                QCOMPARE(obtained, expected);
            }
        }
    }

    void compareVecs(floatvec_t obtained, floatvec_t expected) {
        compareStrided(obtained, expected, 1);
    }

    floatvec_t get(sv_frame_t sourceStartFrame,
                   sv_frame_t sourceFrameCount,
                   int oversampleBy) {
        return WaveformOversampler::getOversampledData
            (*m_sourceModel, 0,
             sourceStartFrame, sourceFrameCount, oversampleBy);
    }
    
    void testVerbatim(sv_frame_t sourceStartFrame,
                      sv_frame_t sourceFrameCount,
                      int oversampleBy,
                      floatvec_t expected) {
        floatvec_t output =
            get(sourceStartFrame, sourceFrameCount, oversampleBy);
        compareVecs(output, expected);
    }

    void testStrided(sv_frame_t sourceStartFrame,
                     sv_frame_t sourceFrameCount,
                     int oversampleBy,
                     floatvec_t expected) {
        // check only the values that are expected to be precisely the
        // original samples
        floatvec_t output =
            get(sourceStartFrame, sourceFrameCount, oversampleBy);
        compareStrided(output, expected, oversampleBy);
    }

    floatvec_t sourceSubset(sv_frame_t start, sv_frame_t length) {
        return floatvec_t(m_source.begin() + start,
                          m_source.begin() + start + length);
    }

private slots:
    void testWholeVerbatim() {
        testVerbatim(0, 5000, 1, m_source);
    }

    void testSubsetsVerbatim() {
        testVerbatim(0, 500, 1, sourceSubset(0, 500));
        testVerbatim(4500, 500, 1, sourceSubset(4500, 500));
        testVerbatim(2000, 1000, 1, sourceSubset(2000, 1000));
    }

    void testOverlapsVerbatim() {
        // overlapping the start -> result should be zero-padded to
        // preserve start frame
        floatvec_t expected = sourceSubset(0, 400);
        expected.insert(expected.begin(), 100, 0.f);
        testVerbatim(-100, 500, 1, expected);

        // overlapping the end -> result should be truncated to
        // preserve source length
        expected = sourceSubset(4600, 400);
        testVerbatim(4600, 500, 1, expected);
    }

    void testWhole2x() {
        testStrided(0, 5000, 2, m_source);

        // check for windowed sinc values between the original samples
        floatvec_t output = get(0, 5000, 2);
        QVERIFY(output[1] - 0.6358 < 0.0001);
        QVERIFY(output[3] + 0.2099 < 0.0001);
    }
    
    void testWhole3x() {
        testStrided(0, 5000, 3, m_source);

        // check for windowed sinc values between the original samples
        floatvec_t output = get(0, 5000, 3);
        QVERIFY(output[1] > 0.7);
        QVERIFY(output[2] > 0.4);
        QVERIFY(output[4] < -0.1);
        QVERIFY(output[5] < -0.1);
    }
    
    void testWhole4x() {
        testStrided(0, 5000, 4, m_source);

        // check for windowed sinc values between the original samples
        floatvec_t output = get(0, 5000, 4);
        QVERIFY(output[1] - 0.9000 < 0.0001);
        QVERIFY(output[2] - 0.6358 < 0.0001);
        QVERIFY(output[3] - 0.2993 < 0.0001);
        QVERIFY(output[5] + 0.1787 < 0.0001);
        QVERIFY(output[6] + 0.2099 < 0.0001);
        QVERIFY(output[7] + 0.1267 < 0.0001);

        // alternate values at 2n should equal all values at n
        output = get(0, 5000, 4);
        floatvec_t half = get(0, 5000, 2);
        compareStrided(output, half, 2);
    }
    
    void testWhole8x() {
        testStrided(0, 5000, 8, m_source);

        // alternate values at 2n should equal all values at n
        floatvec_t output = get(0, 5000, 8);
        floatvec_t half = get(0, 5000, 4);
        compareStrided(output, half, 2);
    }
    
    void testWhole10x() {
        testStrided(0, 5000, 10, m_source);

        // alternate values at 2n should equal all values at n
        floatvec_t output = get(0, 5000, 10);
        floatvec_t half = get(0, 5000, 5);
        compareStrided(output, half, 2);
    }
    
    void testWhole16x() {
        testStrided(0, 5000, 16, m_source);

        // alternate values at 2n should equal all values at n
        floatvec_t output = get(0, 5000, 16);
        floatvec_t half = get(0, 5000, 8);
        compareStrided(output, half, 2);
    }
    
    void testSubsets4x() {
        testStrided(0, 500, 4, sourceSubset(0, 500));
        testStrided(4500, 500, 4, sourceSubset(4500, 500));
        testStrided(2000, 1000, 4, sourceSubset(2000, 1000));

        // check for windowed sinc values between the original
        // samples, even when the original sample that was the source
        // of this sinc kernel is not within the requested range
        floatvec_t output = get(1, 10, 4);
        QVERIFY(output[0] < 0.0001);
        QVERIFY(output[1] + 0.1787 < 0.0001);
        QVERIFY(output[2] + 0.2099 < 0.0001);
        QVERIFY(output[3] + 0.1267 < 0.0001);

        // and again at the end
        output = get(4989, 10, 4);
        QVERIFY(output[39] + 0.9000 < 0.0001);
        QVERIFY(output[38] + 0.6358 < 0.0001);
        QVERIFY(output[37] + 0.2993 < 0.0001);
        QVERIFY(output[35] - 0.1787 < 0.0001);
        QVERIFY(output[34] - 0.2099 < 0.0001);
        QVERIFY(output[33] - 0.1267 < 0.0001);
    }
    
    void testOverlaps4x() {
        // overlapping the start -> result should be zero-padded to
        // preserve start frame
        floatvec_t expected = sourceSubset(0, 400);
        expected.insert(expected.begin(), 100, 0.f);
        testStrided(-100, 500, 4, expected);

        // overlapping the end -> result should be truncated to
        // preserve source length
        expected = sourceSubset(4600, 400);
        testStrided(4600, 500, 4, expected);
    }

    void testSubsets15x() {
        testStrided(0, 500, 15, sourceSubset(0, 500));
        testStrided(4500, 500, 15, sourceSubset(4500, 500));
        testStrided(2000, 1000, 15, sourceSubset(2000, 1000));
    }
    
    void testOverlaps15x() {
        // overlapping the start -> result should be zero-padded to
        // preserve start frame
        floatvec_t expected = sourceSubset(0, 400);
        expected.insert(expected.begin(), 100, 0.f);
        testStrided(-100, 500, 15, expected);

        // overlapping the end -> result should be truncated to
        // preserve source length
        expected = sourceSubset(4600, 400);
        testStrided(4600, 500, 15, expected);
    }
};


#endif
