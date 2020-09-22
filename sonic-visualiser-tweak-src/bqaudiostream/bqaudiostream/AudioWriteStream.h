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

#ifndef BQ_AUDIO_FILE_WRITE_STREAM_H
#define BQ_AUDIO_FILE_WRITE_STREAM_H

#include "bqthingfactory/ThingFactory.h"

#include <string>

namespace breakfastquay {

/* Not thread-safe -- one per thread please. */

class AudioWriteStream
{
public:
    class Target {
    public:
        Target(std::string path, size_t channelCount, size_t sampleRate) :
            m_path(path), m_channelCount(channelCount), m_sampleRate(sampleRate)
        { }

        std::string getPath() const { return m_path; }
        size_t getChannelCount() const { return m_channelCount; }
        size_t getSampleRate() const { return m_sampleRate; }

    private:
        std::string m_path;
        size_t m_channelCount;
        size_t m_sampleRate;
    };

    virtual ~AudioWriteStream() { }

    virtual std::string getError() const { return ""; }

    std::string getPath() const { return m_target.getPath(); }
    size_t getChannelCount() const { return m_target.getChannelCount(); }
    size_t getSampleRate() const { return m_target.getSampleRate(); }
    
    /**
     * May throw FileOperationFailed if encoding fails.
     */
    virtual void putInterleavedFrames(size_t frameCount, float *frames) = 0;
    
protected:
    AudioWriteStream(Target t) : m_target(t) { }
    Target m_target;
};

template <typename T>
class AudioWriteStreamBuilder :
public ConcreteThingBuilder<T, AudioWriteStream, AudioWriteStream::Target>
{
public:
    AudioWriteStreamBuilder(std::string uri, std::vector<std::string> extensions) :
        ConcreteThingBuilder<T, AudioWriteStream, AudioWriteStream::Target>
        (uri, extensions) {
//        std::cerr << "Registering stream builder: " << uri << std::endl;
    }
};

}

#endif
