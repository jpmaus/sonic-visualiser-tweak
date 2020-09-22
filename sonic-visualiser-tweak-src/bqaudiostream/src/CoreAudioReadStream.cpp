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

#ifdef HAVE_COREAUDIO

// OS/X system headers don't cope with DEBUG
#ifdef DEBUG
#undef DEBUG
#endif

#if !defined(__COREAUDIO_USE_FLAT_INCLUDES__)
#include <AudioToolbox/AudioToolbox.h>
#include <AudioToolbox/ExtendedAudioFile.h>
#else
#include "AudioToolbox.h"
#include "ExtendedAudioFile.h"
#endif

#include "CoreAudioReadStream.h"

#include <sstream>

namespace breakfastquay
{

static vector<string>
getCoreAudioExtensions()
{
    vector<string> extensions;
    extensions.push_back("aiff");
    extensions.push_back("aif");
    extensions.push_back("au");
    extensions.push_back("avi");
    extensions.push_back("m4a");
    extensions.push_back("m4b");
    extensions.push_back("m4p");
    extensions.push_back("m4v");
    extensions.push_back("mov");
    extensions.push_back("mp3");
    extensions.push_back("mp4");
    extensions.push_back("wav");
    return extensions;
}

static
AudioReadStreamBuilder<CoreAudioReadStream>
coreaudiobuilder(
    string("http://breakfastquay.com/rdf/turbot/audiostream/CoreAudioReadStream"),
    getCoreAudioExtensions()
    );

class CoreAudioReadStream::D
{
public:
    D() { }

    ExtAudioFileRef              file;
    AudioBufferList              buffer;
    OSStatus                     err; 
    AudioStreamBasicDescription  asbd;
};

static string
codestr(OSStatus err)
{
    char text[5];
    UInt32 uerr = err;
    text[0] = (uerr >> 24) & 0xff;
    text[1] = (uerr >> 16) & 0xff;
    text[2] = (uerr >> 8) & 0xff;
    text[3] = (uerr) & 0xff;
    text[4] = '\0';
    ostringstream os;
    os << err << " (" << text << ")";
    return os.str();
}

CoreAudioReadStream::CoreAudioReadStream(string path) :
    m_path(path),
    m_d(new D)
{
    m_channelCount = 0;
    m_sampleRate = 0;

    CFURLRef url = CFURLCreateFromFileSystemRepresentation
        (kCFAllocatorDefault,
         (const UInt8 *)path.c_str(),
         (CFIndex)path.size(),
         false);

    UInt32 propsize;
    OSStatus noncritical;

    m_d->err = ExtAudioFileOpenURL(url, &m_d->file);

    CFRelease(url);

    if (m_d->err == kAudio_FileNotFoundError) {
        throw FileNotFound(m_path);
    }

    if (m_d->err) { 
        m_error = "CoreAudioReadStream: Error opening file: code " + codestr(m_d->err);
        throw InvalidFileFormat(path, "failed to open audio file");
    }
    if (!m_d->file) { 
        m_error = "CoreAudioReadStream: Failed to open file, but no error reported!";
        throw InvalidFileFormat(path, "failed to open audio file");
    }

    // Retrieve metadata through the underlying AudioFile API if possible

    AudioFileID audioFile = 0;
    propsize = sizeof(AudioFileID);
    noncritical = ExtAudioFileGetProperty
        (m_d->file, kExtAudioFileProperty_AudioFile, &propsize, &audioFile);

    if (noncritical == noErr) {

        CFDictionaryRef dict = nil;
        UInt32 dataSize = sizeof(dict);
        noncritical = AudioFileGetProperty
            (audioFile, kAudioFilePropertyInfoDictionary, &dataSize, &dict);

        if (noncritical == noErr) {

            CFIndex count = CFDictionaryGetCount(dict);
            const void **kk = new const void *[count];
            const void **vv = new const void *[count];
            CFDictionaryGetKeysAndValues(dict, kk, vv);

            int bufsize = 10240;
            char *buffer = new char[bufsize];

            for (int i = 0; i < count; ++i) {
                if (CFGetTypeID(kk[i]) == CFStringGetTypeID() &&
                    CFGetTypeID(vv[i]) == CFStringGetTypeID()) {
                    CFStringRef key = reinterpret_cast<CFStringRef>(kk[i]);
                    CFStringRef value = reinterpret_cast<CFStringRef>(vv[i]);
                    if (CFStringGetCString(key, buffer, bufsize,
                                           kCFStringEncodingUTF8)) {
                        string kstr = buffer;
                        if (CFStringGetCString(value, buffer, bufsize,
                                               kCFStringEncodingUTF8)) {
                            if (kstr == kAFInfoDictionary_Title) {
                                m_track = buffer;
                            } else if (kstr == kAFInfoDictionary_Artist) {
                                m_artist = buffer;
                            }
                        }
                    }
                }
            }

            delete[] buffer;
            delete[] kk;
            delete[] vv;

            CFRelease(dict);
        }
    }

    propsize = sizeof(AudioStreamBasicDescription);
    m_d->err = ExtAudioFileGetProperty
	(m_d->file, kExtAudioFileProperty_FileDataFormat, &propsize, &m_d->asbd);
    
    if (m_d->err) {
        m_error = "CoreAudioReadStream: Error in getting basic description: code " + codestr(m_d->err);
        ExtAudioFileDispose(m_d->file);
        throw FileOperationFailed(m_path, "get basic description", codestr(m_d->err));
    }
	
    m_channelCount = m_d->asbd.mChannelsPerFrame;
    m_sampleRate = m_d->asbd.mSampleRate;

    m_d->asbd.mSampleRate = getSampleRate();
    m_d->asbd.mFormatID = kAudioFormatLinearPCM;
    m_d->asbd.mFormatFlags =
        kAudioFormatFlagIsFloat |
        kAudioFormatFlagIsPacked |
        kAudioFormatFlagsNativeEndian;
    m_d->asbd.mBitsPerChannel = sizeof(float) * 8;
    m_d->asbd.mBytesPerFrame = sizeof(float) * m_channelCount;
    m_d->asbd.mBytesPerPacket = sizeof(float) * m_channelCount;
    m_d->asbd.mFramesPerPacket = 1;
    m_d->asbd.mReserved = 0;
	
    m_d->err = ExtAudioFileSetProperty
	(m_d->file, kExtAudioFileProperty_ClientDataFormat, propsize, &m_d->asbd);
    
    if (m_d->err) {
        m_error = "CoreAudioReadStream: Error in setting client format: code " + codestr(m_d->err);
        throw FileOperationFailed(m_path, "set client format", codestr(m_d->err));
    }

    m_d->buffer.mNumberBuffers = 1;
    m_d->buffer.mBuffers[0].mNumberChannels = m_channelCount;
    m_d->buffer.mBuffers[0].mDataByteSize = 0;
    m_d->buffer.mBuffers[0].mData = 0;
}

size_t
CoreAudioReadStream::getFrames(size_t count, float *frames)
{
    if (!m_channelCount) return 0;
    if (count == 0) return 0;

    m_d->buffer.mBuffers[0].mDataByteSize =
        sizeof(float) * m_channelCount * count;
    
    m_d->buffer.mBuffers[0].mData = frames;

    UInt32 framesRead = count;

    m_d->err = ExtAudioFileRead(m_d->file, &framesRead, &m_d->buffer);
    if (m_d->err) {
        m_error = "CoreAudioReadStream: Error in decoder: code " + codestr(m_d->err);
        throw InvalidFileFormat(m_path, "error in decoder");
    }

 //   cerr << "CoreAudioReadStream::getFrames: " << count << " frames requested across " << m_channelCount << " channel(s), " << framesRead << " frames actually read" << std::endl;

    return framesRead;
}

CoreAudioReadStream::~CoreAudioReadStream()
{
//    cerr << "CoreAudioReadStream::~CoreAudioReadStream" << std::endl;

    if (m_channelCount) {
	ExtAudioFileDispose(m_d->file);
    }

    m_channelCount = 0;

    delete m_d;
}

}

#endif

