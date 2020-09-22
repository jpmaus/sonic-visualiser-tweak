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

#ifdef HAVE_LIBPULSE

#include "PulseAudioIO.h"
#include "ApplicationPlaybackSource.h"
#include "ApplicationRecordTarget.h"
#include "Gains.h"
#include "Log.h"

#include "bqvec/VectorOps.h"
#include "bqvec/Allocators.h"

#include <iostream>
#include <sstream>
#include <cmath>
#include <climits>

using namespace std;

//#define DEBUG_PULSE_AUDIO_IO 1

namespace breakfastquay {

static void log(string message) {
    Log::log("PulseAudioIO: " + message);
}    

static string defaultDeviceName = "Default Device";

vector<string>
PulseAudioIO::getRecordDeviceNames()
{
    return { defaultDeviceName };
}

vector<string>
PulseAudioIO::getPlaybackDeviceNames()
{
    return { defaultDeviceName };
}

PulseAudioIO::PulseAudioIO(Mode mode,
                           ApplicationRecordTarget *target,
                           ApplicationPlaybackSource *source,
                           string /* recordDevice */,
                           string /* playbackDevice */) :
    SystemAudioIO(target, source),
    m_mode(mode),
    m_loop(0),
    m_api(0),
    m_context(0),
    m_in(0), 
    m_out(0),
    m_buffers(0),
    m_interleaved(0),
    m_bufferChannels(0),
    m_bufferSize(0),
    m_sampleRate(0),
    m_done(false),
    m_captureReady(false),
    m_playbackReady(false),
    m_suspended(false)
{
    log("PulseAudioIO: starting");

    if (m_mode == Mode::Playback) {
        m_target = 0;
    }
    if (m_mode == Mode::Record) {
        m_source = 0;
    }

    m_name = (source ? source->getClientName() :
              target ? target->getClientName() : "bqaudioio");

    m_loop = pa_mainloop_new();
    if (!m_loop) {
        m_startupError = "Failed to create PulseAudio main loop";
        log("ERROR: " + m_startupError);
        return;
    }

    m_api = pa_mainloop_get_api(m_loop);

    int sourceRate = 0;
    int targetRate = 0;

    ostringstream os;

    if (m_source) {
        sourceRate = m_source->getApplicationSampleRate();
        if (sourceRate != 0) {
            os << "application source requests sample rate "
               << sourceRate << ", will try to comply";
            log(os.str());
            m_sampleRate = sourceRate;
        }
        m_outSpec.channels = 2;
        if (m_source->getApplicationChannelCount() != 0) {
            m_outSpec.channels = (uint8_t)m_source->getApplicationChannelCount();
        }
    } else {
        m_outSpec.channels = 0;
    }
    
    if (m_target) {
        targetRate = m_target->getApplicationSampleRate();
        if (targetRate != 0) {
            if (sourceRate != 0 && sourceRate != targetRate) {
                os << "WARNING: Source and target both provide sample rates, but different ones (source " << sourceRate << ", target " << targetRate << ") - using source rate";
                log(os.str());
            } else {
                os << "application target requests sample rate "
                   << targetRate << ", will try to comply";
                log(os.str());
                m_sampleRate = targetRate;
            }
        }
        m_inSpec.channels = 2;
        if (m_target->getApplicationChannelCount() != 0) {
            m_inSpec.channels = (uint8_t)m_target->getApplicationChannelCount();
        }
    } else {
        m_inSpec.channels = 0;
    }

    if (m_sampleRate == 0) {
        log("neither source nor target requested a sample rate, requesting default rate of 44100");
        m_sampleRate = 44100;
    }

    m_bufferSize = m_sampleRate / 2; // initially
    
    m_inSpec.rate = m_sampleRate;
    m_outSpec.rate = m_sampleRate;
    
    m_inSpec.format = PA_SAMPLE_FLOAT32NE;
    m_outSpec.format = PA_SAMPLE_FLOAT32NE;
    
    m_bufferChannels = std::max(m_inSpec.channels, m_outSpec.channels);
    m_buffers = allocate_and_zero_channels<float>(m_bufferChannels, m_bufferSize);
    m_interleaved = allocate_and_zero<float>(m_bufferChannels * m_bufferSize);

    m_context = pa_context_new(m_api, m_name.c_str());
    if (!m_context) {
        m_startupError = "Failed to create PulseAudio context object";
        log("ERROR: " + m_startupError);
        return;
    }

    pa_context_set_state_callback(m_context, contextStateChangedStatic, this);

    pa_context_connect(m_context, 0, (pa_context_flags_t)0, 0); // default server

    m_loopthread = thread([this]() { threadRun(); });

    log("started successfully");
}

PulseAudioIO::~PulseAudioIO()
{
    log("PulseAudioIO: closing");

    if (m_context) {

        // (if we have no m_context, then we never started up PA
        // successfully at all so there's nothing to do for this bit)
    
        {
            if (m_loop) {
                pa_mainloop_wakeup(m_loop);
            }
        
            lock_guard<mutex> cguard(m_contextMutex);
            lock_guard<mutex> lguard(m_loopMutex);
            lock_guard<mutex> sguard(m_streamMutex);

            m_done = true;

            if (m_loop) {
                pa_signal_done();
                pa_mainloop_quit(m_loop, 0);
            }
        }
    
        m_loopthread.join();

        {
            lock_guard<mutex> sguard(m_streamMutex);
    
            if (m_in) {
                pa_stream_unref(m_in);
                m_in = 0;
            }
            if (m_out) {
                pa_stream_unref(m_out);
                m_out = 0;
            }
        }

        {
            lock_guard<mutex> cguard(m_contextMutex);
    
            if (m_context) {
                pa_context_unref(m_context);
                m_context = 0;
            }
        }
    }
    
    deallocate_channels(m_buffers, m_bufferChannels);
    deallocate(m_interleaved);
    
    log("closed");
}

void
PulseAudioIO::threadRun()
{
    int rv = 0;

    while (1) {

        {
#ifdef DEBUG_PULSE_AUDIO_IO
//            cerr << "PulseAudioIO::threadRun: locking loop mutex for prepare" << endl;
#endif
            lock_guard<mutex> lguard(m_loopMutex);
            if (m_done) return;

            rv = pa_mainloop_prepare(m_loop, 100);
            if (rv < 0) {
                log("ERROR: threadRun: Failure in pa_mainloop_prepare");
                return;
            }

            rv = pa_mainloop_poll(m_loop);
            if (rv < 0) {
                log("ERROR: threadRun: Failure in pa_mainloop_poll");
                return;
            }
        }

        this_thread::yield();

        {
#ifdef DEBUG_PULSE_AUDIO_IO
//            cerr << "PulseAudioIO::threadRun: locking loop mutex for dispatch" << endl;
#endif
            lock_guard<mutex> lguard(m_loopMutex);
            if (m_done) return;

            rv = pa_mainloop_dispatch(m_loop);
            if (rv < 0) {
                log("ERROR: threadRun: Failure in pa_mainloop_dispatch");
                return;
            }
        }

        this_thread::yield();
    }
}

bool
PulseAudioIO::isSourceOK() const
{
    if (m_mode == Mode::Playback) {
        // record source is irrelevant in playback mode
        return true;
    } else {
        return (m_context != 0);
    }
}

bool
PulseAudioIO::isTargetOK() const
{
    if (m_mode == Mode::Record) {
        // playback target is irrelevant in record mode
        return true;
    } else {
        return (m_context != 0);
    }
}

bool
PulseAudioIO::isSourceReady() const
{
    return m_captureReady;
}

bool
PulseAudioIO::isTargetReady() const
{
    return m_playbackReady;
}

double
PulseAudioIO::getCurrentTime() const
{
    if (!m_out) return 0.0;

    pa_usec_t usec = 0;
    pa_stream_get_time(m_out, &usec);
    return double(usec) / 1000000.0;
}

void
PulseAudioIO::streamWriteStatic(pa_stream *,
                                size_t length,
                                void *data)
{
    PulseAudioIO *io = (PulseAudioIO *)data;

    if (length > INT_MAX) return;
    
    io->streamWrite(int(length));
}

void
PulseAudioIO::checkBufferCapacity(int nframes)
{
    if (nframes > m_bufferSize) {

        m_buffers = reallocate_and_zero_extend_channels
            (m_buffers,
             m_bufferChannels, m_bufferSize,
             m_bufferChannels, nframes);

        m_interleaved = reallocate
            (m_interleaved,
             m_bufferChannels * m_bufferSize,
             m_bufferChannels * nframes);
        
        m_bufferSize = nframes;
    }
}

void
PulseAudioIO::streamWrite(int requested)
{
#ifdef DEBUG_PULSE_AUDIO_IO    
    cerr << "PulseAudioIO::streamWrite(" << requested << ")" << endl;
#endif

#ifdef DEBUG_PULSE_AUDIO_IO
    cerr << "PulseAudioIO::streamWrite: locking stream mutex" << endl;
#endif

    // Pulse is a consumer system with long buffers, this is not a RT
    // context like the other drivers
    lock_guard<mutex> guard(m_streamMutex);
    if (m_done) return;
    if (!m_source) return;

    pa_usec_t latency = 0;
    int negative = 0;
    if (!pa_stream_get_latency(m_out, &latency, &negative)) {
        int latframes = latencyFrames(latency);
        if (latframes > 0) m_source->setSystemPlaybackLatency(latframes);
    }

    int channels = m_outSpec.channels;
    if (channels == 0) return;

    int nframes = requested / int(channels * sizeof(float));

    checkBufferCapacity(nframes);

#ifdef DEBUG_PULSE_AUDIO_IO
    cerr << "PulseAudioIO::streamWrite: nframes = " << nframes << endl;
#endif

    int received = m_source->getSourceSamples(m_buffers, channels, nframes);
    
    if (received < nframes) {
        for (int c = 0; c < channels; ++c) {
            v_zero(m_buffers[c] + received, nframes - received);
        }
    }
        
    float peakLeft = 0.0, peakRight = 0.0;
    auto gain = Gains::gainsFor(m_outputGain, m_outputBalance, channels); 

    for (int c = 0; c < channels; ++c) {
        v_scale(m_buffers[c], gain[c], nframes);
    }
    
    for (int c = 0; c < channels && c < 2; ++c) {
	float peak = 0.f;
        for (int i = 0; i < nframes; ++i) {
            if (m_buffers[c][i] > peak) {
                peak = m_buffers[c][i];
            }
        }
        if (c == 0) peakLeft = peak;
        if (c == 1 || channels == 1) peakRight = peak;
    }

    v_interleave(m_interleaved, m_buffers, channels, nframes);

#ifdef DEBUG_PULSE_AUDIO_IO
    cerr << "calling pa_stream_write with "
         << nframes * channels * sizeof(float) << " bytes" << endl;
#endif

    pa_stream_write(m_out, m_interleaved,
                    nframes * channels * sizeof(float),
                    0, 0, PA_SEEK_RELATIVE);

    m_source->setOutputLevels(peakLeft, peakRight);

    return;
}

void
PulseAudioIO::streamReadStatic(pa_stream *,
                               size_t length,
                               void *data)
{
    PulseAudioIO *io = (PulseAudioIO *)data;
    
    if (length > INT_MAX) return;
    
    io->streamRead(int(length));
}

void
PulseAudioIO::streamRead(int available)
{
#ifdef DEBUG_PULSE_AUDIO_IO
    cerr << "PulseAudioIO::streamRead(" << available << ")" << endl;
#endif

#ifdef DEBUG_PULSE_AUDIO_IO
    cerr << "PulseAudioIO::streamRead: locking stream mutex" << endl;
#endif
    lock_guard<mutex> guard(m_streamMutex);
    if (m_done) return;
    if (!m_target) return;
    
    pa_usec_t latency = 0;
    int negative = 0;
    if (!pa_stream_get_latency(m_in, &latency, &negative)) {
        int latframes = latencyFrames(latency);
        if (latframes > 0) m_target->setSystemRecordLatency(latframes);
    }

    int channels = m_inSpec.channels;
    if (channels == 0) return;

    int nframes = available / int(channels * sizeof(float));

#ifdef DEBUG_PULSE_AUDIO_IO
    cerr << "PulseAudioIO::streamRead: nframes = " << nframes << endl;
#endif

    checkBufferCapacity(nframes);
    
    float peakLeft = 0.0, peakRight = 0.0;

    size_t actual = available;
    
    const void *input = 0;
    pa_stream_peek(m_in, &input, &actual);

    int actualFrames = int(actual) / int(channels * sizeof(float));

    if (actualFrames < nframes) {
        ostringstream os;
        os << "WARNING: streamRead: read " << actualFrames
           << " frames, expected " << nframes;
        log(os.str());
    }
    
    const float *finput = (const float *)input;

    v_deinterleave(m_buffers, finput, channels, actualFrames);

    for (int c = 0; c < channels && c < 2; ++c) {
	float peak = 0.f;
        for (int i = 0; i < actualFrames; ++i) {
            if (m_buffers[c][i] > peak) {
                peak = m_buffers[c][i];
            }
        }
	if (c == 0) peakLeft = peak;
	if (c > 0 || channels == 1) peakRight = peak;
    }

    m_target->putSamples(m_buffers, channels, actualFrames);
    m_target->setInputLevels(peakLeft, peakRight);

    pa_stream_drop(m_in);

    return;
}

void
PulseAudioIO::streamStateChangedStatic(pa_stream *stream,
                                            void *data)
{
    PulseAudioIO *io = (PulseAudioIO *)data;
    
    io->streamStateChanged(stream);
}

void
PulseAudioIO::streamStateChanged(pa_stream *stream)
{
#ifdef DEBUG_PULSE_AUDIO_IO
    cerr << "PulseAudioIO::streamStateChanged" << endl;
#endif

#ifdef DEBUG_PULSE_AUDIO_IO
    cerr << "PulseAudioIO::streamStateChanged: locking stream mutex" << endl;
#endif
    lock_guard<mutex> guard(m_streamMutex);
    if (m_done) return;

    assert(stream == m_in || stream == m_out);

    switch (pa_stream_get_state(stream)) {

        case PA_STREAM_UNCONNECTED:
        case PA_STREAM_CREATING:
        case PA_STREAM_TERMINATED:
            break;

        case PA_STREAM_READY:
        {
            if (stream == m_in) {
                log("streamStateChanged: Capture ready");
                m_captureReady = true;
            } else {
                log("streamStateChanged: Playback ready");
                m_playbackReady = true;
            }                

            pa_usec_t latency = 0;
            int negative = 0;
            
            if (m_source && (stream == m_out)) {
                m_source->setSystemPlaybackSampleRate(m_sampleRate);
                m_source->setSystemPlaybackChannelCount(m_outSpec.channels);
                if (pa_stream_get_latency(m_out, &latency, &negative)) {
                    log("streamStateChanged: Failed to query playback latency");
                } else {
                    ostringstream os;
                    int latframes = latencyFrames(latency);
                    os << "playback latency = " << latency << " usec, "
                       << latframes << " frames";
                    log(os.str());
                    m_source->setSystemPlaybackLatency(latframes);
                }
            }
            if (m_target && (stream == m_in)) {
                m_target->setSystemRecordSampleRate(m_sampleRate);
                m_target->setSystemRecordChannelCount(m_inSpec.channels);
                if (pa_stream_get_latency(m_out, &latency, &negative)) {
                    log("streamStateChanged: Failed to query record latency");
                } else {
                    ostringstream os;
                    int latframes = latencyFrames(latency);
                    os << "record latency = " << latency << " usec, "
                       << latframes << " frames";
                    log(os.str());
                    m_target->setSystemRecordLatency(latframes);
                }
            }

            break;
        }

        case PA_STREAM_FAILED:
        default:
            log(string("streamStateChanged: Error: ") +
                pa_strerror(pa_context_errno(m_context)));
            //!!! do something...
            break;
    }

#ifdef DEBUG_PULSE_AUDIO_IO
    cerr << "PulseAudioIO::streamStateChanged complete" << endl;
#endif
}

void
PulseAudioIO::suspend()
{
    if (m_loop) pa_mainloop_wakeup(m_loop);
    
#ifdef DEBUG_PULSE_AUDIO_IO
    cerr << "PulseAudioIO::suspend: locking all mutexes" << endl;
#endif
    {
        lock_guard<mutex> cguard(m_contextMutex);
        if (m_suspended) return;
    }

    lock_guard<mutex> lguard(m_loopMutex);
#ifdef DEBUG_PULSE_AUDIO_IO
    cerr << "PulseAudioIO::suspend: loop mutex ok" << endl;
#endif
    
    lock_guard<mutex> sguard(m_streamMutex);
#ifdef DEBUG_PULSE_AUDIO_IO
    cerr << "PulseAudioIO::suspend: stream mutex ok" << endl;
#endif

    if (m_done) return;
    
    if (m_in) {
        pa_stream_cork(m_in, 1, 0, 0);
        pa_stream_flush(m_in, 0, 0);
    }

    if (m_out) {
        pa_stream_cork(m_out, 1, 0, 0);
        pa_stream_flush(m_out, 0, 0);
    }

    m_suspended = true;
    
#ifdef DEBUG_PULSE_AUDIO_IO
    cerr << "PulseAudioIO::suspend: corked!" << endl;
#endif
}

void
PulseAudioIO::resume()
{
    if (m_loop) pa_mainloop_wakeup(m_loop);
    
#ifdef DEBUG_PULSE_AUDIO_IO
    cerr << "PulseAudioIO::resume: locking all mutexes" << endl;
#endif
    {
        lock_guard<mutex> cguard(m_contextMutex);
        if (!m_suspended) return;
    }

    lock_guard<mutex> lguard(m_loopMutex);
#ifdef DEBUG_PULSE_AUDIO_IO
    cerr << "PulseAudioIO::suspend: loop mutex ok" << endl;
#endif

    lock_guard<mutex> sguard(m_streamMutex);
#ifdef DEBUG_PULSE_AUDIO_IO
    cerr << "PulseAudioIO::suspend: stream mutex ok" << endl;
#endif
    if (m_done) return;

    if (m_in) {
        pa_stream_flush(m_in, 0, 0);
        pa_stream_cork(m_in, 0, 0, 0);
    }

    if (m_out) {
        pa_stream_cork(m_out, 0, 0, 0);
    }

    m_suspended = false;
    
#ifdef DEBUG_PULSE_AUDIO_IO
    cerr << "PulseAudioIO::resume: uncorked!" << endl;
#endif
}

void
PulseAudioIO::contextStateChangedStatic(pa_context *,
                                        void *data)
{
    PulseAudioIO *io = (PulseAudioIO *)data;
    io->contextStateChanged();
}

void
PulseAudioIO::contextStateChanged()
{
#ifdef DEBUG_PULSE_AUDIO_IO
    cerr << "PulseAudioIO::contextStateChanged" << endl;
#endif
#ifdef DEBUG_PULSE_AUDIO_IO
    cerr << "PulseAudioIO::contextStateChanged: locking context mutex" << endl;
#endif
    lock_guard<mutex> guard(m_contextMutex);

    switch (pa_context_get_state(m_context)) {

        case PA_CONTEXT_UNCONNECTED:
        case PA_CONTEXT_CONNECTING:
        case PA_CONTEXT_AUTHORIZING:
        case PA_CONTEXT_SETTING_NAME:
            break;

        case PA_CONTEXT_READY:
        {
            log("contextStateChanged: Ready");

            pa_stream_flags_t flags;
            flags = pa_stream_flags_t(PA_STREAM_INTERPOLATE_TIMING |
                                      PA_STREAM_AUTO_TIMING_UPDATE);
            if (m_suspended) {
                flags = pa_stream_flags_t(flags | PA_STREAM_START_CORKED);
            }

            if (m_inSpec.channels > 0) {
                
                m_in = pa_stream_new(m_context, "Capture", &m_inSpec, 0);

                if (!m_in) {
                    log("contextStateChanged: Failed to create capture stream");
                } else {
                    pa_stream_set_state_callback(m_in, streamStateChangedStatic, this);
                    pa_stream_set_read_callback(m_in, streamReadStatic, this);
                    pa_stream_set_overflow_callback(m_in, streamOverflowStatic, this);
                    pa_stream_set_underflow_callback(m_in, streamUnderflowStatic, this);
            
                    if (pa_stream_connect_record (m_in, 0, 0, flags)) {
                        log("contextStateChanged: Failed to connect record stream");
                    }
                }
            }

            if (m_outSpec.channels > 0) {

                m_out = pa_stream_new(m_context, "Playback", &m_outSpec, 0);

                if (!m_out) {
                    log("contextStateChanged: Failed to create playback stream");
                } else {
                    pa_stream_set_state_callback(m_out, streamStateChangedStatic, this);
                    pa_stream_set_write_callback(m_out, streamWriteStatic, this);
                    pa_stream_set_overflow_callback(m_out, streamOverflowStatic, this);
                    pa_stream_set_underflow_callback(m_out, streamUnderflowStatic, this);

                    if (pa_stream_connect_playback(m_out, 0, 0, flags, 0, 0)) { 
                        log("contextStateChanged: Failed to connect playback stream");
                    }
                }
            }
            
            break;
        }

        case PA_CONTEXT_TERMINATED:
            log("contextStateChanged: Terminated");
            break;

        case PA_CONTEXT_FAILED:
        default:
            log(string("contextStateChanged: Error: ") +
                pa_strerror(pa_context_errno(m_context)));
            break;
    }

#ifdef DEBUG_PULSE_AUDIO_IO
    cerr << "PulseAudioIO::contextStateChanged complete" << endl;
#endif
}

void
PulseAudioIO::streamOverflowStatic(pa_stream *, void *data)
{
    log("streamOverflowStatic: Overflow!");

    PulseAudioIO *io = (PulseAudioIO *)data;

    if (io->m_target) io->m_target->audioProcessingOverload();
    if (io->m_source) io->m_source->audioProcessingOverload();
}

void
PulseAudioIO::streamUnderflowStatic(pa_stream *, void *data)
{
    log("streamUnderflowStatic: Underflow!");
    
    PulseAudioIO *io = (PulseAudioIO *)data;

    if (io->m_target) io->m_target->audioProcessingOverload();
    if (io->m_source) io->m_source->audioProcessingOverload();
}


}

#endif /* HAVE_LIBPULSE */

