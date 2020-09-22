/* -*- c-basic-offset: 4 indent-tabs-mode: nil -*-  vi:set ts=8 sts=4 sw=4: */

/*
    Sonic Visualiser
    An audio file viewer and annotation editor.
    Centre for Digital Music, Queen Mary, University of London.
    This file copyright 2017 Queen Mary, University of London.
    
    This program is free software; you can redistribute it and/or
    modify it under the terms of the GNU General Public License as
    published by the Free Software Foundation; either version 2 of the
    License, or (at your option) any later version.  See the file
    COPYING included with this distribution for more information.
*/

#ifndef TEST_CSV_STREAM_H
#define TEST_CSV_STREAM_H

#include <QtTest>
#include <QObject>
#include <sstream>
#include <functional>

#include "base/ProgressReporter.h"
#include "base/DataExportOptions.h"
#include "base/Selection.h"
#include "data/model/NoteModel.h"
#include "../CSVStreamWriter.h"
#include "../../model/test/MockWaveModel.h"

class StubReporter : public ProgressReporter
{
public:
    StubReporter( std::function<bool()> isCancelled )
        : m_isCancelled(isCancelled) {}
    bool isDefinite() const override { return true; }
    void setDefinite(bool) override {}
    bool wasCancelled() const override { return m_isCancelled(); }
    void setMessage(QString) override {}
    void setProgress(int p) override
    { 
        ++m_calls;
        m_percentageLog.push_back(p);
    }

    size_t getCallCount() const { return m_calls; }
    std::vector<int> getPercentageLog() const { return m_percentageLog; }
    void reset() { m_calls = 0; }
private:
    size_t m_calls = 0;
    std::function<bool()> m_isCancelled;
    std::vector<int> m_percentageLog;
};

class CSVStreamWriterTest : public QObject
{
    Q_OBJECT
public:
    std::string getExpectedString()
    {
        return
        {
          "0,0,0\n"
          "1,0,0\n"
          "2,0,0\n"
          "3,0,0\n"
          "4,1,1\n"
          "5,1,1\n"
          "6,1,1\n"
          "7,1,1\n"
          "8,1,1\n"
          "9,1,1\n"
          "10,1,1\n"
          "11,1,1\n"
          "12,1,1\n"
          "13,1,1\n"
          "14,1,1\n"
          "15,1,1\n"
          "16,1,1\n"
          "17,1,1\n"
          "18,1,1\n"
          "19,1,1\n"
          "20,0,0\n"
          "21,0,0\n"
          "22,0,0\n"
          "23,0,0"
        };
    }

private slots:
    void simpleValidOutput()
    {
        MockWaveModel mwm({ DC, DC }, 16, 4);

        std::ostringstream oss;
        const auto result = CSVStreamWriter::writeInChunks(oss, mwm);
        QVERIFY( oss.str() == getExpectedString() );
        QVERIFY( result );
    }

    void callsReporterCorrectTimes()
    {
        MockWaveModel mwm({ DC, DC }, 16, 4);
        StubReporter reporter { []() -> bool { return false; } };
        const auto expected = getExpectedString();

        std::ostringstream oss;
        const auto writeStreamWithBlockSize = [&](int blockSize) {
            return CSVStreamWriter::writeInChunks(
                oss,
                mwm,
                &reporter,
                ",",
                DataExportDefaults,
                blockSize
            );
        };

        const auto reset = [&]() {
            oss.str({});
            reporter.reset();
        };

        const auto nonIntegerMultipleResult = writeStreamWithBlockSize(5);
        QVERIFY( nonIntegerMultipleResult );
        QVERIFY( reporter.getCallCount() == 5 /* 4.8 rounded up */ );
        QVERIFY( oss.str() == expected );
        reset();

        const auto integerMultiple = writeStreamWithBlockSize(2);
        QVERIFY( integerMultiple );
        QVERIFY( reporter.getCallCount() == 12 );
        QVERIFY( oss.str() == expected );
        reset();

        const auto largerThanNumberOfSamples = writeStreamWithBlockSize(100);
        QVERIFY( largerThanNumberOfSamples );
        QVERIFY( reporter.getCallCount() == 1 );
        QVERIFY( oss.str() == expected );
        reset();

        const auto zero = writeStreamWithBlockSize(0);
        QVERIFY( zero == false );
        QVERIFY( reporter.getCallCount() == 0 );
    }

    void isCancellable()
    {
        MockWaveModel mwm({ DC, DC }, 16, 4);
        StubReporter reporter { []() -> bool { return true; } };

        std::ostringstream oss;
        const auto cancelImmediately = CSVStreamWriter::writeInChunks(
            oss,
            mwm,
            &reporter,
            ",",
            DataExportDefaults,
            4
        );
        QVERIFY( cancelImmediately == false );
        QVERIFY( reporter.getCallCount() == 0 );

        StubReporter cancelMidway { 
            [&]() { return cancelMidway.getCallCount() == 3; } 
        };
        const auto cancelledMidway = CSVStreamWriter::writeInChunks(
            oss,
            mwm,
            &cancelMidway,
            ",",
            DataExportDefaults,
            4
        );
        QVERIFY( cancelMidway.getCallCount() == 3 );
        QVERIFY( cancelledMidway == false );
    }

    void zeroStartTimeReportsPercentageCorrectly()
    {
        MockWaveModel mwm({ DC, DC }, 16, 4);
        StubReporter reporter { []() -> bool { return false; } };
        std::ostringstream oss;
        const auto succeeded = CSVStreamWriter::writeInChunks(
            oss,
            mwm,
            &reporter,
            ",",
            DataExportDefaults,
            4
        );
        QVERIFY( succeeded == true );
        QVERIFY( reporter.getCallCount() == 6 );
        const std::vector<int> expectedCallLog {
            16,
            33,
            50,
            66,
            83,
            100
        };
        QVERIFY( reporter.getPercentageLog() == expectedCallLog );
        QVERIFY( oss.str() == getExpectedString() );
    }

    void nonZeroStartTimeReportsPercentageCorrectly()
    {
        MockWaveModel mwm({ DC, DC }, 16, 4);
        StubReporter reporter { []() -> bool { return false; } };
        std::ostringstream oss;
        const auto writeSubSection = CSVStreamWriter::writeInChunks(
            oss,
            mwm,
            {4, 20},
            &reporter,
            ",",
            DataExportDefaults,
            4
        );
        QVERIFY( reporter.getCallCount() == 4 );
        const std::vector<int> expectedCallLog {
            25,
            50,
            75,
            100
        };
        QVERIFY( reporter.getPercentageLog() == expectedCallLog );
        QVERIFY( writeSubSection == true );
        const std::string expectedOutput {
          "4,1,1\n"
          "5,1,1\n"
          "6,1,1\n"
          "7,1,1\n"
          "8,1,1\n"
          "9,1,1\n"
          "10,1,1\n"
          "11,1,1\n"
          "12,1,1\n"
          "13,1,1\n"
          "14,1,1\n"
          "15,1,1\n"
          "16,1,1\n"
          "17,1,1\n"
          "18,1,1\n"
          "19,1,1"
        };
        QVERIFY( oss.str() == expectedOutput );
    }

    void multipleSelectionOutput()
    {
        MockWaveModel mwm({ DC, DC }, 16, 4);
        StubReporter reporter { []() -> bool { return false; } };
        std::ostringstream oss;
        MultiSelection regions;
        regions.addSelection({0, 2});
        regions.addSelection({4, 6});
        regions.addSelection({16, 18});
//        qDebug("End frame: %lld", (long long int)mwm.getEndFrame());
        const std::string expectedOutput {
          "0,0,0\n"
          "1,0,0\n"
          "4,1,1\n"
          "5,1,1\n"
          "16,1,1\n"
          "17,1,1"
        };
        const auto wroteMultiSection = CSVStreamWriter::writeInChunks(
            oss,
            mwm,
            regions,
            &reporter,
            ",",
            DataExportDefaults,
            2
        );
        QVERIFY( wroteMultiSection == true );
        QVERIFY( reporter.getCallCount() == 3 );
        const std::vector<int> expectedCallLog { 33, 66, 100 };
        QVERIFY( reporter.getPercentageLog() == expectedCallLog );
//        qDebug("%s", oss.str().c_str());
        QVERIFY( oss.str() == expectedOutput );
    }

    void writeSparseModel()
    {
        const auto pentatonicFromRoot = [](float midiPitch) {
            return std::vector<float> {
                0 + midiPitch,
                2 + midiPitch,
                4 + midiPitch,
                7 + midiPitch,
                9 + midiPitch
            };
        };
        const auto cMajorPentatonic = pentatonicFromRoot(60.0);
        NoteModel notes(8 /* sampleRate */, 4 /* resolution */);
        sv_frame_t startFrame = 0;
        for (const auto& note : cMajorPentatonic) {
            notes.add({startFrame, note, 4, 1.f, ""});
            startFrame += 8;
        }
//        qDebug("Create Expected Output\n");

        // NB. removed end line break
        const auto expectedOutput =
            notes.toDelimitedDataString(",", {}, 0, notes.getEndFrame())
            .trimmed();

        StubReporter reporter { []() -> bool { return false; } };
        std::ostringstream oss;
//        qDebug("End frame: %lld", (long long int)notes.getEndFrame());
//        qDebug("Write streaming\n");
        const auto wroteSparseModel = CSVStreamWriter::writeInChunks(
            oss,
            notes,
            &reporter,
            ",",
            DataExportDefaults,
            2
        );

//        qDebug("\n->>%s<<-\n", expectedOutput.toLocal8Bit().data());
//        qDebug("\n->>%s<<-\n", oss.str().c_str());
        QVERIFY( wroteSparseModel == true );
        QVERIFY( oss.str() != std::string() );
        QVERIFY( oss.str() == expectedOutput.toStdString() );
    }
};

#endif
