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

#if defined(HAVE_LIBSNDFILE) || defined(HAVE_SNDFILE)

#include "WavFileWriteStream.h"
#include "Exceptions.h"

#include <cstring>

using namespace std;

namespace breakfastquay
{

static vector<string> extensions() {
    vector<string> ee;
    ee.push_back("wav");
    ee.push_back("aiff");
    return ee;
}

static 
AudioWriteStreamBuilder<WavFileWriteStream>
wavbuilder(
    string("http://breakfastquay.com/rdf/turbot/audiostream/WavFileWriteStream"),
    extensions()
    );

WavFileWriteStream::WavFileWriteStream(Target target) :
    AudioWriteStream(target),
    m_file(0)
{
    memset(&m_fileInfo, 0, sizeof(SF_INFO));
    m_fileInfo.format = SF_FORMAT_WAV | SF_FORMAT_FLOAT;
    m_fileInfo.channels = getChannelCount();
    m_fileInfo.samplerate = getSampleRate();

    m_file = sf_open(getPath().c_str(), SFM_WRITE, &m_fileInfo);

    if (!m_file) {
	cerr << "WavFileWriteStream::initialize: Failed to open output file for writing ("
		  << sf_strerror(m_file) << ")" << endl;

        m_error = string("Failed to open audio file '") +
            getPath() + "' for writing";
        throw FailedToWriteFile(getPath());
    }
}

WavFileWriteStream::~WavFileWriteStream()
{
    if (m_file) sf_close(m_file);
}

void
WavFileWriteStream::putInterleavedFrames(size_t count, float *frames)
{
    if (count == 0) return;

    sf_count_t written = sf_writef_float(m_file, frames, count);

    if (written != count) {
        throw FileOperationFailed(getPath(), "write sf data");
    }
}

}

#endif
