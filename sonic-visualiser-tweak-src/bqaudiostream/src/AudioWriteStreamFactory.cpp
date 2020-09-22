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

#include "AudioWriteStreamFactory.h"
#include "AudioWriteStream.h"

#include "bqthingfactory/ThingFactory.h"
#include "Exceptions.h"

#include "AudioReadStreamFactory.h"

using namespace std;

namespace breakfastquay {

typedef ThingFactory<AudioWriteStream, AudioWriteStream::Target>
AudioWriteStreamFactoryImpl;

//template <>
//AudioWriteStreamFactoryImpl *
//AudioWriteStreamFactoryImpl::m_instance = 0;

AudioWriteStream *
AudioWriteStreamFactory::createWriteStream(string audioFileName,
                                           size_t channelCount,
                                           size_t sampleRate)
{
    AudioWriteStream *s = 0;

    string extension = AudioReadStreamFactory::extensionOf(audioFileName);
    
    AudioWriteStream::Target target(audioFileName, channelCount, sampleRate);

    AudioWriteStreamFactoryImpl *f = AudioWriteStreamFactoryImpl::getInstance();

    try {
        AudioWriteStream *stream = f->createFor(extension, target);
        if (!stream) throw UnknownFileType(audioFileName);
        return stream;
    } catch (UnknownTagException) {
        throw UnknownFileType(audioFileName);
    }
}

vector<string>
AudioWriteStreamFactory::getSupportedFileExtensions()
{
    return AudioWriteStreamFactoryImpl::getInstance()->getTags();
}

string
AudioWriteStreamFactory::getDefaultUncompressedFileExtension()
{
    vector<string> candidates;
    candidates.push_back("wav");
    candidates.push_back("aiff");
    vector<string> supported = getSupportedFileExtensions();
    set<string> sset(supported.begin(), supported.end());
    for (size_t i = 0; i < candidates.size(); ++i) {
        if (sset.find(candidates[i]) != sset.end()) {
            return candidates[i];
        }
    }
    return "";
}

string
AudioWriteStreamFactory::getDefaultLossyFileExtension()
{
    vector<string> candidates;
    candidates.push_back("mp3");
    candidates.push_back("m4a");
    candidates.push_back("ogg");
    candidates.push_back("oga");
    vector<string> supported = getSupportedFileExtensions();
    set<string> sset(supported.begin(), supported.end());
    for (size_t i = 0; i < candidates.size(); ++i) {
        if (sset.find(candidates[i]) != sset.end()) {
            return candidates[i];
        }
    }
    return "";
}

bool
AudioWriteStreamFactory::isExtensionSupportedFor(string fileName)
{
    vector<string> supported = getSupportedFileExtensions();
    set<string> sset(supported.begin(), supported.end());
    string ext = AudioReadStreamFactory::extensionOf(fileName);
    return sset.find(ext) != sset.end();
}

}

// We rather eccentrically include the C++ files here, not the
// headers.  This file actually doesn't need any includes in order to
// compile, but we are building it into a static archive, from which
// only those object files that are referenced in the code that uses
// the archive will be extracted for linkage.  Since no code refers
// directly to the stream implementations (they are self-registering),
// this means they will not be linked in.  So we include them directly
// into this object file instead, and it's not necessary to build them
// separately in the project. In each case the code is completely
// #ifdef'd out if the implementation is not selected, so there is no
// overhead.

#include "WavFileWriteStream.cpp"
#include "SimpleWavFileWriteStream.cpp"
#include "CoreAudioWriteStream.cpp"

