/* -*- c-basic-offset: 4 indent-tabs-mode: nil -*-  vi:set ts=8 sts=4 sw=4: */

/*
    Sonic Visualiser
    An audio file viewer and annotation editor.
    Centre for Digital Music, Queen Mary, University of London.
    This file copyright 2006 Chris Cannam.
    
    This program is free software; you can redistribute it and/or
    modify it under the terms of the GNU General Public License as
    published by the Free Software Foundation; either version 2 of the
    License, or (at your option) any later version.  See the file
    COPYING included with this distribution for more information.
*/

#ifndef SV_CODED_AUDIO_FILE_READER_H
#define SV_CODED_AUDIO_FILE_READER_H

#include "AudioFileReader.h"

#include <QMutex>
#include <QReadWriteLock>

#ifdef Q_OS_WIN
#include <windows.h>
#define ENABLE_SNDFILE_WINDOWS_PROTOTYPES 1
#endif

#include <sndfile.h>

class WavFileReader;
class Serialiser;

namespace breakfastquay {
    class Resampler;
}

class CodedAudioFileReader : public AudioFileReader
{
    Q_OBJECT

public:
    virtual ~CodedAudioFileReader();

    enum CacheMode {
        CacheInTemporaryFile,
        CacheInMemory
    };

    enum DecodeMode {
        DecodeAtOnce, // decode the file on construction, with progress 
        DecodeThreaded // decode in a background thread after construction
    };

    floatvec_t getInterleavedFrames(sv_frame_t start, sv_frame_t count) const override;

    sv_samplerate_t getNativeRate() const override { return m_fileRate; }

    QString getLocalFilename() const override { return m_cacheFileName; }
    
    /// Intermediate cache means all CodedAudioFileReaders are quickly seekable
    bool isQuicklySeekable() const override { return true; }

signals:
    void progress(int);

protected:
    CodedAudioFileReader(CacheMode cacheMode, 
                         sv_samplerate_t targetRate,
                         bool normalised);

    void initialiseDecodeCache(); // samplerate, channels must have been set

    // compensation for encoder delays:
    void setFramesToTrim(sv_frame_t fromStart, sv_frame_t fromEnd);
    
    // may throw InsufficientDiscSpace:
    void addSamplesToDecodeCache(float **samples, sv_frame_t nframes);
    void addSamplesToDecodeCache(float *samplesInterleaved, sv_frame_t nframes);
    void addSamplesToDecodeCache(const floatvec_t &interleaved);

    // may throw InsufficientDiscSpace:
    void finishDecodeCache();

    bool isDecodeCacheInitialised() const { return m_initialised; }

    void startSerialised(QString id);
    void endSerialised();

private:
    void pushCacheWriteBufferMaybe(bool final);
    
    sv_frame_t pushBuffer(float *interleaved, sv_frame_t sz, bool final);

    // to be called only by pushBuffer
    void pushBufferResampling(float *interleaved, sv_frame_t sz, double ratio, bool final);

    // to be called only by pushBuffer and pushBufferResampling
    void pushBufferNonResampling(float *interleaved, sv_frame_t sz);

protected:
    QMutex m_cacheMutex;
    CacheMode m_cacheMode;
    floatvec_t m_data;
    mutable QMutex m_dataLock;
    bool m_initialised;
    Serialiser *m_serialiser;
    sv_samplerate_t m_fileRate;

    QString m_cacheFileName;
    SNDFILE *m_cacheFileWritePtr;
    WavFileReader *m_cacheFileReader;
    float *m_cacheWriteBuffer;
    sv_frame_t m_cacheWriteBufferIndex;  // buffer write pointer in samples
    sv_frame_t m_cacheWriteBufferFrames; // buffer size in frames

    breakfastquay::Resampler *m_resampler;
    float *m_resampleBuffer;
    int m_resampleBufferFrames;
    sv_frame_t m_fileFrameCount;

    bool m_normalised;
    float m_max;
    float m_gain;

    sv_frame_t m_trimFromStart;
    sv_frame_t m_trimFromEnd;
    
    sv_frame_t m_clippedCount;
    sv_frame_t m_firstNonzero;
    sv_frame_t m_lastNonzero;
};

#endif
