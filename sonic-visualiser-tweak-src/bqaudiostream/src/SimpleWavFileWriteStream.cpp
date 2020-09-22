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

#include "SimpleWavFileWriteStream.h"

#if ! (defined(HAVE_LIBSNDFILE) || defined(HAVE_SNDFILE))

#include "Exceptions.h"
#include <iostream>

using namespace std;

namespace breakfastquay
{

static vector<string> extensions() {
    vector<string> ee;
    ee.push_back("wav");
    return ee;
}

static 
AudioWriteStreamBuilder<SimpleWavFileWriteStream>
simplewavbuilder(
    string("http://breakfastquay.com/rdf/turbot/audiostream/SimpleWavFileWriteStream"),
    extensions()
    );

SimpleWavFileWriteStream::SimpleWavFileWriteStream(Target target) :
    AudioWriteStream(target),
    m_bitDepth(24),
    m_file(0)
{
    m_file = new ofstream(getPath().c_str(), ios::out | std::ios::binary);

    if (!*m_file) {
        delete m_file;
        m_file = 0;
        cerr << "SimpleWavFileWriteStream: Failed to open output file for writing" << endl;
        m_error = string("Failed to open audio file '") +
            getPath() + "' for writing";
        throw FailedToWriteFile(getPath());
    }

    writeFormatChunk();
}

SimpleWavFileWriteStream::~SimpleWavFileWriteStream()
{
    if (!m_file) {
        return;
    }

    m_file->seekp(0, ios::end);
    unsigned int totalSize = m_file->tellp();

    // seek to first length position
    m_file->seekp(4, ios::beg);

    // write complete file size minus 8 bytes to here
    putBytes(int2le(totalSize - 8, 4));

    // reseek from start forward 40
    m_file->seekp(40, ios::beg);

    // write the data chunk size to end
    putBytes(int2le(totalSize - 44, 4));

    m_file->close();

    delete m_file;
    m_file = 0;
}

void
SimpleWavFileWriteStream::putBytes(string s)
{
    if (!m_file) return;
    for (unsigned int i = 0; i < s.length(); i++) {
        *m_file << (unsigned char)s[i];
    }
}

void
SimpleWavFileWriteStream::putBytes(const unsigned char *buffer, size_t n)
{
    if (!m_file) return;
    m_file->write((const char *)buffer, n);
}

string
SimpleWavFileWriteStream::int2le(unsigned int value, unsigned int length)
{
    string r;

    do {
        r += (unsigned char)((long)((value >> (8 * r.length())) & 0xff));
    } while (r.length() < length);

    return r;
}

void
SimpleWavFileWriteStream::writeFormatChunk()
{
    if (!m_file) return;

    string outString;

    outString += "RIFF";
    outString += "0000";
    outString += "WAVE";
    outString += "fmt ";

    // length
    outString += int2le(0x10, 4);

    // 1 for PCM, 3 for float
    outString += int2le(0x01, 2);

    // channels
    outString += int2le(getChannelCount(), 2);

    // sample rate
    outString += int2le(getSampleRate(), 4);

    // bytes per second
    outString += int2le((m_bitDepth / 8) * getChannelCount() * getSampleRate(), 4);

    // bytes per frame
    outString += int2le((m_bitDepth / 8) * getChannelCount(), 2);

    // bits per sample
    outString += int2le(m_bitDepth, 2);

    outString += "data";
    outString += "0000";

    putBytes(outString);
}

void
SimpleWavFileWriteStream::putInterleavedFrames(size_t count, float *frames)
{
    if (count == 0) return;

    for (size_t i = 0; i < count; ++i) {
        for (size_t c = 0; c < getChannelCount(); ++c) {
            
            double f = frames[i * getChannelCount() + c];
            unsigned int u = 0;
            unsigned char ubuf[4];
            if (f < -1.0) f = -1.0;
            if (f > 1.0) f = 1.0;
            
            switch (m_bitDepth) {

            case 24:
                f = f * 2147483647.0;
                u = (unsigned int)(int(f));
                u >>= 8;
                ubuf[0] = (u & 0xff);
                u >>= 8;
                ubuf[1] = (u & 0xff);
                u >>= 8;
                ubuf[2] = (u & 0xff);
                break;

            default:
                ubuf[0] = ubuf[1] = ubuf[2] = ubuf[3] = '\0';
                break;
            }

            putBytes(ubuf, m_bitDepth / 8);
        }
    }
}

}

#endif
