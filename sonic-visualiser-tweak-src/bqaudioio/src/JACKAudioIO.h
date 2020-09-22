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

#ifndef BQAUDIOIO_JACK_IO_H_
#define BQAUDIOIO_JACK_IO_H_

#ifdef HAVE_JACK

#include <jack/jack.h>
#include <vector>
#include <mutex>

#include "SystemAudioIO.h"
#include "AudioFactory.h"
#include "Mode.h"

namespace breakfastquay {

class ApplicationRecordTarget;
class ApplicationPlaybackSource;

class JACKAudioIO : public SystemAudioIO
{
public:
    JACKAudioIO(Mode mode,
                ApplicationRecordTarget *recordTarget,
		ApplicationPlaybackSource *playSource,
                std::string recordDevice,
                std::string playbackDevice);
    virtual ~JACKAudioIO();

    static std::vector<std::string> getRecordDeviceNames();
    static std::vector<std::string> getPlaybackDeviceNames();
    
    bool isSourceOK() const override;
    bool isTargetOK() const override;

    void suspend() override {}
    void resume() override {}
    
    double getCurrentTime() const override;

    std::string getStartupErrorString() const { return m_startupError; }
    
protected:
    void setup(bool connectRecord, bool connectPlayback);
    int process(jack_nframes_t nframes);
    int xrun();

    static int processStatic(jack_nframes_t, void *);
    static int xrunStatic(void *);

    Mode                        m_mode;
    jack_client_t              *m_client;
    std::vector<jack_port_t *>  m_outputs;
    std::vector<jack_port_t *>  m_inputs;
    jack_nframes_t              m_bufferSize;
    jack_nframes_t              m_sampleRate;
    std::mutex                  m_mutex;
    std::string                 m_startupError;

    JACKAudioIO(const JACKAudioIO &)=delete;
    JACKAudioIO &operator=(const JACKAudioIO &)=delete;
};

}

#endif /* HAVE_JACK */

#endif

