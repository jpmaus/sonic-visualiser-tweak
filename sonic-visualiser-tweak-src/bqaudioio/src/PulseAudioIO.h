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

#ifndef BQAUDIOIO_PULSE_AUDIO_IO_H
#define BQAUDIOIO_PULSE_AUDIO_IO_H

#ifdef HAVE_LIBPULSE

#include <pulse/pulseaudio.h>

#include "SystemAudioIO.h"
#include "AudioFactory.h"
#include "Mode.h"

#include <mutex>
#include <thread>

#include <vector>
#include <string>

namespace breakfastquay {

class ApplicationRecordTarget;
class ApplicationPlaybackSource;

class PulseAudioIO : public SystemAudioIO
{
public:
    PulseAudioIO(Mode mode,
                 ApplicationRecordTarget *recordTarget,
                 ApplicationPlaybackSource *playSource,
                 std::string recordDevice,
                 std::string playbackDevice);
    virtual ~PulseAudioIO();

    static std::vector<std::string> getRecordDeviceNames();
    static std::vector<std::string> getPlaybackDeviceNames();
    
    bool isSourceOK() const override;
    bool isSourceReady() const override;
    bool isTargetOK() const override;
    bool isTargetReady() const override;

    double getCurrentTime() const override;

    void suspend() override;
    void resume() override;

    std::string getStartupErrorString() const { return m_startupError; }

protected:
    void streamWrite(int);
    void streamRead(int);
    void streamStateChanged(pa_stream *);
    void contextStateChanged();

    static void streamWriteStatic(pa_stream *, size_t, void *);
    static void streamReadStatic(pa_stream *, size_t, void *);
    static void streamStateChangedStatic(pa_stream *, void *);
    static void streamOverflowStatic(pa_stream *, void *);
    static void streamUnderflowStatic(pa_stream *, void *);
    static void contextStateChangedStatic(pa_context *, void *);

    int latencyFrames(pa_usec_t latusec) {
        return int((double(latusec) / 1000000.0) * double(m_sampleRate));
    }

    std::mutex m_loopMutex;
    std::mutex m_contextMutex;
    mutable std::mutex m_streamMutex;
    std::thread m_loopthread;

    void threadRun();

    Mode m_mode;
    std::string m_name;
    
    pa_mainloop *m_loop;
    pa_mainloop_api *m_api;
    pa_context *m_context;
    pa_stream *m_in;
    pa_stream *m_out;
    pa_sample_spec m_inSpec;
    pa_sample_spec m_outSpec;

    float **m_buffers;
    float *m_interleaved;
    int m_bufferChannels;
    int m_bufferSize;
    int m_sampleRate;
    bool m_done;

    bool m_captureReady;
    bool m_playbackReady;

    bool m_suspended;

    std::string m_startupError;

    void checkBufferCapacity(int nframes);
    
    PulseAudioIO(const PulseAudioIO &)=delete;
    PulseAudioIO &operator=(const PulseAudioIO &)=delete;
};

}

#endif /* HAVE_LIBPULSE */

#endif

