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

#ifndef SV_AUDIO_CALLBACK_PLAY_SOURCE_H
#define SV_AUDIO_CALLBACK_PLAY_SOURCE_H

#include "base/RingBuffer.h"
#include "base/AudioPlaySource.h"
#include "base/PropertyContainer.h"
#include "base/Scavenger.h"

#include <bqaudioio/ApplicationPlaybackSource.h>

#include <QObject>
#include <QMutex>
#include <QWaitCondition>

#include "base/Thread.h"
#include "base/RealTime.h"
#include "data/model/Model.h"

#include <samplerate.h>

#include <set>
#include <map>

namespace RubberBand {
    class RubberBandStretcher;
}

namespace breakfastquay {
    class ResamplerWrapper;
}

class Model;
class ViewManagerBase;
class AudioGenerator;
class PlayParameters;
class RealTimePluginInstance;
class AudioCallbackPlayTarget;

/**
 * AudioCallbackPlaySource manages audio data supply to callback-based
 * audio APIs such as JACK or CoreAudio.  It maintains one ring buffer
 * per channel, filled during playback by a non-realtime thread, and
 * provides a method for a realtime thread to pick up the latest
 * available sample data from these buffers.
 */
class AudioCallbackPlaySource : public QObject,
                                public AudioPlaySource,
                                public breakfastquay::ApplicationPlaybackSource
{
    Q_OBJECT

public:
    AudioCallbackPlaySource(ViewManagerBase *, QString clientName);
    virtual ~AudioCallbackPlaySource();
    
    /**
     * Add a data model to be played from.  The source can mix
     * playback from a number of sources including dense and sparse
     * models.  The models must match in sample rate, but they don't
     * have to have identical numbers of channels.
     */
    virtual void addModel(ModelId model);

    /**
     * Remove a model.
     */
    virtual void removeModel(ModelId model);

    /**
     * Remove all models.  (Silence will ensue.)
     */
    virtual void clearModels();

    /**
     * Start making data available in the ring buffers for playback,
     * from the given frame.  If playback is already under way, reseek
     * to the given frame and continue.
     */
    virtual void play(sv_frame_t startFrame) override;

    /**
     * Stop playback and ensure that no more data is returned.
     */
    virtual void stop() override;

    /**
     * Return whether playback is currently supposed to be happening.
     */
    virtual bool isPlaying() const override { return m_playing; }

    /**
     * Return the frame number that is currently expected to be coming
     * out of the speakers.  (i.e. compensating for playback latency.)
     */
    virtual sv_frame_t getCurrentPlayingFrame() override;
    
    /** 
     * Return the last frame that would come out of the speakers if we
     * stopped playback right now.
     */
    virtual sv_frame_t getCurrentBufferedFrame();

    /**
     * Return the frame at which playback is expected to end (if not looping).
     */
    virtual sv_frame_t getPlayEndFrame() { return m_lastModelEndFrame; }

    /**
     * Set the playback target.
     */
    virtual void setSystemPlaybackTarget(breakfastquay::SystemPlaybackTarget *);

    /**
     * Set the resampler wrapper, if one is in use.
     */
    virtual void setResamplerWrapper(breakfastquay::ResamplerWrapper *);
    
    /**
     * Set the block size of the target audio device.  This should be
     * called by the target class.
     */
    virtual void setSystemPlaybackBlockSize(int blockSize) override;

    /**
     * Get the block size of the target audio device.  This may be an
     * estimate or upper bound, if the target has a variable block
     * size; the source should behave itself even if this value turns
     * out to be inaccurate.
     */
    virtual int getTargetBlockSize() const override;

    /**
     * Set the playback latency of the target audio device, in frames
     * at the device sample rate.  This is the difference between the
     * frame currently "leaving the speakers" and the last frame (or
     * highest last frame across all channels) requested via
     * getSamples().  The default is zero.
     */
    virtual void setSystemPlaybackLatency(int) override;

    /**
     * Get the playback latency of the target audio device.
     */
    sv_frame_t getTargetPlayLatency() const;

    /**
     * Specify that the target audio device has a fixed sample rate
     * (i.e. cannot accommodate arbitrary sample rates based on the
     * source).  If the target sets this to something other than the
     * source sample rate, this class will resample automatically to
     * fit.
     */
    virtual void setSystemPlaybackSampleRate(int) override;

    /**
     * Return the sample rate set by the target audio device (or the
     * source sample rate if the target hasn't set one).
     */
    virtual sv_samplerate_t getDeviceSampleRate() const override;

    /**
     * Indicate how many channels the target audio device was opened
     * with. Note that the target device does channel mixing in the
     * case where our requested channel count does not match its, so
     * long as we provide the number of channels we specified when the
     * target was started in getApplicationChannelCount().
     */
    virtual void setSystemPlaybackChannelCount(int) override;
    
    /**
     * Set the current output levels for metering (for call from the
     * target)
     */
    virtual void setOutputLevels(float left, float right) override;

    /**
     * Return the current output levels in the range 0.0 -> 1.0, for
     * metering purposes. The values returned are the peak values
     * since the last time this function was called (after which they
     * are reset to zero until setOutputLevels is called again by the
     * driver).
     *
     * Return true if the values have been set since this function was
     * last called (i.e. if they are meaningful). Return false if they
     * have not been set (in which case both will be zero).
     */
    virtual bool getOutputLevels(float &left, float &right) override;

    /**
     * Get the number of channels of audio that in the source models.
     * This may safely be called from a realtime thread.  Returns 0 if
     * there is no source yet available.
     */
    int getSourceChannelCount() const;

    /**
     * Get the number of channels of audio that will be provided
     * to the play target.  This may be more than the source channel
     * count: for example, a mono source will provide 2 channels
     * after pan.
     *
     * This may safely be called from a realtime thread.  Returns 0 if
     * there is no source yet available.
     *
     * override from AudioPlaySource
     */
    virtual int getTargetChannelCount() const override;

    /**
     * Get the number of channels of audio the device is
     * expecting. Equal to whatever getTargetChannelCount() was
     * returning at the time the device was initialised.
     */
    int getDeviceChannelCount() const;
    
    /**
     * ApplicationPlaybackSource equivalent of the above.
     *
     * override from breakfastquay::ApplicationPlaybackSource
     */
    virtual int getApplicationChannelCount() const override {
        return getTargetChannelCount();
    }
    
    /**
     * Get the actual sample rate of the source material (the main
     * model).  This may safely be called from a realtime thread.
     * Returns 0 if there is no source yet available.
     *
     * When this changes, the AudioCallbackPlaySource notifies its
     * ResamplerWrapper of the new sample rate so that it can resample
     * correctly on the way to the device (which is opened at a fixed
     * rate, see getApplicationSampleRate).
     */
    virtual sv_samplerate_t getSourceSampleRate() const override;

    /**
     * ApplicationPlaybackSource interface method: get the sample rate
     * at which the application wants the device to be opened. We
     * always allow the device to open at its default rate, and then
     * we resample if the audio is at a different rate. This avoids
     * having to close and re-open the device to obtain consistent
     * behaviour for consecutive sessions with different source rates.
     */
    virtual int getApplicationSampleRate() const override {
        return 0;
    }

    /**
     * Get "count" samples (at the target sample rate) of the mixed
     * audio data, in all channels.  This may safely be called from a
     * realtime thread.
     */
    virtual int getSourceSamples(float *const *buffer, int nchannels, int count) override;

    /**
     * Set the time stretcher factor (i.e. playback speed).
     */
    void setTimeStretch(double factor);

    /**
     * Set a single real-time plugin as a processing effect for
     * auditioning during playback.
     *
     * The plugin must have been initialised with
     * getTargetChannelCount() channels and a getTargetBlockSize()
     * sample frame processing block size.
     *
     * This playback source takes ownership of the plugin, which will
     * be deleted at some point after the following call to
     * setAuditioningEffect (depending on real-time constraints).
     *
     * Pass a null pointer to remove the current auditioning plugin,
     * if any.
     */
    virtual void setAuditioningEffect(Auditionable *plugin) override;

    /**
     * Specify that only the given set of models should be played.
     */
    void setSoloModelSet(std::set<ModelId>s);

    /**
     * Specify that all models should be played as normal (if not
     * muted).
     */
    void clearSoloModelSet();

    virtual std::string getClientName() const override {
        return m_clientName;
    }

signals:
    void playStatusChanged(bool isPlaying);

    void sampleRateMismatch(sv_samplerate_t requested,
                            sv_samplerate_t available,
                            bool willResample);

    void channelCountIncreased(int count); // target channel count (see getTargetChannelCount())

    void audioOverloadPluginDisabled();
    void audioTimeStretchMultiChannelDisabled();

    void activity(QString);

public slots:
    void audioProcessingOverload() override;

protected slots:
    void selectionChanged();
    void playLoopModeChanged();
    void playSelectionModeChanged();
    void playParametersChanged(int);
    void preferenceChanged(PropertyContainer::PropertyName);
    void modelChangedWithin(ModelId, sv_frame_t startFrame, sv_frame_t endFrame);

protected:
    ViewManagerBase                  *m_viewManager;
    AudioGenerator                   *m_audioGenerator;
    std::string                       m_clientName;

    class RingBufferVector : public std::vector<RingBuffer<float> *> {
    public:
        virtual ~RingBufferVector() {
            while (!empty()) {
                delete *begin();
                erase(begin());
            }
        }
    };

    std::set<ModelId>                 m_models;
    RingBufferVector                 *m_readBuffers;
    RingBufferVector                 *m_writeBuffers;
    sv_frame_t                        m_readBufferFill;
    sv_frame_t                        m_writeBufferFill;
    Scavenger<RingBufferVector>       m_bufferScavenger;
    int                               m_sourceChannelCount;
    sv_frame_t                        m_blockSize;
    sv_samplerate_t                   m_sourceSampleRate;
    sv_samplerate_t                   m_deviceSampleRate;
    int                               m_deviceChannelCount;
    sv_frame_t                        m_playLatency;
    breakfastquay::SystemPlaybackTarget *m_target;
    double                            m_lastRetrievalTimestamp;
    sv_frame_t                        m_lastRetrievedBlockSize;
    bool                              m_trustworthyTimestamps;
    sv_frame_t                        m_lastCurrentFrame;
    bool                              m_playing;
    bool                              m_exiting;
    sv_frame_t                        m_lastModelEndFrame;
    int                               m_ringBufferSize;
    float                             m_outputLeft;
    float                             m_outputRight;
    bool                              m_levelsSet;
    RealTimePluginInstance           *m_auditioningPlugin;
    bool                              m_auditioningPluginBypassed;
    Scavenger<RealTimePluginInstance> m_pluginScavenger;
    sv_frame_t                        m_playStartFrame;
    bool                              m_playStartFramePassed;
    RealTime                          m_playStartedAt;

    RingBuffer<float> *getWriteRingBuffer(int c) {
        if (m_writeBuffers && c < (int)m_writeBuffers->size()) {
            return (*m_writeBuffers)[c];
        } else {
            return 0;
        }
    }

    RingBuffer<float> *getReadRingBuffer(int c) {
        RingBufferVector *rb = m_readBuffers;
        if (rb && c < (int)rb->size()) {
            return (*rb)[c];
        } else {
            return 0;
        }
    }

    void clearRingBuffers(bool haveLock = false, int count = 0);
    void unifyRingBuffers();

    RubberBand::RubberBandStretcher *m_timeStretcher;
    RubberBand::RubberBandStretcher *m_monoStretcher;
    double m_stretchRatio;
    bool m_stretchMono;
    
    int m_stretcherInputCount;
    float **m_stretcherInputs;
    sv_frame_t *m_stretcherInputSizes;

    // Called from fill thread, m_playing true, mutex held
    // Return true if work done
    bool fillBuffers();
    
    // Called from fillBuffers.  Return the number of frames written,
    // which will be count or fewer.  Return in the frame argument the
    // new buffered frame position (which may be earlier than the
    // frame argument passed in, in the case of looping).
    sv_frame_t mixModels(sv_frame_t &frame, sv_frame_t count, float **buffers);

    // Called from getSourceSamples.
    void applyAuditioningEffect(sv_frame_t count, float *const *buffers);

    // Ranges of current selections, if play selection is active
    std::vector<RealTime> m_rangeStarts;
    std::vector<RealTime> m_rangeDurations;
    void rebuildRangeLists();

    sv_frame_t getCurrentFrame(RealTime outputLatency);

    class FillThread : public Thread
    {
    public:
        FillThread(AudioCallbackPlaySource &source) :
            Thread(Thread::NonRTThread),
            m_source(source) { }

        void run() override;

    protected:
        AudioCallbackPlaySource &m_source;
    };

    QMutex m_mutex;
    QWaitCondition m_condition;
    FillThread *m_fillThread;
    breakfastquay::ResamplerWrapper *m_resamplerWrapper; // I don't own this
};

#endif


