/* -*- c-basic-offset: 4 indent-tabs-mode: nil -*-  vi:set ts=8 sts=4 sw=4: */
/*
    bqaudiostream

    A small library wrapping various audio file read/write
    implementations in C++.

    Copyright 2007-2015 Particular Programs Ltd.

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

#include "AudioReadStream.h"

#include "bqresample/Resampler.h"

#include <cmath>

using namespace std;

namespace breakfastquay
{
	
AudioReadStream::AudioReadStream() :
    m_channelCount(0),
    m_sampleRate(0),
    m_retrievalRate(0),
    m_totalFileFrames(0),
    m_totalRetrievedFrames(0),
    m_resampler(0),
    m_resampleBuffer(0)
{
}

AudioReadStream::~AudioReadStream()
{
    delete m_resampler;
    delete m_resampleBuffer;
}

void
AudioReadStream::setRetrievalSampleRate(size_t rate)
{
    static size_t max = 1536000;
    if (rate > max) {
        cerr << "WARNING: unsupported sample rate " << rate
             << ", clamping to " << max << endl;
        rate = max;
    }
    m_retrievalRate = rate;
}

size_t
AudioReadStream::getRetrievalSampleRate() const
{
    if (m_retrievalRate == 0) return m_sampleRate;
    else return m_retrievalRate;
}

size_t
AudioReadStream::getInterleavedFrames(size_t count, float *frames)
{
    if (m_retrievalRate == 0 ||
        m_retrievalRate == m_sampleRate ||
        m_channelCount == 0) {
        return getFrames(count, frames);
    }
    
    // The resampler API works in ints - so we may have to do this in
    // chunks if count is very large. But that's not an unreasonable
    // way to do it anyway. 
    static size_t chunkSizeSamples = 1000000;

    size_t chunkFrames = chunkSizeSamples / m_channelCount;
    size_t frameOffset = 0;

    while (frameOffset < count) {

        size_t n = count - frameOffset;
        if (n > chunkFrames) n = chunkFrames;
        
        int framesObtained = getResampledChunk
            (int(n), frames + m_channelCount * frameOffset);
        
        if (framesObtained <= 0) {
            return frameOffset;
        }
        
        frameOffset += size_t(framesObtained);
        
        if (size_t(framesObtained) < n) {
            return frameOffset;
        }
    }

    return count;
}

int
AudioReadStream::getResampledChunk(int frameCount, float *frames)
{
    int channels = int(m_channelCount);

    if (!m_resampler) {
        Resampler::Parameters params;
        params.quality = Resampler::FastestTolerable;
        params.initialSampleRate = int(m_sampleRate);
        m_resampler = new Resampler(params, channels);
        m_resampleBuffer = new RingBuffer<float>(frameCount * channels);
    }

    double ratio = double(m_retrievalRate) / double(m_sampleRate);
    int fileFrames = int(ceil(frameCount / ratio));
    
    float *in  = allocate<float>(fileFrames * channels);
    float *out = allocate<float>((frameCount + 1) * channels);

    int samples = frameCount * channels;
    bool finished = false;
    
    while (m_resampleBuffer->getReadSpace() < samples) {

        if (finished) {
            int zeros = samples - m_resampleBuffer->getReadSpace();
            if (m_resampleBuffer->getWriteSpace() < zeros) {
                m_resampleBuffer = m_resampleBuffer->resized
                    (m_resampleBuffer->getSize() + samples);
            }
            m_resampleBuffer->zero(zeros);
            continue;
        }
        
        int fileFramesToGet =
            int(ceil((samples - m_resampleBuffer->getReadSpace())
                     / (channels * ratio)));

        int got = 0;

        if (!finished) {
            got = int(getFrames(fileFramesToGet, in));
            m_totalFileFrames += got;
            if (got < fileFramesToGet) {
                finished = true;
            }
        } else {
            v_zero(in, fileFramesToGet * channels);
            got = fileFramesToGet;
        }
        
        if (got > 0) {
            int resampled = m_resampler->resampleInterleaved
                (out, frameCount + 1, in, got, ratio, finished);
            if (m_resampleBuffer->getWriteSpace() < resampled * channels) {
                int resizeTo = (m_resampleBuffer->getSize() +
                                resampled * channels);
                m_resampleBuffer = m_resampleBuffer->resized(resizeTo);
            }
            m_resampleBuffer->write(out, resampled * channels);
        }
    }

    deallocate(in);
    deallocate(out);

    int toReturn = samples;
    int available = int(double(m_totalFileFrames) * ratio -
                        double(m_totalRetrievedFrames)) * channels;
    if (toReturn > available) toReturn = available;
    int actual = m_resampleBuffer->read(frames, toReturn) / channels;
    m_totalRetrievedFrames += actual;
    return actual;
}

}

