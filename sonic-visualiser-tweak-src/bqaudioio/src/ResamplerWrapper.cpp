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

#include "ResamplerWrapper.h"

#include "bqresample/Resampler.h"
#include "bqvec/Allocators.h"
#include "bqvec/VectorOps.h"

#include "ApplicationPlaybackSource.h"
#include "Log.h"

#include <iostream>
#include <sstream>

//#define DEBUG_RESAMPLER_WRAPPER 1

using namespace std;

namespace breakfastquay {

static int defaultMaxBufferSize = 10240; // bigger will require dynamic resizing

ResamplerWrapper::ResamplerWrapper(ApplicationPlaybackSource *source) :
    m_source(source),
    m_targetRate(44100), // will update when the target calls back
    m_sourceRate(0),
    m_resampler(0),
    m_in(0),
    m_inSize(0),
    m_resampled(0),
    m_resampledSize(0),
    m_resampledFill(0),
    m_ptrs(0)
{
    m_sourceRate = m_source->getApplicationSampleRate();

    // Note, m_sourceRate might be zero if the application is happy to
    // allow the device to be opened at any rate. We can't actually
    // work with a zero source rate, but the application may change it
    // through a call to changeApplicationSampleRate() before playback
    // begins, so we have to allow this at this point.
    
    m_channels = m_source->getApplicationChannelCount();

    Resampler::Parameters params;
    params.quality = Resampler::FastestTolerable;
    params.maxBufferSize = defaultMaxBufferSize;
    if (m_sourceRate != 0) {
        params.initialSampleRate = m_sourceRate;
    }
    m_resampler = new Resampler(params, m_channels);
    
    m_ptrs = new float *[m_channels];
    setupBuffersFor(defaultMaxBufferSize);
}

ResamplerWrapper::~ResamplerWrapper()
{
    delete m_resampler;
    delete[] m_ptrs;
    if (m_in) {
	deallocate_channels(m_in, m_channels);
	deallocate_channels(m_resampled, m_channels);
    }
}

void
ResamplerWrapper::changeApplicationSampleRate(int newRate)
{
    m_sourceRate = newRate;
    setupBuffersFor(defaultMaxBufferSize);
}

std::string
ResamplerWrapper::getClientName() const
{
    return m_source->getClientName();
}

int
ResamplerWrapper::getApplicationSampleRate() const
{
    // Although we could return 0 here (as we can accept any rate from
    // the target), things are simplest if the target can offer the
    // rate that we actually do want. But this isn't supposed to
    // change, so call this source function rather than returning our
    // m_sourceRate (which is changeable)
    return m_source->getApplicationSampleRate();
}

int
ResamplerWrapper::getApplicationChannelCount() const
{
    return m_source->getApplicationChannelCount();
}

void
ResamplerWrapper::setSystemPlaybackBlockSize(int)
{
}

void
ResamplerWrapper::setSystemPlaybackSampleRate(int rate)
{
    m_targetRate = rate;
    m_source->setSystemPlaybackSampleRate(m_targetRate);
}

void
ResamplerWrapper::setSystemPlaybackChannelCount(int c)
{
    m_source->setSystemPlaybackChannelCount(c);
}

void
ResamplerWrapper::setSystemPlaybackLatency(int latency)
{
    m_source->setSystemPlaybackLatency(latency);
}

void
ResamplerWrapper::setOutputLevels(float left, float right)
{
    m_source->setOutputLevels(left, right);
}

void
ResamplerWrapper::audioProcessingOverload()
{
    m_source->audioProcessingOverload();
}

void
ResamplerWrapper::reset()
{
    if (m_resampler) m_resampler->reset();
    m_resampledFill = 0;
}

int
ResamplerWrapper::getSourceSamples(float *const *samples, int nchannels, int nframes)
{
#ifdef DEBUG_RESAMPLER_WRAPPER
    cerr << "ResamplerWrapper::getSourceSamples(" << nframes << "): source rate = " << m_sourceRate << ", target rate = " << m_targetRate << ", channels = " << m_channels << endl;
#endif
    
    setupBuffersFor(nframes);

    if (m_sourceRate == 0) {
        v_zero_channels(samples, nchannels, nframes);
        return nframes;
    }
    
    if (nchannels != m_channels) {
        cerr << "nchannels = " << nchannels << ", m_channels = " << m_channels << endl;
        throw std::logic_error("Different number of channels requested than ResamplerWrapper declared");
    }
    
    if (m_sourceRate == m_targetRate) {
	return m_source->getSourceSamples(samples, nchannels, nframes);
    }

    double ratio = double(m_targetRate) / double(m_sourceRate);

    int reqResampled = nframes - m_resampledFill + 1;
    int req = int(round(reqResampled / ratio)) + 1;

    int received = m_source->getSourceSamples(m_in, m_channels, req);
    
    for (int i = 0; i < m_channels; ++i) {
        m_ptrs[i] = m_resampled[i] + m_resampledFill;
    }

#ifdef DEBUG_RESAMPLER_WRAPPER
    cerr << "ResamplerWrapper: nframes = " << nframes << ", ratio = " << ratio << endl;
    cerr << "ResamplerWrapper: m_inSize = " << m_inSize << ", m_resampledSize = "
         << m_resampledSize << ", m_resampledFill = " << m_resampledFill << endl;
    cerr << "ResamplerWrapper: reqResampled = " << reqResampled << ", req = "
         << req << ", received = " << received << endl;
#endif

    if (received > 0) {

        try {
            int resampled = m_resampler->resample
                (m_ptrs, m_resampledSize - m_resampledFill,
                 m_in, received,
                 ratio);

            m_resampledFill += resampled;
        
#ifdef DEBUG_RESAMPLER_WRAPPER
            cerr << "ResamplerWrapper: resampled = " << resampled << ", m_resampledFill now = " << m_resampledFill << endl;
#endif

        } catch (const breakfastquay::Resampler::Exception &e) {
            static bool errorShown = false;
            if (!errorShown) {
                ostringstream os;
                os << "ResamplerWrapper: Failed to resample " << received
                   << " sample(s) at a ratio of " << ratio
                   << " (NB this error will not be printed again, even if "
                   << "the problem persists)";
                Log::log(os.str());
                cerr << "ERROR: " << os.str() << endl;
                errorShown = true;
            }
        }
    }
            
    if (m_resampledFill < nframes) {
	for (int i = 0; i < m_channels; ++i) {
	    v_zero(m_resampled[i] + m_resampledFill, nframes - m_resampledFill);
	}
        m_resampledFill = nframes;
    }

    v_copy_channels(samples, m_resampled, m_channels, nframes);

    if (m_resampledFill > nframes) {
        for (int i = 0; i < m_channels; ++i) {
            m_ptrs[i] = m_resampled[i] + nframes;
        }
        v_move_channels(m_resampled, m_ptrs, m_channels, m_resampledFill - nframes);
    }

    m_resampledFill -= nframes;

#ifdef DEBUG_RESAMPLER_WRAPPER
    cerr << "ResamplerWrapper: m_resampledFill now = " << m_resampledFill << " and returning nframes = " << nframes << endl;
#endif

    return nframes;
}

void
ResamplerWrapper::setupBuffersFor(int nframes)
{
    if (m_sourceRate == 0) return;
    if (m_sourceRate == m_targetRate) return;

#ifdef DEBUG_RESAMPLER_WRAPPER
    cerr << "ResamplerWrapper::setupBuffersFor: Source rate "
         << m_sourceRate << " -> target rate " << m_targetRate << endl;
#endif

    int slack = 100;
    double ratio = double(m_targetRate) / double(m_sourceRate);
    if (ratio > 50.0) {
        slack = int(ratio * 2);
    }
    int newResampledSize = nframes + slack;
    int newInSize = int(newResampledSize / ratio);

#ifdef DEBUG_RESAMPLER_WRAPPER
    cerr << "newResampledSize = " << newResampledSize << ", newInSize = " << newInSize << endl;
#endif
    
    if (!m_resampled || newResampledSize > m_resampledSize) {
        m_resampled = reallocate_and_zero_extend_channels
            (m_resampled,
             m_channels, m_resampledSize,
             m_channels, newResampledSize);
        m_resampledSize = newResampledSize;
    }

    if (!m_in || newInSize > m_inSize) {
	m_in = reallocate_and_zero_extend_channels
            (m_in,
             m_channels, m_inSize,
             m_channels, newInSize);
	m_inSize = newInSize;
    }
}

}

