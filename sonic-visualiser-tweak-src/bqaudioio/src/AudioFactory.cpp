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

#include "AudioFactory.h"

#include "JACKAudioIO.h"
#include "PortAudioIO.h"
#include "PulseAudioIO.h"

#include "Log.h"

#include <iostream>

using std::string;
using std::vector;

namespace breakfastquay {

void
AudioFactory::setLogCallback(LogCallback *callback)
{
    Log::setLogCallback(callback);
}

vector<string>
AudioFactory::getImplementationNames()
{
    vector<string> names;
    
#ifdef HAVE_JACK
    names.push_back("jack");
#endif

#ifdef HAVE_LIBPULSE
    names.push_back("pulse");
#endif

#ifdef HAVE_PORTAUDIO
    names.push_back("port");
#endif

    return names;
}

string
AudioFactory::getImplementationDescription(string implementationName)
{
    if (implementationName == "") {
        return "(auto)";
    }
    if (implementationName == "jack") {
        return "JACK Audio Connection Kit";
    }
    if (implementationName == "pulse") {
        return "PulseAudio Server";
    }
    if (implementationName == "port") {
        return "PortAudio Driver";
    }
    return "(unknown)";
}

vector<string>
AudioFactory::getRecordDeviceNames(string implementationName)
{
    if (implementationName == "") {
        // Can't offer implementation-specific choices as we don't
        // know which implementation will end up being used
        return { };
    }
    
#ifdef HAVE_JACK
    if (implementationName == "jack") {
        return JACKAudioIO::getRecordDeviceNames();
    }
#endif

#ifdef HAVE_LIBPULSE
    if (implementationName == "pulse") {
        return PulseAudioIO::getRecordDeviceNames();
    }
#endif

#ifdef HAVE_PORTAUDIO
    if (implementationName == "port") {
        return PortAudioIO::getRecordDeviceNames();
    }
#endif

    return {};
}

vector<string>
AudioFactory::getPlaybackDeviceNames(string implementationName)
{
    if (implementationName == "") {
        // Can't offer implementation-specific choices as we don't
        // know which implementation will end up being used
        return { };
    }

#ifdef HAVE_JACK
    if (implementationName == "jack") {
        return JACKAudioIO::getPlaybackDeviceNames();
    }
#endif

#ifdef HAVE_LIBPULSE
    if (implementationName == "pulse") {
        return PulseAudioIO::getPlaybackDeviceNames();
    }
#endif

#ifdef HAVE_PORTAUDIO
    if (implementationName == "port") {
        return PortAudioIO::getPlaybackDeviceNames();
    }
#endif

    return {};
}

static SystemAudioIO *
createIO(Mode mode,
         ApplicationRecordTarget *target,
         ApplicationPlaybackSource *source,
         AudioFactory::Preference preference,
         std::string &errorString)
{
    string startupError;
    int implementationsTried = 0;
    
#ifdef HAVE_JACK
    if (preference.implementation == "" || preference.implementation == "jack") {
        ++implementationsTried;
        JACKAudioIO *io = new JACKAudioIO(mode, target, source,
                                          preference.recordDevice,
                                          preference.playbackDevice);
        if (io->isOK()) return io;
        else {
            std::cerr << "WARNING: AudioFactory::createCallbackIO: Failed to open JACK I/O" << std::endl;
            startupError = io->getStartupErrorString();
            delete io;
        }
    }
#endif

#ifdef HAVE_LIBPULSE
    if (preference.implementation == "" || preference.implementation == "pulse") {
        ++implementationsTried;
        PulseAudioIO *io = new PulseAudioIO(mode, target, source,
                                            preference.recordDevice,
                                            preference.playbackDevice);
        if (io->isOK()) return io;
        else {
            std::cerr << "WARNING: AudioFactory::createCallbackIO: Failed to open PulseAudio I/O" << std::endl;
            startupError = io->getStartupErrorString();
            delete io;
        }
    }
#endif

#ifdef HAVE_PORTAUDIO
    if (preference.implementation == "" || preference.implementation == "port") {
        ++implementationsTried;
        PortAudioIO *io = new PortAudioIO(mode, target, source,
                                          preference.recordDevice,
                                          preference.playbackDevice);
        if (io->isOK()) return io;
        else {
            std::cerr << "WARNING: AudioFactory::createCallbackIO: Failed to open PortAudio I/O" << std::endl;
            startupError = io->getStartupErrorString();
            delete io;
        }
    }
#endif

    if (implementationsTried == 0) {
        if (preference.implementation == "") {
            errorString = "No audio drivers compiled in";
        } else {
            errorString = "Requested audio driver is not compiled in";
        }
    } else if (implementationsTried == 1) {
        errorString = startupError;
    }
    
    std::cerr << "WARNING: AudioFactory::createIO: No suitable implementation available" << std::endl;
    return nullptr;
}

SystemPlaybackTarget *
AudioFactory::createCallbackPlayTarget(ApplicationPlaybackSource *source,
                                       Preference preference,
                                       std::string &errorString)
{
    if (!source) {
        throw std::logic_error("ApplicationPlaybackSource must be provided");
    }

    return createIO(Mode::Playback, 0, source, preference, errorString);
}

SystemRecordSource *
AudioFactory::createCallbackRecordSource(ApplicationRecordTarget *target,
                                         Preference preference,
                                         std::string &errorString)
{
    if (!target) {
        throw std::logic_error("ApplicationRecordTarget must be provided");
    }

    return createIO(Mode::Record, target, 0, preference, errorString);
}

SystemAudioIO *
AudioFactory::createCallbackIO(ApplicationRecordTarget *target,
                               ApplicationPlaybackSource *source,
                               Preference preference,
                               std::string &errorString)
{
    if (!target || !source) {
        throw std::logic_error("ApplicationRecordTarget and ApplicationPlaybackSource must both be provided");
    }

    return createIO(Mode::Duplex, target, source, preference, errorString);
}

}
