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

#ifdef HAVE_MEDIAFOUNDATION

#define WINVER 0x0601 // _WIN32_WINNT_WIN7, earliest version to define MF API

#include "MediaFoundationReadStream.h"

#include <windows.h>
#include <mfapi.h>
#include <mfidl.h>
#include <mfreadwrite.h>
#include <mferror.h>
#include <propvarutil.h>
#include <propkey.h>
#include <shobjidl_core.h>
#include <stdio.h>
#include <VersionHelpers.h>

#include <iostream>
#include <algorithm>

using namespace std;

namespace breakfastquay
{

static vector<string>
getMediaFoundationExtensions()
{
    vector<string> extensions;

    extensions.push_back("mp3");
    extensions.push_back("wav");
    extensions.push_back("wma");
    extensions.push_back("avi");
    extensions.push_back("m4a");
    extensions.push_back("m4v");
    extensions.push_back("mov");
    extensions.push_back("mp4");
    extensions.push_back("aac");

    return extensions;
}

static
AudioReadStreamBuilder<MediaFoundationReadStream>
mediafoundationbuilder(
    string("http://breakfastquay.com/rdf/turbot/audiostream/MediaFoundationReadStream"),
    getMediaFoundationExtensions()
    );


static const int maxBufferSize = 1048575;

class MediaFoundationReadStream::D
{
public:
    D(MediaFoundationReadStream *s) :
        refCount(0),
        stream(s),
        channelCount(0),
        bitDepth(0),
        sampleRate(0),
        isFloat(false),
        reader(0),
        mediaType(0),
        mediaBuffer(0),
        mediaBufferIndex(0),
        err(S_OK),
        complete(false)
        { }

    ~D() {
        if (mediaBuffer) {
            mediaBuffer->Release();
        }

        if (reader) {
            reader->Release();
        }

        if (mediaType) {
            mediaType->Release();
        }  
    }

    ULONG APIENTRY AddRef() {
        return ++refCount;
    }
    
    ULONG APIENTRY Release() {
        return --refCount;
    }

    ULONG refCount;

    MediaFoundationReadStream *stream;

    int channelCount;
    int bitDepth;
    int sampleRate;
    bool isFloat;

    IMFSourceReader *reader;
    IMFMediaType *mediaType;
    IMFMediaBuffer *mediaBuffer;
    int mediaBufferIndex;

    HRESULT err;

    bool complete;

    string trackName;
    string artistName;

    int getFrames(int count, float *frames);
    void convertSamples(const unsigned char *in, int inbufsize, float *out);
    float convertSample(const unsigned char *);
    void fillBuffer();
};

static string
wideStringToString(LPWSTR wstr)
{
    if (!wstr) return "";
    int wlen = wcslen(wstr);
    int len = WideCharToMultiByte(CP_UTF8, 0, wstr, wlen, 0, 0, 0, 0);
    if (len < 0) return "";
    char *conv = new char[len + 1];
    (void)WideCharToMultiByte(CP_UTF8, 0, wstr, wlen, conv, len, 0, 0);
    conv[len] = '\0';
    string s = string(conv, len);
    delete[] conv;
    return s;
}

MediaFoundationReadStream::MediaFoundationReadStream(string path) :
    m_path(path),
    m_d(new D(this))
{
    m_channelCount = 0;
    m_sampleRate = 0;

    // References: 
    // http://msdn.microsoft.com/en-gb/library/windows/desktop/dd757929%28v=vs.85%29.aspx

    // Note: CoInitializeEx must already have been called

    IPropertyStore *store = NULL;
    IMFMediaType *partialType = 0;
    int wlen = 0;
    wchar_t *wpath = NULL, *wfullpath = NULL;
    string errorLocation;
    
    m_d->err = MFStartup(MF_VERSION);
    if (FAILED(m_d->err)) {
        m_error = "MediaFoundationReadStream: Failed to initialise MediaFoundation";
        errorLocation = "initialise";
        goto fail;
    }

    wlen = MultiByteToWideChar(CP_UTF8, 0, path.c_str(), int(path.size()), 0, 0);
    if (wlen < 0) {
        m_error = "MediaFoundationReadStream: Failed to convert path to wide chars";
        errorLocation = "convert path";
        goto fail;
    }
    wpath = new wchar_t[wlen+1];
    (void)MultiByteToWideChar(CP_UTF8, 0, path.c_str(), int(path.size()), wpath, wlen);
    wpath[wlen] = L'\0';

    wfullpath = _wfullpath(NULL, wpath, 0);
    
    if (SUCCEEDED(SHGetPropertyStoreFromParsingName
                  (wfullpath, NULL, GPS_BESTEFFORT, IID_PPV_ARGS(&store)))) {
        vector<wchar_t> buf(10000, L'\0'); 
        PROPVARIANT v;
        if (SUCCEEDED(store->GetValue(PKEY_Title, &v)) &&
            SUCCEEDED(PropVariantToString(v, buf.data(), buf.size()-1))) {
            m_d->trackName = wideStringToString(buf.data());
        }
        if (SUCCEEDED(store->GetValue(PKEY_Music_Artist, &v)) &&
            SUCCEEDED(PropVariantToString(v, buf.data(), buf.size()-1))) {
            m_d->artistName = wideStringToString(buf.data());
        }
        store->Release();
    }

    m_d->err = MFCreateSourceReaderFromURL(wfullpath, NULL, &m_d->reader);

    delete[] wpath;
    wpath = NULL;

    free(wfullpath);
    wfullpath = NULL;
    
    if (FAILED(m_d->err)) {
        m_error = "MediaFoundationReadStream: Failed to create source reader";
        errorLocation = "create source reader";
        goto fail;
    }

    m_d->err = m_d->reader->SetStreamSelection
        ((DWORD)MF_SOURCE_READER_ALL_STREAMS, FALSE);
    if (SUCCEEDED(m_d->err)) {
        m_d->err = m_d->reader->SetStreamSelection
            ((DWORD)MF_SOURCE_READER_FIRST_AUDIO_STREAM, TRUE);
    }
    if (FAILED(m_d->err)) {
        m_error = "MediaFoundationReadStream: Failed to select first audio stream";
        errorLocation = "select stream";
        goto fail;
    }

    // Create a partial media type that specifies uncompressed PCM audio

    m_d->err = MFCreateMediaType(&partialType);
    if (SUCCEEDED(m_d->err)) {
        m_d->err = partialType->SetGUID(MF_MT_MAJOR_TYPE, MFMediaType_Audio);
    }
    if (SUCCEEDED(m_d->err)) {
        m_d->err = partialType->SetGUID(MF_MT_SUBTYPE, MFAudioFormat_PCM);
    }
    if (SUCCEEDED(m_d->err)) {
        m_d->err = m_d->reader->SetCurrentMediaType
            ((DWORD)MF_SOURCE_READER_FIRST_AUDIO_STREAM, NULL, partialType);
    }
    if (SUCCEEDED(m_d->err)) {
        m_d->err = m_d->reader->GetCurrentMediaType
            ((DWORD)MF_SOURCE_READER_FIRST_AUDIO_STREAM, &m_d->mediaType);
    }
    if (SUCCEEDED(m_d->err)) {
        // surely this is redundant, as we did it already? but they do
        // it twice in the MS example... well, presumably no harm anyway
        m_d->err = m_d->reader->SetStreamSelection
            ((DWORD)MF_SOURCE_READER_FIRST_AUDIO_STREAM, TRUE);
    }
    if (partialType) {
        partialType->Release();
        partialType = 0;
    }

    if (SUCCEEDED(m_d->err)) {
        UINT32 depth;
        m_d->err = m_d->mediaType->GetUINT32
            (MF_MT_AUDIO_BITS_PER_SAMPLE, &depth);
        m_d->bitDepth = depth;
    }

    if (SUCCEEDED(m_d->err)) {
        UINT32 rate;
        m_d->err = m_d->mediaType->GetUINT32
            (MF_MT_AUDIO_SAMPLES_PER_SECOND, &rate);
        m_sampleRate = m_d->sampleRate = rate;
    }

    if (SUCCEEDED(m_d->err)) {
        UINT32 chans;
        m_d->err = m_d->mediaType->GetUINT32
            (MF_MT_AUDIO_NUM_CHANNELS, &chans);
        m_channelCount = m_d->channelCount = chans;
    }

    if (FAILED(m_d->err)) {
        m_error = "MediaFoundationReadStream: File format could not be converted to PCM stream";
        errorLocation = "media type selection";
        goto fail;
    }

    return;

fail:
    delete m_d;
    MFShutdown();
    throw FileOperationFailed(m_path, string("MediaFoundation ") + errorLocation);
}

MediaFoundationReadStream::~MediaFoundationReadStream()
{
    delete m_d;
    MFShutdown(); // "Call this function once for every call to MFStartup"
                  // - i.e. they are allowed to nest
}

size_t
MediaFoundationReadStream::getFrames(size_t count, float *frames)
{
    return m_d->getFrames(int(count), frames);
}

string
MediaFoundationReadStream::getTrackName() const
{
    return m_d->trackName;
}

string
MediaFoundationReadStream::getArtistName() const
{
    return m_d->artistName;
}

int
MediaFoundationReadStream::D::getFrames(int framesRequired, float *frames)
{
    if (!mediaBuffer) {
        fillBuffer();
        if (!mediaBuffer) {
            // legitimate end of file
            return 0;
        }
    }
        
    BYTE *data = 0;
    DWORD length = 0;

    err = mediaBuffer->Lock(&data, 0, &length);

    if (FAILED(err)) {
        stream->m_error = "MediaFoundationReadStream: Failed to lock media buffer?!";
        throw FileOperationFailed(stream->m_path, "Read from audio file");
    }

    int bytesPerFrame = channelCount * (bitDepth / 8);
    int framesAvailable = (length - mediaBufferIndex) / bytesPerFrame;
    int framesToGet = std::min(framesRequired, framesAvailable);

    if (framesToGet > 0) {
        // have something in the buffer, not necessarily all we need
        convertSamples(data + mediaBufferIndex,
                       framesToGet * bytesPerFrame,
                       frames);
        mediaBufferIndex += framesToGet * bytesPerFrame;
    }

    mediaBuffer->Unlock();
    
    if (framesToGet == framesRequired) {
        // we got enough! rah
        return framesToGet;
    }
    
    // otherwise, we ran out: this buffer has nothing left, release it
    // and call again for the next part

    mediaBuffer->Release();
    mediaBuffer = 0;
    mediaBufferIndex = 0;
    
    return framesToGet +
        getFrames(framesRequired - framesToGet, 
                  frames + framesToGet * channelCount);
}

void
MediaFoundationReadStream::D::fillBuffer()
{
    // assumes mediaBuffer is currently null
    
    DWORD flags = 0;
    IMFSample *sample = 0;

    while (!sample) {

        // "In some cases a call to ReadSample does not generate data,
        // in which case the IMFSample pointer is NULL" (hence the loop)

        err = reader->ReadSample((DWORD)MF_SOURCE_READER_FIRST_AUDIO_STREAM,
                                 0, 0, &flags, 0, &sample);

        if (FAILED(err)) {
            stream->m_error = "MediaFoundationReadStream: Failed to read sample from stream";
            throw FileOperationFailed(stream->m_path, "Read from audio file");
        }

        if (flags & MF_SOURCE_READERF_ENDOFSTREAM) {
            return;
        }
    }

    err = sample->ConvertToContiguousBuffer(&mediaBuffer);
    if (FAILED(err)) {
        stream->m_error = "MediaFoundationReadStream: Failed to convert sample to buffer";
        throw FileOperationFailed(stream->m_path, "Read from audio file");
    }
}

void
MediaFoundationReadStream::D::convertSamples(const unsigned char *inbuf,
                                             int inbufbytes,
                                             float *out)
{
    int inix = 0;
    int bytesPerSample = bitDepth / 8;
    while (inix < inbufbytes) {
        *out = convertSample(inbuf + inix);
        out += 1;
        inix += bytesPerSample;
    }
}

float
MediaFoundationReadStream::D::convertSample(const unsigned char *c)
{
    if (isFloat) {
        return *(float *)c;
    }

    switch (bitDepth) {

    case 8: {
            // WAV stores 8-bit samples unsigned, other sizes signed.
            return (float)(c[0] - 128.0) / 128.0;
        }

    case 16: {
            // Two's complement little-endian 16-bit integer.
            // We convert endianness (if necessary) but assume 16-bit short.
            unsigned char b2 = c[0];
            unsigned char b1 = c[1];
            unsigned int bits = (b1 << 8) + b2;
            return float(double(short(bits)) / 32768.0);
        }

    case 24: {
            // Two's complement little-endian 24-bit integer.
            // Again, convert endianness but assume 32-bit int.
            unsigned char b3 = c[0];
            unsigned char b2 = c[1];
            unsigned char b1 = c[2];
            // Rotate 8 bits too far in order to get the sign bit
            // in the right place; this gives us a 32-bit value,
            // hence the larger float divisor
            unsigned int bits = (b1 << 24) + (b2 << 16) + (b3 << 8);
            return float(double(int(bits)) / 2147483648.0);
        }

    default:
        return 0.0f;
    }
}

}

#endif

