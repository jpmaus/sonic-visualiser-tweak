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

#ifndef RESAMPLER_WRAPPER_H
#define RESAMPLER_WRAPPER_H

#include "ApplicationPlaybackSource.h"

namespace breakfastquay {

class Resampler;

/**
 * Utility class for applications that want automatic sample rate
 * conversion on playback (resampling on record is not provided).
 *
 * An ApplicationPlaybackSource may request a specific sample rate
 * through its getApplicationSampleRate callback. This will be used as
 * the default rate when opening the audio driver. However, not all
 * drivers can be opened at arbitrary rates (e.g. a JACK driver always
 * inherits the JACK graph sample rate), so it's possible that a
 * driver may be opened at a different rate from that requested by the
 * source.
 *
 * An application can accommodate this by wrapping its
 * ApplicationPlaybackSource in a ResamplerWrapper when passing it to
 * the AudioFactory. The ResamplerWrapper will automatically resample
 * output if the driver happened to be opened at a different rate from
 * that requested by the source.
 */
class ResamplerWrapper : public ApplicationPlaybackSource
{
public:
    /**
     * Create a wrapper around the given ApplicationPlaybackSource,
     * implementing another ApplicationPlaybackSource interface that
     * draws from the same source data but resampling to a playback
     * target's expected sample rate automatically.
     */
    ResamplerWrapper(ApplicationPlaybackSource *source);

    ~ResamplerWrapper();

    /**
     * Call this function (e.g. from the wrapped
     * ApplicationPlaybackSource) to indicate a change in the sample
     * rate that we should be resampling from.
     *
     * (The wrapped ApplicationPlaybackSource should not change the
     * value it returns from getApplicationSampleRate(), as the API
     * requires that that be fixed.)
     */
    void changeApplicationSampleRate(int newRate);

    /**
     * Clear resampler buffers.
     */
    void reset();
    
    // These functions are passed through to the wrapped
    // ApplicationPlaybackSource
    
    std::string getClientName() const override;
    int getApplicationSampleRate() const override;
    int getApplicationChannelCount() const override;

    void setSystemPlaybackBlockSize(int) override;
    void setSystemPlaybackSampleRate(int) override;
    void setSystemPlaybackChannelCount(int) override;
    void setSystemPlaybackLatency(int) override;

    void setOutputLevels(float peakLeft, float peakRight) override;
    void audioProcessingOverload() override;

    /** 
     * Request some samples from the wrapped
     * ApplicationPlaybackSource, resample them if necessary, and
     * return them to the target
     */
    int getSourceSamples(float *const *samples, int nchannels, int nframes) override;

private:
    ApplicationPlaybackSource *m_source;
    
    int m_channels;
    int m_targetRate;
    int m_sourceRate;

    Resampler *m_resampler;

    float **m_in;
    int m_inSize;
    float **m_resampled;
    int m_resampledSize;
    int m_resampledFill;
    float **m_ptrs;

    void setupBuffersFor(int reqsize);

    ResamplerWrapper(const ResamplerWrapper &)=delete;
    ResamplerWrapper &operator=(const ResamplerWrapper &)=delete;
};

}

#endif
