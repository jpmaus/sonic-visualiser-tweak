/* -*- c-basic-offset: 4 indent-tabs-mode: nil -*-  vi:set ts=8 sts=4 sw=4: */
/*
    bqaudioio

    Copyright 2007-2016 Particular Programs Ltd.

    Permission is hereby granted, free of charge, to any person
    obtaining a copy of this software and associated documentation
    files (the "Software"), to deal in the Software without
    restriction, including without limitation the rights to use, copy,
    modify, merge, publish, distribute, sublicense, and/or sell copies
    of the Software, and to permit persons to whom the Software is
    furnished to do so, subject to the following conditions:

    The above copyright notice and this permission notice shall be
    included in all copies or substantial portions of the Software.

    THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND,
    EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF
    MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND
    NONINFRINGEMENT. IN NO EVENT SHALL THE AUTHORS BE LIABLE FOR
    ANY CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER IN AN ACTION OF
    CONTRACT, TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN CONNECTION
    WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.

    Except as contained in this notice, the names of Chris Cannam and
    Particular Programs Ltd shall not be used in advertising or
    otherwise to promote the sale, use or other dealings in this
    Software without prior written authorization.
*/

#ifndef BQAUDIOIO_APPLICATION_PLAYBACK_SOURCE_H
#define BQAUDIOIO_APPLICATION_PLAYBACK_SOURCE_H

#include <string>

namespace breakfastquay {

class SystemPlaybackTarget;

/**
 * Interface for a source of playback samples from the application. To
 * be implemented in the application and passed to
 * AudioFactory::createCallbackPlayTarget or createAudioIO.
 */
class ApplicationPlaybackSource
{
public:
    virtual ~ApplicationPlaybackSource() { }

    /**
     * Return an identifier for the application client. May be used in
     * connection strings or (possibly) error and logging information.
     */
    virtual std::string getClientName() const = 0;

    /**
     * Return the sample rate at which the application runs. The
     * target or IO will attempt to open its device at the rate
     * returned by this call at the point where the device is opened,
     * although it might not succeed; it will provide the actual rate
     * through a subsequent call to setSystemPlaybackSampleRate.
     *
     * Return 0 if the application has no central sample rate of its
     * own and is happy to accept the default rate of the device.
     * 
     * This should not change during the lifetime of the target or
     * IO. If you want to handle a changing source sample rate, use a
     * ResamplerWrapper.
     */
    virtual int getApplicationSampleRate() const = 0;

    /**
     * Return the number of audio channels that will be delivered by
     * the application. The target or IO will attempt to open its
     * device with this number of channels, though it might not
     * succeed; it will provide the actual number of channels through
     * a subsequent call to setSystemPlaybackChannelCount and will
     * mixdown as appropriate.
     *
     * This must not be zero and is not expected to change during the
     * lifetime of the target or IO.
     */
    virtual int getApplicationChannelCount() const = 0;

    /**
     * Called by the system target/IO if processing will be using a
     * fixed block size, to tell the application what that block size
     * will be (in sample frames). If this is not called, the
     * application must assume that any number of samples could be
     * requested at a time.
     */
    virtual void setSystemPlaybackBlockSize(int) = 0;

    /**
     * Called by the system target/IO to tell the application the
     * sample rate at which the audio device was opened.
     */
    virtual void setSystemPlaybackSampleRate(int) = 0;

    /**
     * Called by the system target/IO to tell the application the
     * actual number of channels with which the audio device was
     * opened. Note that the target/IO handles channel mapping and
     * mixdown; this is just informative.
     */
    virtual void setSystemPlaybackChannelCount(int) = 0;
    
    /**
     * Called by the system target/IO to tell the application the
     * system playback latency in sample frames at the playback sample
     * rate.
     */
    virtual void setSystemPlaybackLatency(int) = 0;

    /**
     * Request a number of audio sample frames from the
     * application. The samples pointer will point to nchannels
     * channel buffers, each having enough space for nframes
     * samples. This function should write the requested number of
     * samples directly into those buffers.  The value of nchannels is
     * guaranteed to be the same as getApplicationChannelCount()
     * returned at the time the device was initialised.
     *
     * Return value should be the number of sample frames written
     * (nframes unless fewer samples exist to be played).
     *
     * This may be called from a realtime context.
     */
    virtual int getSourceSamples(float *const *samples, int nchannels, int nframes) = 0;

    /**
     * Report peak output levels for the last output
     * buffer. Potentially useful for monitoring.
     *
     * This may be called from a realtime context.
     */
    virtual void setOutputLevels(float peakLeft, float peakRight) = 0;

    /**
     * Called when an audio dropout is reported due to a processing
     * overload.
     */
    virtual void audioProcessingOverload() { }
};

}

#endif
    
