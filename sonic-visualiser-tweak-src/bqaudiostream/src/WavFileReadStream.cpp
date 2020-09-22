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

#include "WavFileReadStream.h"
#include "Exceptions.h"

#include <iostream>

using namespace std;

namespace breakfastquay
{

static vector<string>
getWavReaderExtensions()
{
    vector<string> extensions;
    int count;
    
    if (sf_command(0, SFC_GET_FORMAT_MAJOR_COUNT, &count, sizeof(count))) {
        extensions.push_back("wav");
        extensions.push_back("aiff");
        extensions.push_back("aifc");
        extensions.push_back("aif");
        return extensions;
    }

    SF_FORMAT_INFO info;
    for (int i = 0; i < count; ++i) {
        info.format = i;
        if (!sf_command(0, SFC_GET_FORMAT_MAJOR, &info, sizeof(info))) {
            extensions.push_back(string(info.extension));
        }
    }

    return extensions;
}

static
AudioReadStreamBuilder<WavFileReadStream>
wavbuilder(
    string("http://breakfastquay.com/rdf/turbot/audiostream/WavFileReadStream"),
    getWavReaderExtensions()
    );

WavFileReadStream::WavFileReadStream(string path) :
    m_file(0),
    m_path(path),
    m_offset(0)
{
    m_channelCount = 0;
    m_sampleRate = 0;

    m_fileInfo.format = 0;
    m_fileInfo.frames = 0;

#ifdef _WIN32
    int wlen = MultiByteToWideChar
        (CP_UTF8, 0, m_path.c_str(), m_path.length(), 0, 0);
    if (wlen > 0) {
        wchar_t *buf = new wchar_t[wlen+1];
        (void)MultiByteToWideChar
            (CP_UTF8, 0, m_path.c_str(), m_path.length(), buf, wlen);
        buf[wlen] = L'\0';
        m_file = sf_wchar_open(buf, SFM_READ, &m_fileInfo);
        delete[] buf;
    }
#else
    m_file = sf_open(m_path.c_str(), SFM_READ, &m_fileInfo);
#endif

    if (!m_file || m_fileInfo.frames <= 0 || m_fileInfo.channels <= 0) {
//	cerr << "WavFileReadStream::initialize: Failed to open file \""
//                  << path << "\" (" << sf_strerror(m_file) << ")" << endl;
        if (sf_error(m_file) == SF_ERR_SYSTEM) {
	    m_error = string("Couldn't load audio file '") +
                m_path + "':\n" + sf_strerror(m_file);
            throw FileNotFound(m_path);
        }
	if (m_file) {
	    m_error = string("Couldn't load audio file '") +
                m_path + "':\n" + sf_strerror(m_file);
	} else {
	    m_error = string("Failed to open audio file '") +
		m_path + "'";
	}
        throw InvalidFileFormat(m_path, m_error);
    }

    m_channelCount = m_fileInfo.channels;
    m_sampleRate = m_fileInfo.samplerate;

    const char *str = sf_get_string(m_file, SF_STR_TITLE);
    if (str) {
        m_track = str;
    }
    str = sf_get_string(m_file, SF_STR_ARTIST);
    if (str) {
        m_artist = str;
    }
    
    sf_seek(m_file, 0, SEEK_SET);
}

WavFileReadStream::~WavFileReadStream()
{
    if (m_file) sf_close(m_file);
}

size_t
WavFileReadStream::getFrames(size_t count, float *frames)
{
    if (!m_file || !m_channelCount) return 0;
    if (count == 0) return 0;

    if ((long)m_offset >= m_fileInfo.frames) {
	return 0;
    }

    sf_count_t readCount = sf_readf_float(m_file, frames, count);
    
    if (readCount < 0) {
        return 0;
    }

    m_offset = m_offset + readCount;

    return readCount;
}

}

#endif
