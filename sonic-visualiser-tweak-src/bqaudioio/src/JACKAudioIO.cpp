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

#ifdef HAVE_JACK

#include "JACKAudioIO.h"
#include "DynamicJACK.h"
#include "ApplicationPlaybackSource.h"
#include "ApplicationRecordTarget.h"
#include "Gains.h"
#include "Log.h"

#include <bqvec/Range.h>

#include <iostream>
#include <sstream>
#include <cmath>
#include <cstdio>
#include <cstring>
#include <climits>

#include <unistd.h> // getpid

//#define DEBUG_AUDIO_JACK_IO 1

using namespace std;

namespace breakfastquay {

static string defaultConnectionName = "Default Connection";
static string noConnectionName = "No Connection";

static void log(string message) {
    Log::log("JACKAudioIO: " + message);
}    

vector<string>
JACKAudioIO::getRecordDeviceNames()
{
    return { defaultConnectionName, noConnectionName };
}

vector<string>
JACKAudioIO::getPlaybackDeviceNames()
{
    return { defaultConnectionName, noConnectionName };
}

JACKAudioIO::JACKAudioIO(Mode mode,
                         ApplicationRecordTarget *target,
			 ApplicationPlaybackSource *source,
                         string recordDevice,
                         string playbackDevice) :
    SystemAudioIO(target, source),
    m_mode(mode),
    m_client(0),
    m_bufferSize(0),
    m_sampleRate(0)
{
    log("starting");
    
    if (m_mode == Mode::Playback) {
        m_target = 0;
    }
    if (m_mode == Mode::Record) {
        m_source = 0;
    }

    std::string clientName =
        (source ? source->getClientName() :
         target ? target->getClientName() : "bqaudioio");

    JackOptions options = JackNullOption;
    
#if defined(HAVE_PORTAUDIO) || defined(HAVE_LIBPULSE)
    options = JackNoStartServer;
#endif

    JackStatus status = JackStatus(0);
    m_client = jack_client_open(clientName.c_str(), options, &status);
    if (!m_client) {
        m_startupError = "Failed to connect to JACK server";
        log("ERROR: " + m_startupError);
        return;
    }

    m_bufferSize = jack_get_buffer_size(m_client);
    m_sampleRate = jack_get_sample_rate(m_client);

    jack_set_xrun_callback(m_client, xrunStatic, this);
    jack_set_process_callback(m_client, processStatic, this);

    if (jack_activate(m_client)) {
        m_startupError = "Failed to activate JACK client";
        log("ERROR: " + m_startupError);
        return;
    }

    bool connectRecord = (recordDevice != noConnectionName);
    bool connectPlayback = (playbackDevice != noConnectionName);
    
    setup(connectRecord, connectPlayback);

    log("started successfully");
}

JACKAudioIO::~JACKAudioIO()
{
    if (m_client) {
	jack_deactivate(m_client);
	jack_client_close(m_client);
        log("closed");
    }
}

bool
JACKAudioIO::isSourceOK() const
{
    if (m_mode == Mode::Playback) {
        // record source is irrelevant in playback mode
        return true;
    } else {
        return (m_client != 0);
    }
}

bool
JACKAudioIO::isTargetOK() const
{
    if (m_mode == Mode::Record) {
        // playback target is irrelevant in record mode
        return true;
    } else {
        return (m_client != 0);
    }
}

double
JACKAudioIO::getCurrentTime() const
{
    if (m_client && m_sampleRate) {
        return double(jack_frame_time(m_client)) / double(m_sampleRate);
    } else {
        return 0.0;
    }
}

int
JACKAudioIO::processStatic(jack_nframes_t nframes, void *arg)
{
    return ((JACKAudioIO *)arg)->process(nframes);
}

int
JACKAudioIO::xrunStatic(void *arg)
{
    return ((JACKAudioIO *)arg)->xrun();
}

void
JACKAudioIO::setup(bool connectRecord, bool connectPlayback)
{
    lock_guard<mutex> guard(m_mutex);

    int channelsPlay = 2;
    int channelsRec = 2;
    
    if (m_source) {
        m_source->setSystemPlaybackBlockSize(m_bufferSize);
        m_source->setSystemPlaybackSampleRate(m_sampleRate);
        if (m_source->getApplicationChannelCount() > 0) {
            channelsPlay = m_source->getApplicationChannelCount();
        }
    }
    if (m_target) {
        m_target->setSystemRecordBlockSize(m_bufferSize);
        m_target->setSystemRecordSampleRate(m_sampleRate);
        if (m_target->getApplicationChannelCount() > 0) {
            channelsRec = m_target->getApplicationChannelCount();
        }
    }

    if (!m_client) return;
    
    if (channelsPlay == int(m_outputs.size()) &&
        channelsRec == int(m_inputs.size())) {
	return;
    }

    const char **playPorts =
	jack_get_ports(m_client, NULL, NULL,
		       JackPortIsPhysical | JackPortIsInput);
    const char **capPorts =
	jack_get_ports(m_client, NULL, NULL,
		       JackPortIsPhysical | JackPortIsOutput);

    int playPortCount = 0;
    while (playPorts && playPorts[playPortCount]) ++playPortCount;

    int capPortCount = 0;
    while (capPorts && capPorts[capPortCount]) ++capPortCount;

    {
        ostringstream os;
        os << "Setup: have "
           << channelsPlay << " playback channels, "
           << channelsRec << " capture channels, "
           << playPortCount << " playback ports, "
           << capPortCount << " capture ports";
        log(os.str());
    }

    if (m_source) {

        while (int(m_outputs.size()) < channelsPlay) {
	
            char name[50];
            jack_port_t *port;

            sprintf(name, "out %ld", long(m_outputs.size() + 1));

            port = jack_port_register(m_client,
                                      name,
                                      JACK_DEFAULT_AUDIO_TYPE,
                                      JackPortIsOutput,
                                      0);

            if (!port) {
                ostringstream os;
                os << "ERROR: Failed to create JACK output port " << m_outputs.size();
                log(os.str());
                return;
            } else {
                jack_latency_range_t range;
                jack_port_get_latency_range(port, JackPlaybackLatency, &range);
                m_source->setSystemPlaybackLatency(range.max);
            }

            if (connectPlayback) {
                if (int(m_outputs.size()) < playPortCount) {
                    jack_connect(m_client,
                                 jack_port_name(port),
                                 playPorts[m_outputs.size()]);
                }
            }

            m_outputs.push_back(port);
        }
    }

    if (m_target) {

        while (int(m_inputs.size()) < channelsRec) {
	
            char name[50];
            jack_port_t *port;

            sprintf(name, "in %ld", long(m_inputs.size() + 1));

            port = jack_port_register(m_client,
                                      name,
                                      JACK_DEFAULT_AUDIO_TYPE,
                                      JackPortIsInput,
                                      0);

            if (!port) {
                ostringstream os;
                os << "ERROR: Failed to create JACK input port " << m_inputs.size();
                log(os.str());
                return;
            } else {
                jack_latency_range_t range;
                jack_port_get_latency_range(port, JackCaptureLatency, &range);
                m_target->setSystemRecordLatency(range.max);
            }

            if (connectRecord) {
                if (int(m_inputs.size()) < capPortCount) {
                    jack_connect(m_client,
                                 capPorts[m_inputs.size()],
                                 jack_port_name(port));
                }
            }

            m_inputs.push_back(port);
        }
    }

    while (int(m_outputs.size()) > channelsPlay) {
	vector<jack_port_t *>::iterator itr = m_outputs.end();
	--itr;
	jack_port_t *port = *itr;
	if (port) jack_port_unregister(m_client, port);
	m_outputs.erase(itr);
    }

    while (int(m_inputs.size()) > channelsRec) {
	vector<jack_port_t *>::iterator itr = m_inputs.end();
	--itr;
	jack_port_t *port = *itr;
	if (port) jack_port_unregister(m_client, port);
	m_inputs.erase(itr);
    }

    if (m_source) {
        m_source->setSystemPlaybackChannelCount(channelsPlay);
    }
    if (m_target) {
        m_target->setSystemRecordChannelCount(channelsRec);
    }
}

int
JACKAudioIO::process(jack_nframes_t j_nframes)
{
    if (!m_mutex.try_lock()) {
	return 0;
    }

    lock_guard<mutex> guard(m_mutex, adopt_lock);

    if (j_nframes > INT_MAX) j_nframes = 0;
    int nframes = int(j_nframes);
    
    if (m_outputs.empty() && m_inputs.empty()) {
	return 0;
    }

#ifdef DEBUG_AUDIO_JACK_IO    
    cout << "JACKAudioIO::process(" << nframes << "): have a purpose in life" << endl;
#endif

#ifdef DEBUG_AUDIO_JACK_IO    
    if (m_bufferSize != nframes) {
	cerr << "WARNING: m_bufferSize != nframes (" << m_bufferSize << " != " << nframes << ")" << endl;
    }
#endif

    float **inbufs = (float **)alloca(m_inputs.size() * sizeof(float *));
    float **outbufs = (float **)alloca(m_outputs.size() * sizeof(float *));

    float peakLeft, peakRight;

    if (m_target) {

        for (int ch = 0; in_range_for(m_inputs, ch); ++ch) {
            inbufs[ch] = (float *)jack_port_get_buffer(m_inputs[ch], nframes);
        }

        peakLeft = 0.0; peakRight = 0.0;

        for (int ch = 0; in_range_for(m_inputs, ch); ++ch) {

            float peak = 0.0;

            for (int i = 0; i < nframes; ++i) {
                float sample = fabsf(inbufs[ch][i]);
                if (sample > peak) peak = sample;
            }

            if (ch == 0) peakLeft = peak;
            if (ch > 0 || m_inputs.size() == 1) peakRight = peak;
        }
        
	m_target->setInputLevels(peakLeft, peakRight);
        m_target->putSamples(inbufs, int(m_inputs.size()), nframes);
    }

    auto gain = Gains::gainsFor(m_outputGain, m_outputBalance, m_outputs.size()); 
    
    if (m_source) {

        for (int ch = 0; in_range_for(m_outputs, ch); ++ch) {
            outbufs[ch] = (float *)jack_port_get_buffer(m_outputs[ch], nframes);
        }
        
	int received = m_source->getSourceSamples
            (outbufs, int(m_outputs.size()), nframes);

        peakLeft = 0.0; peakRight = 0.0;

        for (int ch = 0; in_range_for(m_outputs, ch); ++ch) {

            float peak = 0.0;

            for (int i = received; i < nframes; ++i) {
                outbufs[ch][i] = 0.0;
            }
        
            for (int i = 0; i < nframes; ++i) {
                outbufs[ch][i] *= gain[ch];
                float sample = fabsf(outbufs[ch][i]);
                if (sample > peak) peak = sample;
            }

            if (ch == 0) peakLeft = peak;
            if (ch > 0 || m_outputs.size() == 1) peakRight = peak;
        }
	    
        m_source->setOutputLevels(peakLeft, peakRight);

    } else if (!m_outputs.empty()) {
        
	for (int ch = 0; in_range_for(m_outputs, ch); ++ch) {
	    for (int i = 0; i < nframes; ++i) {
		outbufs[ch][i] = 0.0;
	    }
	}
    }

    return 0;
}

int
JACKAudioIO::xrun()
{
    log("xrun!");
    if (m_target) m_target->audioProcessingOverload();
    if (m_source) m_source->audioProcessingOverload();
    return 0;
}

}

#endif /* HAVE_JACK */

