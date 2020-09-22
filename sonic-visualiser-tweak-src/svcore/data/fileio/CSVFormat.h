/* -*- c-basic-offset: 4 indent-tabs-mode: nil -*-  vi:set ts=8 sts=4 sw=4: */

/*
    Sonic Visualiser
    An audio file viewer and annotation editor.
    Centre for Digital Music, Queen Mary, University of London.
    This file copyright 2006 Chris Cannam.
    
    This program is free software; you can redistribute it and/or
    modify it under the terms of the GNU General Public License as
    published by the Free Software Foundation; either version 2 of the
    License, or (at your option) any later version.  See the file
    COPYING included with this distribution for more information.
*/

#ifndef SV_CSV_FORMAT_H
#define SV_CSV_FORMAT_H

#include <QString>
#include <QStringList>

#include <set>

#include "base/BaseTypes.h"

class CSVFormat
{
public:
    enum ModelType {
        OneDimensionalModel,
        TwoDimensionalModel,
        TwoDimensionalModelWithDuration,
        TwoDimensionalModelWithDurationAndPitch,
        TwoDimensionalModelWithDurationAndExtent,
        ThreeDimensionalModel,
        WaveFileModel
    };
    
    enum TimingType {
        ExplicitTiming,
        ImplicitTiming
    };

    enum TimeUnits {
        TimeSeconds,
        TimeMilliseconds,
        TimeAudioFrames,
        TimeWindows,
    };

    enum ColumnPurpose {
        ColumnUnknown,
        ColumnStartTime,
        ColumnEndTime,
        ColumnDuration,
        ColumnValue,
        ColumnPitch,
        ColumnLabel
    };

    enum ColumnQuality {
        ColumnNumeric    = 1,   // No non-numeric values were seen in sample
        ColumnIntegral   = 2,   // All sampled values were integers
        ColumnIncreasing = 4,   // Sampled values were monotonically increasing
        ColumnSmall      = 8,   // All sampled values had magnitude < 1
        ColumnLarge      = 16,  // Values "quickly" grew to over 1000
        ColumnSigned     = 32,  // Some negative values were seen
        ColumnNearEmpty  = 64,  // Nothing in this column beyond first row
    };
    typedef unsigned int ColumnQualities;

    enum AudioSampleRange {
        SampleRangeSigned1 = 0, //     -1 .. 1
        SampleRangeUnsigned255, //      0 .. 255
        SampleRangeSigned32767, // -32768 .. 32767
        SampleRangeOther        // Other/unknown: Normalise on load
    };

    CSVFormat() : // arbitrary defaults
        m_modelType(TwoDimensionalModel),
        m_timingType(ExplicitTiming),
        m_timeUnits(TimeSeconds),
        m_separator(""),
        m_sampleRate(44100),
        m_windowSize(1024),
        m_columnCount(0),
        m_variableColumnCount(false),
        m_audioSampleRange(SampleRangeOther),
        m_allowQuoting(true),
        m_maxExampleCols(0)
    { }

    CSVFormat(QString path); // guess format

    /**
     * Guess the format of the given CSV file, setting the fields in
     * this object accordingly.  If the current separator is the empty
     * string, the separator character will also be guessed; otherwise
     * the current separator will be used.  The other properties of
     * this object will be set according to guesses from the file.
     *
     * The properties that are guessed from the file contents are:
     * separator, column count, variable-column-count flag, audio
     * sample range, timing type, time units, column qualities, column
     * purposes, and model type. The sample rate and window size
     * cannot be guessed and will not be changed by this function.
     * Note also that this function will never guess WaveFileModel for
     * the model type.
     *
     * Return false if there is some fundamental error, e.g. the file
     * could not be opened at all. Return true otherwise. Note that
     * this function returns true even if the file doesn't appear to
     * make much sense as a data format.
     */
    bool guessFormatFor(QString path);
 
    ModelType    getModelType()     const { return m_modelType;     }
    TimingType   getTimingType()    const { return m_timingType;    }
    TimeUnits    getTimeUnits()     const { return m_timeUnits;     }
    sv_samplerate_t getSampleRate() const { return m_sampleRate;    }
    int          getWindowSize()    const { return m_windowSize;    }
    int          getColumnCount()   const { return m_columnCount;   }
    AudioSampleRange getAudioSampleRange() const { return m_audioSampleRange; }
    bool         getAllowQuoting()  const { return m_allowQuoting;  }
    QChar        getSeparator()     const { 
        if (m_separator == "") return ',';
        else return m_separator[0];
    }
    // set rather than QSet to ensure a fixed order
    std::set<QChar> getPlausibleSeparators() const {
        return m_plausibleSeparators;
    }

    void setModelType(ModelType t)        { m_modelType    = t; }
    void setTimingType(TimingType t)      { m_timingType   = t; }
    void setTimeUnits(TimeUnits t)        { m_timeUnits    = t; }
    void setSeparator(QChar s)            { m_separator    = s; }
    void setSampleRate(sv_samplerate_t r) { m_sampleRate   = r; }
    void setWindowSize(int s)             { m_windowSize   = s; }
    void setColumnCount(int c)            { m_columnCount  = c; }
    void setAudioSampleRange(AudioSampleRange r) { m_audioSampleRange = r; }
    void setAllowQuoting(bool q)          { m_allowQuoting = q; }

    QList<ColumnPurpose> getColumnPurposes() const { return m_columnPurposes; }
    void setColumnPurposes(QList<ColumnPurpose> cl) { m_columnPurposes = cl; }

    ColumnPurpose getColumnPurpose(int i);
    ColumnPurpose getColumnPurpose(int i) const;
    void setColumnPurpose(int i, ColumnPurpose p);
    
    // read-only; only valid if format has been guessed:
    const QList<ColumnQualities> &getColumnQualities() const {
        return m_columnQualities;
    }

    // read-only; only valid if format has been guessed:
    const QList<QStringList> &getExample() const {
        return m_example;
    }
    
    int getMaxExampleCols() const { return m_maxExampleCols; }
        
protected:
    ModelType    m_modelType;
    TimingType   m_timingType;
    TimeUnits    m_timeUnits;
    QString      m_separator; // "" or a single char - basically QChar option
    std::set<QChar> m_plausibleSeparators;
    sv_samplerate_t m_sampleRate;
    int          m_windowSize;

    int          m_columnCount;
    bool         m_variableColumnCount;

    QList<ColumnQualities> m_columnQualities;
    QList<ColumnPurpose> m_columnPurposes;

    AudioSampleRange m_audioSampleRange;

    QList<float> m_prevValues;

    bool m_allowQuoting;

    QList<QStringList> m_example;
    int m_maxExampleCols;

    void guessSeparator(QString line);
    void guessQualities(QString line, int lineno);
    void guessPurposes();
    void guessAudioSampleRange();
};

#endif
