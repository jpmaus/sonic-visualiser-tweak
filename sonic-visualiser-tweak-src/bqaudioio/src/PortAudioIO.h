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

#ifndef BQAUDIOIO_PORTAUDIO_IO_H
#define BQAUDIOIO_PORTAUDIO_IO_H

#ifdef HAVE_PORTAUDIO

#include <portaudio.h>

#include "SystemAudioIO.h"
#include "AudioFactory.h"
#include "Mode.h"

#include <vector>
#include <string>

namespace breakfastquay {

class ApplicationRecordTarget;
class ApplicationPlaybackSource;

class PortAudioIO : public SystemAudioIO
{
public:
    PortAudioIO(Mode mode,
                ApplicationRecordTarget *recordTarget,
                ApplicationPlaybackSource *playSource,
                std::string recordDevice,
                std::string playbackDevice);
    virtual ~PortAudioIO();

    static std::vector<std::string> getRecordDeviceNames();
    static std::vector<std::string> getPlaybackDeviceNames();
    
    virtual bool isSourceOK() const override;
    virtual bool isTargetOK() const override;

    virtual double getCurrentTime() const override;

    virtual void suspend() override;
    virtual void resume() override;

    std::string getStartupErrorString() const { return m_startupError; }
    
protected:
    int process(const void *input, void *output, unsigned long frames,
                const PaStreamCallbackTimeInfo *timeInfo,
                PaStreamCallbackFlags statusFlags);

    static PaError openStream(Mode, PaStream **,
                              const PaStreamParameters *,
                              const PaStreamParameters *,
                              double, unsigned long, void *);
    
    static int processStatic(const void *, void *, unsigned long,
                             const PaStreamCallbackTimeInfo *,
                             PaStreamCallbackFlags, void *);

    PaStream *m_stream;

    Mode m_mode;
    int m_bufferSize;
    double m_sampleRate;
    int m_sourceChannels;
    int m_targetChannels;
    int m_inputChannels;
    int m_outputChannels;
    int m_inputLatency;
    int m_outputLatency;
    bool m_prioritySet;
    bool m_suspended;
    float **m_buffers;
    int m_bufferChannels;
    std::string m_startupError;

    PortAudioIO(const PortAudioIO &)=delete;
    PortAudioIO &operator=(const PortAudioIO &)=delete;
};

}

#endif /* HAVE_PORTAUDIO */

#endif

