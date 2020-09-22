/* -*- c-basic-offset: 4 indent-tabs-mode: nil -*-  vi:set ts=8 sts=4 sw=4: */

/*
    Sonic Visualiser
    An audio file viewer and annotation editor.
    Centre for Digital Music, Queen Mary, University of London.
    This file copyright 2006 Chris Cannam and QMUL.
    
    This program is free software; you can redistribute it and/or
    modify it under the terms of the GNU General Public License as
    published by the Free Software Foundation; either version 2 of the
    License, or (at your option) any later version.  See the file
    COPYING included with this distribution for more information.
*/

#ifndef READ_ONLY_WAVE_FILE_MODEL_H
#define READ_ONLY_WAVE_FILE_MODEL_H

#include "WaveFileModel.h"

#include "base/Thread.h"
#include <QMutex>
#include <QTimer>

#include "data/fileio/FileSource.h"

#include "RangeSummarisableTimeValueModel.h"
#include "PowerOfSqrtTwoZoomConstraint.h"

#include <stdlib.h>

class AudioFileReader;

class ReadOnlyWaveFileModel : public WaveFileModel
{
    Q_OBJECT

public:
    /**
     * Construct a WaveFileModel from a source path and optional
     * resampling target rate
     */
    ReadOnlyWaveFileModel(FileSource source, sv_samplerate_t targetRate = 0);

    /**
     * Construct a WaveFileModel from a source path using an existing
     * AudioFileReader. The model does not take ownership of the
     * AudioFileReader, which remains managed by the caller and must
     * outlive the model.
     */
    ReadOnlyWaveFileModel(FileSource source, AudioFileReader *reader);
    
    ~ReadOnlyWaveFileModel();

    bool isOK() const override;
    bool isReady(int *) const override;
    int getCompletion() const override {
        int c = 0;
        (void)isReady(&c);
        return c;
    }

    const ZoomConstraint *getZoomConstraint() const override { return &m_zoomConstraint; }

    sv_frame_t getFrameCount() const override;
    int getChannelCount() const override;
    sv_samplerate_t getSampleRate() const override;
    sv_samplerate_t getNativeRate() const override;

    QString getTitle() const override;
    QString getMaker() const override;
    QString getLocation() const override;

    QString getLocalFilename() const;

    float getValueMinimum() const override { return -1.0f; }
    float getValueMaximum() const override { return  1.0f; }

    sv_frame_t getStartFrame() const override { return m_startFrame; }
    sv_frame_t getTrueEndFrame() const override { return m_startFrame + getFrameCount(); }

    void setStartFrame(sv_frame_t startFrame) override { m_startFrame = startFrame; }

    floatvec_t getData(int channel, sv_frame_t start, sv_frame_t count) const override;

    std::vector<floatvec_t> getMultiChannelData(int fromchannel, int tochannel, sv_frame_t start, sv_frame_t count) const override;

    int getSummaryBlockSize(int desired) const override;

    void getSummaries(int channel, sv_frame_t start, sv_frame_t count,
                              RangeBlock &ranges,
                              int &blockSize) const override;

    Range getSummary(int channel, sv_frame_t start, sv_frame_t count) const override;

    QString getTypeName() const override { return tr("Wave File"); }

    void toXml(QTextStream &out,
                       QString indent = "",
                       QString extraAttributes = "") const override;

protected slots:
    void fillTimerTimedOut();
    void cacheFilled();
    
protected:
    void initialize();

    class RangeCacheFillThread : public Thread
    {
    public:
        RangeCacheFillThread(ReadOnlyWaveFileModel &model) :
            m_model(model), m_fillExtent(0),
            m_frameCount(model.getFrameCount()) { }
    
        sv_frame_t getFillExtent() const { return m_fillExtent; }
        void run() override;

    protected:
        ReadOnlyWaveFileModel &m_model;
        sv_frame_t m_fillExtent;
        sv_frame_t m_frameCount;
    };
         
    void fillCache();

    FileSource m_source;
    QString m_path;
    AudioFileReader *m_reader;
    bool m_myReader;

    sv_frame_t m_startFrame;

    RangeBlock m_cache[2]; // interleaved at two base resolutions
    mutable QMutex m_mutex;
    RangeCacheFillThread *m_fillThread;
    QTimer *m_updateTimer;
    sv_frame_t m_lastFillExtent;
    mutable int m_prevCompletion;
    bool m_exiting;
    static PowerOfSqrtTwoZoomConstraint m_zoomConstraint;

    mutable floatvec_t m_directRead;
    mutable sv_frame_t m_lastDirectReadStart;
    mutable sv_frame_t m_lastDirectReadCount;
    mutable QMutex m_directReadMutex;
};    

#endif
