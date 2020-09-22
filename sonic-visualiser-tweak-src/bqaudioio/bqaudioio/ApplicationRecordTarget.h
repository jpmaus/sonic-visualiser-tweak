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

#ifndef BQAUDIOIO_APPLICATION_RECORD_TARGET_H
#define BQAUDIOIO_APPLICATION_RECORD_TARGET_H

#include <string>

namespace breakfastquay {

class SystemRecordSource;

/**
 * Interface for an application sink that accepts recorded samples. To
 * be implemented in the application and passed to
 * AudioFactory::createCallbackRecordSource or createAudioIO.
 */
class ApplicationRecordTarget
{
public:
    virtual ~ApplicationRecordTarget() { }

    /**
     * Return an identifier for the application client. May be used in
     * connection strings or (possibly) error and logging information.
     */
    virtual std::string getClientName() const = 0;
    
    /**
     * Return the sample rate at which the application runs. The
     * source or IO will attempt to open its device at the rate
     * returned by this call at the point where the device is opened,
     * although it might not succeed; it will provide the actual rate
     * through a subsequent call to setSystemPlaybackSampleRate.
     *
     * Return 0 if the application has no central sample rate of its
     * own and is happy to accept the default rate of the device.
     */
    virtual int getApplicationSampleRate() const { return 0; }

    /**
     * Return the number of audio channels expected by the
     * application. The source or IO will attempt to open its device
     * with this number of channels, though it might not succeed; it
     * will provide the actual number of channels through a subsequent
     * call to setSystemPlaybackChannelCount and will mixdown as
     * appropriate.
     *
     * This must not be zero and is not expected to change during the
     * lifetime of the source or IO.
    */
    virtual int getApplicationChannelCount() const = 0;

    /**
     * Called by the system source/IO if processing will be using a
     * fixed block size, to tell the application what that block size
     * will be (in sample frames). If this is not called, the
     * application must assume that any number of samples could be
     * provided at a time.
     */
    virtual void setSystemRecordBlockSize(int) = 0;
    
    /**
     * Called by the system source/IO to tell the application the
     * sample rate at which the audio device was opened.
     */
    virtual void setSystemRecordSampleRate(int) = 0;
    
    /**
     * Called by the system source/IO to tell the application the
     * actual number of channels with which the audio device was
     * opened. Note that the source/IO handles channel mapping and
     * mixdown; this is just informative.
     */
    virtual void setSystemRecordChannelCount(int) = 0;
    
    /**
     * Called by the system source/IO to tell the application the
     * system record latency in sample frames.
     */
    virtual void setSystemRecordLatency(int) = 0;

    /**
     * Accept a number of audio sample frames that have been received
     * from the record device. The samples pointer will point to
     * nchannels channel buffers each having nframes samples. The
     * value of nchannels will be whatever
     * getApplicationChannelCount() returned at the time the device
     * was initialised.
     *
     * This may be called from realtime context.
     */
    virtual void putSamples(const float *const *samples, int nchannels, int nframes) = 0;
    
    /**
     * Report peak input levels for the last output
     * buffer. Potentially useful for monitoring.
     *
     * This may be called from realtime context.
     */
    virtual void setInputLevels(float peakLeft, float peakRight) = 0;

    /**
     * Called when an audio dropout is reported due to a processing
     * overload.
     */
    virtual void audioProcessingOverload() { }
};

}
#endif
    
