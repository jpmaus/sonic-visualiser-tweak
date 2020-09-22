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

#ifdef HAVE_PORTAUDIO

#include "PortAudioIO.h"
#include "ApplicationPlaybackSource.h"
#include "ApplicationRecordTarget.h"
#include "Gains.h"
#include "Log.h"

#include "bqvec/VectorOps.h"
#include "bqvec/Allocators.h"

#include <iostream>
#include <sstream>
#include <cassert>
#include <cmath>
#include <climits>
#include <algorithm>

#include <mutex>

#ifndef __LINUX__
#ifndef _WIN32
#include <pthread.h>
#endif
#endif

//#define DEBUG_AUDIO_PORT_AUDIO_IO 1

using namespace std;

namespace breakfastquay {

static void log(string message) {
    Log::log("PortAudioIO: " + message);
}    

#ifdef __LINUX__
extern "C" {
void
PaAlsa_EnableRealtimeScheduling(PaStream *, int);
}
#endif

static bool // true if "can attempt on this platform", not "succeeded"
enableRT(PaStream *
#ifdef __LINUX__
         stream
#endif
    ) {
#ifdef __LINUX__
    // This will link only if the PA ALSA host API is linked statically
    PaAlsa_EnableRealtimeScheduling(stream, 1);
    return true;
#else
    return false;
#endif
}

static bool // true if "can attempt on this platform", not "succeeded"
enableRT() { // on current thread
#ifndef __LINUX__
#ifndef _WIN32
    sched_param param;
    param.sched_priority = 20;
    if (pthread_setschedparam(pthread_self(), SCHED_RR, &param)) {
        log("NOTE: couldn't set RT scheduling class");
    } else {
        log("NOTE: successfully set RT scheduling class");
    }
    return true;
#endif
#endif
    return false;
}

static bool paio_initialised = false;
static bool paio_working = false;
static mutex paio_init_mutex;

static bool initialise() {

    lock_guard<mutex> guard(paio_init_mutex);

    if (!paio_initialised) {
        PaError err = Pa_Initialize();
        paio_initialised = true;
        if (err != paNoError) {
            log("ERROR: Failed to initialize PortAudio");
            paio_working = false;
        } else {
            paio_working = true;
        }
    }

    return paio_working;
}

static void deinitialise() {

    lock_guard<mutex> guard(paio_init_mutex);

    if (paio_initialised && paio_working) {
	Pa_Terminate();
        paio_initialised = false;
    }
}

static
vector<string>
getDeviceNames(bool record)
{
    if (!initialise()) {
        return {};
    }
    
    vector<string> names;
    PaDeviceIndex count = Pa_GetDeviceCount();

    if (count < 0) {
        // error
        log(string("error in retrieving device list: ") + Pa_GetErrorText(count));
        return names;
    } else {
        ostringstream os;
        os << "have " << count << " device(s)";
        log(os.str());
    }
    
    for (int i = 0; i < count; ++i) {

        const PaDeviceInfo *info = Pa_GetDeviceInfo(i);

        ostringstream os;
        os << "device " << i << " of " << count << ":" << endl;
        os << "name = \"" << info->name << "\"" << endl;
        os << "maxInputChannels = " << info->maxInputChannels << endl;
        os << "maxOutputChannels = " << info->maxOutputChannels << endl;
        os << "defaultSampleRate = " << info->defaultSampleRate;
        log(os.str());

        if (record) {
            if (info->maxInputChannels > 0) {
                names.push_back(info->name);
            }
        } else {
            if (info->maxOutputChannels > 0) {
                names.push_back(info->name);
            }
        }
    }
    return names;
}

static
PaDeviceIndex
getDeviceIndex(string name, bool record)
{
    {
        ostringstream os;
        os << "getDeviceIndex: name = \"" << name << "\", record = " << record;
        log(os.str());
    }
    
    if (name != "") {
        PaDeviceIndex count = Pa_GetDeviceCount();
        if (count < 0) {
            log(string("error in retrieving device index: ") + Pa_GetErrorText(count));
        }
        for (int i = 0; i < count; ++i) {
            const PaDeviceInfo *info = Pa_GetDeviceInfo(i);
            if (record) {
                if (info->maxInputChannels > 0) {
                    if (name == info->name) {
                        return i;
                    }
                }
            } else {
                if (info->maxOutputChannels > 0) {
                    if (name == info->name) {
                        return i;
                    }
                }
            }
        }
    }

    // no name supplied, or no match in device list
    if (record) {
        return Pa_GetDefaultInputDevice();
    } else {
        return Pa_GetDefaultOutputDevice();
    }
}

vector<string>
PortAudioIO::getRecordDeviceNames()
{
    return getDeviceNames(true);
}

vector<string>
PortAudioIO::getPlaybackDeviceNames()
{
    return getDeviceNames(false);
}

PortAudioIO::PortAudioIO(Mode mode,
                         ApplicationRecordTarget *target,
                         ApplicationPlaybackSource *source,
                         string recordDevice,
                         string playbackDevice) :
    SystemAudioIO(target, source),
    m_stream(0),
    m_mode(mode),
    m_bufferSize(0),
    m_sampleRate(0),
    m_inputLatency(0),
    m_outputLatency(0),
    m_prioritySet(false),
    m_suspended(false),
    m_buffers(0),
    m_bufferChannels(0)
{
    log("starting");

    if (!initialise()) return;

    // solely to debug-log the list of devices, so both arg
    // and return value are irrelevant here:
    (void)getDeviceNames(false);

    if (m_mode == Mode::Playback) {
        m_target = 0;
    }
    if (m_mode == Mode::Record) {
        m_source = 0;
    }
    
    PaError err;

    m_bufferSize = 0;

    PaStreamParameters ip, op;
    ip.device = getDeviceIndex(recordDevice, true);
    op.device = getDeviceIndex(playbackDevice, false);

    {
        ostringstream os;
        os << "Obtained playback device index " << op.device
           << " and record device index " << ip.device;
        log(os.str());
    }

    const PaDeviceInfo *inInfo = Pa_GetDeviceInfo(ip.device);
    const PaDeviceInfo *outInfo = Pa_GetDeviceInfo(op.device);
    if (outInfo) {
        m_sampleRate = outInfo->defaultSampleRate;
    }

    m_sourceChannels = 2;
    m_targetChannels = 2;

    int sourceRate = 0;
    int targetRate = 0;
    
    if (m_source) {
        sourceRate = m_source->getApplicationSampleRate();
        if (sourceRate != 0) {
            m_sampleRate = sourceRate;
        }
        if (m_source->getApplicationChannelCount() != 0) {
            m_sourceChannels = m_source->getApplicationChannelCount();
        }
    }
    if (m_target) {
        targetRate = m_target->getApplicationSampleRate();
        if (targetRate != 0) {
            if (sourceRate != 0 && sourceRate != targetRate) {
                ostringstream os;
                os << "WARNING: Source and target both provide sample rates, but different ones (source " << sourceRate << ", target " << targetRate << ") - using source rate";
            } else {
                m_sampleRate = targetRate;
            }
        }
        if (m_target->getApplicationChannelCount() != 0) {
            m_targetChannels = m_target->getApplicationChannelCount();
        }
    }
    if (m_sampleRate == 0) {
        m_sampleRate = 44100;
    }

    m_inputChannels = m_targetChannels;
    m_outputChannels = m_sourceChannels;

    if (inInfo &&
        m_inputChannels > inInfo->maxInputChannels &&
        inInfo->maxInputChannels > 0) {
        m_inputChannels = inInfo->maxInputChannels;
    }

    if (outInfo &&
        m_outputChannels > outInfo->maxOutputChannels &&
        outInfo->maxOutputChannels > 0) {
        m_outputChannels = outInfo->maxOutputChannels;
    }
    
    ip.channelCount = m_inputChannels;
    op.channelCount = m_outputChannels;
    ip.sampleFormat = paFloat32;
    op.sampleFormat = paFloat32;
    ip.suggestedLatency = 0.2;
    op.suggestedLatency = 0.2;
    ip.hostApiSpecificStreamInfo = 0;
    op.hostApiSpecificStreamInfo = 0;

    m_bufferSize = 0;
    err = openStream(m_mode, &m_stream, &ip, &op, m_sampleRate,
                     paFramesPerBufferUnspecified, this);

    if (err != paNoError) {
	m_bufferSize = 1024;
        err = openStream(m_mode, &m_stream, &ip, &op, m_sampleRate,
                         1024, this);
    }

    if (err != paNoError) {
        if (m_inputChannels != 2 || m_outputChannels != 2) {

            log(string("WARNING: Failed to open PortAudio stream: ") + 
                Pa_GetErrorText(err) + ": trying again with 2x2 configuration");
            
            m_inputChannels = 2;
            m_outputChannels = 2;
            ip.channelCount = m_inputChannels;
            op.channelCount = m_outputChannels;

            m_bufferSize = 0;
            err = openStream(m_mode, &m_stream, &ip, &op, m_sampleRate,
                             paFramesPerBufferUnspecified, this);

            m_bufferSize = 1024;
            if (err != paNoError) {
                err = openStream(m_mode, &m_stream, &ip, &op, m_sampleRate,
                                 1024, this);
            }
        }
    }
    
    if (err != paNoError) {
        m_startupError = "Failed to open PortAudio stream: ";
        m_startupError += Pa_GetErrorText(err);
	log("ERROR: " + m_startupError);
	m_stream = 0;
        deinitialise();
	return;
    }

    const PaStreamInfo *info = Pa_GetStreamInfo(m_stream);
    m_outputLatency = int(info->outputLatency * m_sampleRate + 0.001);
    m_inputLatency = int(info->inputLatency * m_sampleRate + 0.001);
    if (m_bufferSize == 0) m_bufferSize = m_outputLatency;
    if (m_bufferSize == 0) m_bufferSize = m_inputLatency;

    if (enableRT(m_stream)) {
        m_prioritySet = true;
    }

    {
        ostringstream os;
        os << "block size " << m_bufferSize;
        log(os.str());
    }

    if (m_source) {
	m_source->setSystemPlaybackBlockSize(m_bufferSize);
	m_source->setSystemPlaybackSampleRate(int(round(m_sampleRate)));
	m_source->setSystemPlaybackLatency(m_outputLatency);
        m_source->setSystemPlaybackChannelCount(m_outputChannels);
    }

    if (m_target) {
	m_target->setSystemRecordBlockSize(m_bufferSize);
	m_target->setSystemRecordSampleRate(int(round(m_sampleRate)));
	m_target->setSystemRecordLatency(m_inputLatency);
        m_target->setSystemRecordChannelCount(m_inputChannels);
    }

    m_bufferChannels = std::max(std::max(m_sourceChannels, m_targetChannels),
                                std::max(m_inputChannels, m_outputChannels));
    m_buffers = allocate_and_zero_channels<float>(m_bufferChannels, m_bufferSize);

    err = Pa_StartStream(m_stream);

    if (err != paNoError) {
	m_startupError = "Failed to start PortAudio stream: ";
        m_startupError += Pa_GetErrorText(err);
        log("ERROR: " + m_startupError);
	Pa_CloseStream(m_stream);
	m_stream = 0;
        deinitialise();
	return;
    }

    log("started successfully");
}

PortAudioIO::~PortAudioIO()
{
    if (m_stream) {
	PaError err;
        if (!m_suspended) {
            err = Pa_StopStream(m_stream);
            if (err != paNoError) {
                log("ERROR: Failed to stop PortAudio stream");
                err = Pa_AbortStream(m_stream);
                if (err != paNoError) {
                    log("ERROR: Failed to abort PortAudio stream");
                }
            }
	}
	err = Pa_CloseStream(m_stream);
	if (err != paNoError) {
	    log("ERROR: Failed to close PortAudio stream");
	}
        deallocate_channels(m_buffers, m_bufferChannels);
        deinitialise();
        log("closed");
    }
}

PaError
PortAudioIO::openStream(Mode mode,
                        PaStream **stream,
                        const PaStreamParameters *inputParameters,
                        const PaStreamParameters *outputParameters,
                        double sampleRate,
                        unsigned long framesPerBuffer,
                        void *data)
{
    switch (mode) {
    case Mode::Playback:
        return Pa_OpenStream(stream, 0, outputParameters, sampleRate,
                             framesPerBuffer, paNoFlag, processStatic, data);
    case Mode::Record:
        return Pa_OpenStream(stream, inputParameters, 0, sampleRate,
                             framesPerBuffer, paNoFlag, processStatic, data);
    case Mode::Duplex:
        return Pa_OpenStream(stream, inputParameters, outputParameters,
                             sampleRate,
                             framesPerBuffer, paNoFlag, processStatic, data);
    };
    return paNoError;
}

bool
PortAudioIO::isSourceOK() const
{
    if (m_mode == Mode::Playback) {
        // record source is irrelevant in playback mode
        return true;
    } else {
        return (m_stream != 0);
    }
}

bool
PortAudioIO::isTargetOK() const
{
    if (m_mode == Mode::Record) {
        // playback target is irrelevant in record mode
        return true;
    } else {
        return (m_stream != 0);
    }
}

double
PortAudioIO::getCurrentTime() const
{
    if (!m_stream) return 0.0;
    else return Pa_GetStreamTime(m_stream);
}

void
PortAudioIO::suspend()
{
    log("suspend called");

    if (m_suspended || !m_stream) return;
    PaError err = Pa_StopStream(m_stream);
    if (err != paNoError) {
        log("ERROR: Failed to stop PortAudio stream");
    }
    
    m_suspended = true;
    log("suspended");
}

void
PortAudioIO::resume()
{
    log("resume called");

    if (!m_suspended || !m_stream) return;
    PaError err = Pa_StartStream(m_stream);
    if (err != paNoError) {
        cerr << "ERROR: Failed to restart PortAudio stream";
    }

    m_suspended = false;
    log("resumed");
}

int
PortAudioIO::processStatic(const void *input, void *output,
                           unsigned long nframes,
                           const PaStreamCallbackTimeInfo *timeInfo,
                           PaStreamCallbackFlags flags, void *data)
{
    return ((PortAudioIO *)data)->process(input, output,
                                          nframes, timeInfo,
                                          flags);
}

int
PortAudioIO::process(const void *inputBuffer, void *outputBuffer,
                     unsigned long pa_nframes,
                     const PaStreamCallbackTimeInfo *,
                     PaStreamCallbackFlags)
{
#ifdef DEBUG_AUDIO_PORT_AUDIO_IO    
    cout << "PortAudioIO::process(" << pa_nframes << ")" << endl;
#endif

    if (!m_prioritySet) {
        enableRT();
        m_prioritySet = true;
    }

    if (!m_source && !m_target) return 0;
    if (!m_stream) return 0;

    if (pa_nframes > INT_MAX) pa_nframes = 0;

    int nframes = int(pa_nframes);

    if (nframes > m_bufferSize) {
        m_buffers = reallocate_and_zero_extend_channels
            (m_buffers,
             m_bufferChannels, m_bufferSize,
             m_bufferChannels, nframes);
        m_bufferSize = nframes;
    }
    
    const float *input = (const float *)inputBuffer;
    float *output = (float *)outputBuffer;

    float peakLeft, peakRight;

    if (m_target && input) {

        v_deinterleave
            (m_buffers, input, m_inputChannels, nframes);
        
        v_reconfigure_channels_inplace
            (m_buffers, m_targetChannels, m_inputChannels, nframes);

        peakLeft = 0.0, peakRight = 0.0;

        for (int c = 0; c < m_targetChannels && c < 2; ++c) {
            float peak = 0.f;
            for (int i = 0; i < nframes; ++i) {
                if (m_buffers[c][i] > peak) {
                    peak = m_buffers[c][i];
                }
            }
            if (c == 0) peakLeft = peak;
            if (c > 0 || m_targetChannels == 1) peakRight = peak;
        }

        m_target->putSamples(m_buffers, m_targetChannels, nframes);
        m_target->setInputLevels(peakLeft, peakRight);
    }

    if (m_source && output) {

        int received = m_source->getSourceSamples(m_buffers, m_sourceChannels, nframes);

        if (received < nframes) {
            for (int c = 0; c < m_sourceChannels; ++c) {
                v_zero(m_buffers[c] + received, nframes - received);
            }
        }

        v_reconfigure_channels_inplace
            (m_buffers, m_outputChannels, m_sourceChannels, nframes);

        auto gain = Gains::gainsFor(m_outputGain, m_outputBalance, m_outputChannels);
        for (int c = 0; c < m_outputChannels; ++c) {
            v_scale(m_buffers[c], gain[c], nframes);
        }

        peakLeft = 0.0, peakRight = 0.0;
        for (int c = 0; c < m_outputChannels && c < 2; ++c) {
            float peak = 0.f;
            for (int i = 0; i < nframes; ++i) {
                if (m_buffers[c][i] > peak) {
                    peak = m_buffers[c][i];
                }
            }
            if (c == 0) peakLeft = peak;
            if (c == 1 || m_outputChannels == 1) peakRight = peak;
        }

        v_interleave
            (output, m_buffers, m_outputChannels, nframes);
        
        m_source->setOutputLevels(peakLeft, peakRight);

    } else if (m_outputChannels > 0) {

        v_zero(output, m_outputChannels * nframes);
    }

    return 0;
}

}

#endif /* HAVE_PORTAUDIO */

