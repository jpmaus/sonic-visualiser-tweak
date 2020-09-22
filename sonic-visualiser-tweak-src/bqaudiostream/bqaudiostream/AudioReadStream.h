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

#ifndef BQ_AUDIO_READ_STREAM_H
#define BQ_AUDIO_READ_STREAM_H

#include <bqthingfactory/ThingFactory.h>
#include <bqvec/RingBuffer.h>

#include <string>
#include <vector>

namespace breakfastquay {

class Resampler;

/* Not thread-safe -- one per thread please. */

class AudioReadStream
{
public:
    virtual ~AudioReadStream();

    virtual std::string getError() const { return ""; }

    size_t getChannelCount() const { return m_channelCount; }
    size_t getSampleRate() const { return m_sampleRate; } // source stream rate
    
    void setRetrievalSampleRate(size_t);
    size_t getRetrievalSampleRate() const;

    virtual std::string getTrackName() const = 0;
    virtual std::string getArtistName() const = 0;
    
    /**
     * Retrieve \count frames of audio data (that is, \count *
     * getChannelCount() samples) from the source and store in
     * \frames.  Return the number of frames actually retrieved; this
     * will differ from \count only when the end of stream is reached.
     * The region pointed to by \frames must contain enough space for
     * \count * getChannelCount() values.
     *
     * If a retrieval sample rate has been set, the audio will be
     * resampled to that rate (and \count refers to the number of
     * frames at the retrieval rate rather than the file's original
     * rate).
     *
     * May throw InvalidFileFormat if decoding fails.
     */
    size_t getInterleavedFrames(size_t count, float *frames);
    
protected:
    AudioReadStream();
    virtual size_t getFrames(size_t count, float *frames) = 0;
    int getResampledChunk(int count, float *frames);
    size_t m_channelCount;
    size_t m_sampleRate;
    size_t m_retrievalRate;
    size_t m_totalFileFrames;
    size_t m_totalRetrievedFrames;
    Resampler *m_resampler;
    RingBuffer<float> *m_resampleBuffer;
};

template <typename T>
class AudioReadStreamBuilder :
    public ConcreteThingBuilder<T, AudioReadStream, std::string>
{
public:
    AudioReadStreamBuilder(std::string uri, std::vector<std::string> extensions) :
        ConcreteThingBuilder<T, AudioReadStream, std::string>(uri, extensions) {
//        std::cerr << "Registering stream builder: " << uri << std::endl;
    }
};

}

#endif
