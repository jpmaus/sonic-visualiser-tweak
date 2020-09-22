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

#ifndef SV_AUDIO_CALLBACK_RECORD_TARGET_H
#define SV_AUDIO_CALLBACK_RECORD_TARGET_H

#include "base/AudioRecordTarget.h"

#include <bqaudioio/ApplicationRecordTarget.h>

#include <string>
#include <atomic>

#include <QObject>
#include <QMutex>

#include "base/BaseTypes.h"
#include "base/RingBuffer.h"

class ViewManagerBase;
class WritableWaveFileModel;

class AudioCallbackRecordTarget : public QObject,
                                  public AudioRecordTarget,
                                  public breakfastquay::ApplicationRecordTarget
{
    Q_OBJECT

public:
    AudioCallbackRecordTarget(ViewManagerBase *, QString clientName);
    virtual ~AudioCallbackRecordTarget();

    virtual std::string getClientName() const override { return m_clientName; }
    
    virtual int getApplicationSampleRate() const override;
    virtual int getApplicationChannelCount() const override;

    virtual void setSystemRecordBlockSize(int) override;
    virtual void setSystemRecordSampleRate(int) override;
    virtual void setSystemRecordLatency(int) override;
    virtual void setSystemRecordChannelCount(int) override;

    virtual void putSamples(const float *const *samples, int nchannels, int nframes) override;
    
    virtual void setInputLevels(float peakLeft, float peakRight) override;

    virtual void audioProcessingOverload() override { }
    
    virtual bool isRecording() const override { return m_recording; }
    virtual sv_frame_t getRecordDuration() const override { return m_frameCount; }

    /**
     * Return the current input levels in the range 0.0 -> 1.0, for
     * metering purposes. The values returned are the peak values
     * since the last time this function was called (after which they
     * are reset to zero until setInputLevels is called again by the
     * driver).
     *
     * Return true if the values have been set since this function was
     * last called (i.e. if they are meaningful). Return false if they
     * have not been set (in which case both will be zero).
     */
     virtual bool getInputLevels(float &left, float &right) override;

    WritableWaveFileModel *startRecording(); // caller takes ownership of model
    void stopRecording();

signals:
    void recordStatusChanged(bool recording);
    void recordDurationChanged(sv_frame_t, sv_samplerate_t); // emitted occasionally
    void recordCompleted();

protected slots:
    void modelAboutToBeDeleted();
    void updateModel();
    
private:
    ViewManagerBase *m_viewManager;
    std::string m_clientName;
    std::atomic_bool m_recording;
    sv_samplerate_t m_recordSampleRate;
    int m_recordChannelCount;
    sv_frame_t m_frameCount;
    QString m_audioFileName;
    WritableWaveFileModel *m_model;
    RingBuffer<float> **m_buffers;
    QMutex m_bufPtrMutex;
    int m_bufferCount;
    float m_inputLeft;
    float m_inputRight;
    bool m_levelsSet;

    void recreateBuffers();
};

#endif
