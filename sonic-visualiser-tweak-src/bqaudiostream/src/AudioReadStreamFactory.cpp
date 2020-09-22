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

#include "AudioReadStreamFactory.h"
#include "AudioReadStream.h"
#include "Exceptions.h"

#include <bqthingfactory/ThingFactory.h>

#define DEBUG_AUDIO_READ_STREAM_FACTORY 1

using namespace std;

namespace breakfastquay {

typedef ThingFactory<AudioReadStream, string>
AudioReadStreamFactoryImpl;

string
AudioReadStreamFactory::extensionOf(string audioFileName)
{
    string::size_type pos = audioFileName.rfind('.');
    if (pos == string::npos) return "";
    string ext;
    for (string::size_type i = pos + 1; i < audioFileName.size(); ++i) {
        ext += (char)tolower((unsigned char)audioFileName[i]);
    }
    return ext;
}

AudioReadStream *
AudioReadStreamFactory::createReadStream(string audioFileName)
{
    string extension = extensionOf(audioFileName);

    AudioReadStreamFactoryImpl *f = AudioReadStreamFactoryImpl::getInstance();

    // Earlier versions of this code would first try to use a reader
    // that had actually registered an interest in this extension,
    // then fall back (if that failed) to trying every reader in
    // order. But we rely on extensions so much anyway, it's probably
    // more predictable always to use only the reader that has
    // registered the extension (if there is one).

    try {
        AudioReadStream *stream = f->createFor(extension, audioFileName);
        if (!stream) throw UnknownFileType(audioFileName);
        return stream;
    } catch (const UnknownTagException &) {
        throw UnknownFileType(audioFileName);
    }
}

vector<string>
AudioReadStreamFactory::getSupportedFileExtensions()
{
    return AudioReadStreamFactoryImpl::getInstance()->getTags();
}

bool
AudioReadStreamFactory::isExtensionSupportedFor(string fileName)
{
    vector<string> supported = getSupportedFileExtensions();
    set<string> sset(supported.begin(), supported.end());
    return sset.find(extensionOf(fileName)) != sset.end();
}

string
AudioReadStreamFactory::getFileFilter()
{
    vector<string> extensions = getSupportedFileExtensions();
    string filter;
    for (size_t i = 0; i < extensions.size(); ++i) {
        string ext = extensions[i];
        if (filter != "") filter += " ";
        filter += "*." + ext;
    }
    return filter;
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

#include "WavFileReadStream.cpp"
#include "OggVorbisReadStream.cpp"
#include "MediaFoundationReadStream.cpp"
#include "CoreAudioReadStream.cpp"
#include "OpusReadStream.cpp"

