/* -*- c-basic-offset: 4 indent-tabs-mode: nil -*-  vi:set ts=8 sts=4 sw=4: */

/*
    Sonic Visualiser
    An audio file viewer and annotation editor.
    Centre for Digital Music, Queen Mary, University of London.
    This file copyright 2006-2007 Chris Cannam and QMUL.
    
    This program is free software; you can redistribute it and/or
    modify it under the terms of the GNU General Public License as
    published by the Free Software Foundation; either version 2 of the
    License, or (at your option) any later version.  See the file
    COPYING included with this distribution for more information.
*/

#include "CodedAudioFileReader.h"

#include "WavFileReader.h"
#include "base/TempDirectory.h"
#include "base/Exceptions.h"
#include "base/Profiler.h"
#include "base/Serialiser.h"
#include "base/StorageAdviser.h"

#include <bqresample/Resampler.h>

#include <stdint.h>
#include <iostream>
#include <QDir>
#include <QMutexLocker>

using namespace std;

CodedAudioFileReader::CodedAudioFileReader(CacheMode cacheMode,
                                           sv_samplerate_t targetRate,
                                           bool normalised) :
    m_cacheMode(cacheMode),
    m_initialised(false),
    m_serialiser(nullptr),
    m_fileRate(0),
    m_cacheFileWritePtr(nullptr),
    m_cacheFileReader(nullptr),
    m_cacheWriteBuffer(nullptr),
    m_cacheWriteBufferIndex(0),
    m_cacheWriteBufferFrames(65536),
    m_resampler(nullptr),
    m_resampleBuffer(nullptr),
    m_resampleBufferFrames(0),
    m_fileFrameCount(0),
    m_normalised(normalised),
    m_max(0.f),
    m_gain(1.f),
    m_trimFromStart(0),
    m_trimFromEnd(0),
    m_clippedCount(0),
    m_firstNonzero(0),
    m_lastNonzero(0)
{
    SVDEBUG << "CodedAudioFileReader:: cache mode: " << cacheMode
            << " (" << (cacheMode == CacheInTemporaryFile
                        ? "CacheInTemporaryFile" : "CacheInMemory") << ")"
            << ", rate: " << targetRate
            << (targetRate == 0 ? " (use source rate)" : "")
            << ", normalised: " << normalised << endl;

    m_frameCount = 0;
    m_sampleRate = targetRate;
}

CodedAudioFileReader::~CodedAudioFileReader()
{
    QMutexLocker locker(&m_cacheMutex);

    if (m_serialiser) endSerialised();
    
    if (m_cacheFileWritePtr) sf_close(m_cacheFileWritePtr);

    SVDEBUG << "CodedAudioFileReader::~CodedAudioFileReader: deleting cache file reader" << endl;

    delete m_cacheFileReader;
    delete[] m_cacheWriteBuffer;
    
    if (m_cacheFileName != "") {
        SVDEBUG << "CodedAudioFileReader::~CodedAudioFileReader: deleting cache file " << m_cacheFileName << endl;
        if (!QFile(m_cacheFileName).remove()) {
            SVDEBUG << "WARNING: CodedAudioFileReader::~CodedAudioFileReader: Failed to delete cache file \"" << m_cacheFileName << "\"" << endl;
        }
    }

    delete m_resampler;
    delete[] m_resampleBuffer;

    if (!m_data.empty()) {
        StorageAdviser::notifyDoneAllocation
            (StorageAdviser::MemoryAllocation,
             (m_data.size() * sizeof(float)) / 1024);
    }
}

void
CodedAudioFileReader::setFramesToTrim(sv_frame_t fromStart, sv_frame_t fromEnd)
{
    m_trimFromStart = fromStart;
    m_trimFromEnd = fromEnd;
}

void
CodedAudioFileReader::startSerialised(QString id)
{
    SVDEBUG << "CodedAudioFileReader(" << this << ")::startSerialised: id = " << id << endl;

    delete m_serialiser;
    m_serialiser = new Serialiser(id);
}

void
CodedAudioFileReader::endSerialised()
{
    SVDEBUG << "CodedAudioFileReader(" << this << ")::endSerialised: id = " << (m_serialiser ? m_serialiser->getId() : "(none)") << endl;

    delete m_serialiser;
    m_serialiser = nullptr;
}

void
CodedAudioFileReader::initialiseDecodeCache()
{
    QMutexLocker locker(&m_cacheMutex);

    SVDEBUG << "CodedAudioFileReader::initialiseDecodeCache: file rate = " << m_fileRate << endl;

    if (m_channelCount == 0) {
        SVCERR << "CodedAudioFileReader::initialiseDecodeCache: No channel count set!" << endl;
        throw std::logic_error("No channel count set");
    }
    
    if (m_fileRate == 0) {
        SVDEBUG << "CodedAudioFileReader::initialiseDecodeCache: ERROR: File sample rate unknown (bug in subclass implementation?)" << endl;
        throw FileOperationFailed("(coded file)", "sample rate unknown (bug in subclass implementation?)");
    }
    if (m_sampleRate == 0) {
        m_sampleRate = m_fileRate;
        SVDEBUG << "CodedAudioFileReader::initialiseDecodeCache: rate (from file) = " << m_fileRate << endl;
    }
    if (m_fileRate != m_sampleRate) {
        SVDEBUG << "CodedAudioFileReader: resampling " << m_fileRate << " -> " <<  m_sampleRate << endl;

        breakfastquay::Resampler::Parameters params;
        params.quality = breakfastquay::Resampler::FastestTolerable;
        params.maxBufferSize = int(m_cacheWriteBufferFrames);
        params.initialSampleRate = m_fileRate;
        m_resampler = new breakfastquay::Resampler(params, m_channelCount);

        double ratio = m_sampleRate / m_fileRate;
        m_resampleBufferFrames = int(ceil(double(m_cacheWriteBufferFrames) *
                                          ratio + 1));
        m_resampleBuffer = new float[m_resampleBufferFrames * m_channelCount];
    }

    m_cacheWriteBuffer = new float[m_cacheWriteBufferFrames * m_channelCount];
    m_cacheWriteBufferIndex = 0;

    if (m_cacheMode == CacheInTemporaryFile) {

        try {
            QDir dir(TempDirectory::getInstance()->getPath());
            m_cacheFileName = dir.filePath(QString("decoded_%1.w64")
                                           .arg((intptr_t)this));

            SF_INFO fileInfo;
            int fileRate = int(round(m_sampleRate));
            if (m_sampleRate != sv_samplerate_t(fileRate)) {
                SVDEBUG << "CodedAudioFileReader: WARNING: Non-integer sample rate "
                     << m_sampleRate << " presented for writing, rounding to " << fileRate
                     << endl;
            }
            fileInfo.samplerate = fileRate;
            fileInfo.channels = m_channelCount;

            // Previously we were writing SF_FORMAT_PCM_16 and in a
            // comment I wrote: "No point in writing 24-bit or float;
            // generally this class is used for decoding files that
            // have come from a 16 bit source or that decode to only
            // 16 bits anyway." That was naive -- we want to preserve
            // the original values to the same float precision that we
            // use internally. Saving PCM_16 obviously doesn't
            // preserve values for sources at bit depths greater than
            // 16, but it also doesn't always do so for sources at bit
            // depths less than 16.
            //
            // (This came to light with a bug in libsndfile 1.0.26,
            // which always reports every file as non-seekable, so
            // that coded readers were being used even for WAV
            // files. This changed the values that came from PCM_8 WAV
            // sources, breaking Sonic Annotator's output comparison
            // tests.)
            //
            // So: now we write floats.
            fileInfo.format = SF_FORMAT_W64 | SF_FORMAT_FLOAT;

#ifdef Q_OS_WIN
            m_cacheFileWritePtr = sf_wchar_open
                ((LPCWSTR)m_cacheFileName.utf16(), SFM_WRITE, &fileInfo);
#else
            m_cacheFileWritePtr = sf_open
                (m_cacheFileName.toLocal8Bit(), SFM_WRITE, &fileInfo);
#endif

            if (m_cacheFileWritePtr) {

                // Ideally we would do this now only if we were in a
                // threaded mode -- creating the reader later if we're
                // not threaded -- but we don't have access to that
                // information here

                m_cacheFileReader = new WavFileReader(m_cacheFileName);

                if (!m_cacheFileReader->isOK()) {
                    SVDEBUG << "ERROR: CodedAudioFileReader::initialiseDecodeCache: Failed to construct WAV file reader for temporary file: " << m_cacheFileReader->getError() << endl;
                    delete m_cacheFileReader;
                    m_cacheFileReader = nullptr;
                    m_cacheMode = CacheInMemory;
                    sf_close(m_cacheFileWritePtr);
                }

            } else {
                SVDEBUG << "CodedAudioFileReader::initialiseDecodeCache: failed to open cache file \"" << m_cacheFileName << "\" (" << m_channelCount << " channels, sample rate " << m_sampleRate << " for writing, falling back to in-memory cache" << endl;
                m_cacheMode = CacheInMemory;
            }

        } catch (const DirectoryCreationFailed &f) {
            SVDEBUG << "CodedAudioFileReader::initialiseDecodeCache: failed to create temporary directory! Falling back to in-memory cache" << endl;
            m_cacheMode = CacheInMemory;
        }
    }

    if (m_cacheMode == CacheInMemory) {
        m_data.clear();
    }

    if (m_trimFromEnd >= (m_cacheWriteBufferFrames * m_channelCount)) {
        SVCERR << "WARNING: CodedAudioFileReader::setSamplesToTrim: Can't handle trimming more frames from end (" << m_trimFromEnd << ") than can be stored in cache-write buffer (" << (m_cacheWriteBufferFrames * m_channelCount) << "), won't trim anything from the end after all";
        m_trimFromEnd = 0;
    }

    m_initialised = true;
}

void
CodedAudioFileReader::addSamplesToDecodeCache(float **samples, sv_frame_t nframes)
{
    QMutexLocker locker(&m_cacheMutex);

    if (!m_initialised) return;

    for (sv_frame_t i = 0; i < nframes; ++i) {

        if (m_trimFromStart > 0) {
            --m_trimFromStart;
            continue;
        }
        
        for (int c = 0; c < m_channelCount; ++c) {

            float sample = samples[c][i];
            m_cacheWriteBuffer[m_cacheWriteBufferIndex++] = sample;

        }

        pushCacheWriteBufferMaybe(false);
    }
}

void
CodedAudioFileReader::addSamplesToDecodeCache(float *samples, sv_frame_t nframes)
{
    QMutexLocker locker(&m_cacheMutex);

    if (!m_initialised) return;

    for (sv_frame_t i = 0; i < nframes; ++i) {

        if (m_trimFromStart > 0) {
            --m_trimFromStart;
            continue;
        }
        
        for (int c = 0; c < m_channelCount; ++c) {

            float sample = samples[i * m_channelCount + c];
        
            m_cacheWriteBuffer[m_cacheWriteBufferIndex++] = sample;
        }

        pushCacheWriteBufferMaybe(false);
    }
}

void
CodedAudioFileReader::addSamplesToDecodeCache(const floatvec_t &samples)
{
    QMutexLocker locker(&m_cacheMutex);

    if (!m_initialised) return;

    for (float sample: samples) {

        if (m_trimFromStart > 0) {
            --m_trimFromStart;
            continue;
        }
        
        m_cacheWriteBuffer[m_cacheWriteBufferIndex++] = sample;

        pushCacheWriteBufferMaybe(false);
    }
}

void
CodedAudioFileReader::finishDecodeCache()
{
    QMutexLocker locker(&m_cacheMutex);

    Profiler profiler("CodedAudioFileReader::finishDecodeCache");

    if (!m_initialised) {
        SVDEBUG << "WARNING: CodedAudioFileReader::finishDecodeCache: Cache was never initialised!" << endl;
        return;
    }

    pushCacheWriteBufferMaybe(true);

    delete[] m_cacheWriteBuffer;
    m_cacheWriteBuffer = nullptr;

    delete[] m_resampleBuffer;
    m_resampleBuffer = nullptr;

    delete m_resampler;
    m_resampler = nullptr;

    if (m_cacheMode == CacheInTemporaryFile) {

        sf_close(m_cacheFileWritePtr);
        m_cacheFileWritePtr = nullptr;
        if (m_cacheFileReader) m_cacheFileReader->updateFrameCount();

    } else {
        // I know, I know, we already allocated it...
        StorageAdviser::notifyPlannedAllocation
            (StorageAdviser::MemoryAllocation,
             (m_data.size() * sizeof(float)) / 1024);
    }

    SVDEBUG << "CodedAudioFileReader: File decodes to " << m_fileFrameCount
            << " frames" << endl;
    if (m_fileFrameCount != m_frameCount) {
        SVDEBUG << "CodedAudioFileReader: Resampled to " << m_frameCount
                << " frames" << endl;
    }
    SVDEBUG << "CodedAudioFileReader: Signal abs max is " << m_max
            << ", " << m_clippedCount
            << " samples clipped, first non-zero frame is at "
            << m_firstNonzero << ", last at " << m_lastNonzero << endl;
    if (m_normalised) {
        SVDEBUG << "CodedAudioFileReader: Normalising, gain is " << m_gain << endl;
    }
}

void
CodedAudioFileReader::pushCacheWriteBufferMaybe(bool final)
{
    if (final ||
        (m_cacheWriteBufferIndex ==
         m_cacheWriteBufferFrames * m_channelCount)) {

        if (m_trimFromEnd > 0) {
        
            sv_frame_t framesToPush =
                (m_cacheWriteBufferIndex / m_channelCount) - m_trimFromEnd;

            if (framesToPush <= 0 && !final) {
                // This won't do, the buffer is full so we have to push
                // something. Should have checked for this earlier
                throw std::logic_error("Buffer full but nothing to push");
            }

            pushBuffer(m_cacheWriteBuffer, framesToPush, final);
            
            m_cacheWriteBufferIndex -= framesToPush * m_channelCount;

            for (sv_frame_t i = 0; i < m_cacheWriteBufferIndex; ++i) {
                m_cacheWriteBuffer[i] =
                    m_cacheWriteBuffer[framesToPush * m_channelCount + i];
            }

        } else {

            pushBuffer(m_cacheWriteBuffer,
                       m_cacheWriteBufferIndex / m_channelCount,
                       final);

            m_cacheWriteBufferIndex = 0;
        }

        if (m_cacheFileReader) {
            m_cacheFileReader->updateFrameCount();
        }
    }
}

sv_frame_t
CodedAudioFileReader::pushBuffer(float *buffer, sv_frame_t sz, bool final)
{
    m_fileFrameCount += sz;

    double ratio = 1.0;
    if (m_resampler && m_fileRate != 0) {
        ratio = m_sampleRate / m_fileRate;
    }
        
    if (ratio != 1.0) {
        pushBufferResampling(buffer, sz, ratio, final);
    } else {
        pushBufferNonResampling(buffer, sz);
    }

    return sz;
}

void
CodedAudioFileReader::pushBufferNonResampling(float *buffer, sv_frame_t sz)
{
    float clip = 1.0;
    sv_frame_t count = sz * m_channelCount;

    // statistics
    for (sv_frame_t j = 0; j < sz; ++j) {
        for (int c = 0; c < m_channelCount; ++c) {
            sv_frame_t i = j * m_channelCount + c;
            float v = buffer[i];
            if (!m_normalised) {
                if (v > clip) {
                    buffer[i] = clip;
                    ++m_clippedCount;
                } else if (v < -clip) {
                    buffer[i] = -clip;
                    ++m_clippedCount;
                }
            }
            v = fabsf(v);
            if (v != 0.f) {
                if (m_firstNonzero == 0) {
                    m_firstNonzero = m_frameCount;
                }
                m_lastNonzero = m_frameCount;
                if (v > m_max) {
                    m_max = v;
                }
            }
        }
        ++m_frameCount;
    }

    if (m_max > 0.f) {
        m_gain = 1.f / m_max; // used when normalising only
    }

    switch (m_cacheMode) {

    case CacheInTemporaryFile:
        if (sf_writef_float(m_cacheFileWritePtr, buffer, sz) < sz) {
            sf_close(m_cacheFileWritePtr);
            m_cacheFileWritePtr = nullptr;
            throw InsufficientDiscSpace(TempDirectory::getInstance()->getPath());
        }
        break;

    case CacheInMemory:
        m_dataLock.lock();
        try {
            m_data.insert(m_data.end(), buffer, buffer + count);
        } catch (const std::bad_alloc &e) {
            m_data.clear();
            SVCERR << "CodedAudioFileReader: Caught bad_alloc when trying to add " << count << " elements to buffer" << endl;
            m_dataLock.unlock();
            throw e;
        }
        m_dataLock.unlock();
        break;
    }
}

void
CodedAudioFileReader::pushBufferResampling(float *buffer, sv_frame_t sz,
                                           double ratio, bool final)
{
//    SVDEBUG << "pushBufferResampling: ratio = " << ratio << ", sz = " << sz << ", final = " << final << endl;

    if (sz > 0) {

        sv_frame_t out = m_resampler->resampleInterleaved
            (m_resampleBuffer,
             m_resampleBufferFrames,
             buffer,
             int(sz),
             ratio,
             false);

        pushBufferNonResampling(m_resampleBuffer, out);
    }

    if (final) {

        sv_frame_t padFrames = 1;
        if (double(m_frameCount) / ratio < double(m_fileFrameCount)) {
            padFrames = m_fileFrameCount - sv_frame_t(double(m_frameCount) / ratio) + 1;
        }

        sv_frame_t padSamples = padFrames * m_channelCount;

        SVDEBUG << "CodedAudioFileReader::pushBufferResampling: frameCount = " << m_frameCount << ", equivFileFrames = " << double(m_frameCount) / ratio << ", m_fileFrameCount = " << m_fileFrameCount << ", padFrames = " << padFrames << ", padSamples = " << padSamples << endl;

        float *padding = new float[padSamples];
        for (sv_frame_t i = 0; i < padSamples; ++i) padding[i] = 0.f;

        sv_frame_t out = m_resampler->resampleInterleaved
            (m_resampleBuffer,
             m_resampleBufferFrames,
             padding,
             int(padFrames),
             ratio,
             true);

        SVDEBUG << "CodedAudioFileReader::pushBufferResampling: resampled padFrames to " << out << " frames" << endl;

        sv_frame_t expected = sv_frame_t(round(double(m_fileFrameCount) * ratio));
        if (m_frameCount + out > expected) {
            out = expected - m_frameCount;
            SVDEBUG << "CodedAudioFileReader::pushBufferResampling: clipping that to " << out << " to avoid producing more samples than desired" << endl;
        }

        pushBufferNonResampling(m_resampleBuffer, out);
        delete[] padding;
    }
}

floatvec_t
CodedAudioFileReader::getInterleavedFrames(sv_frame_t start, sv_frame_t count) const
{
    // Lock is only required in CacheInMemory mode (the cache file
    // reader is expected to be thread safe and manage its own
    // locking)

    if (!m_initialised) {
        SVDEBUG << "CodedAudioFileReader::getInterleavedFrames: not initialised" << endl;
        return {};
    }

    floatvec_t frames;
    
    switch (m_cacheMode) {

    case CacheInTemporaryFile:
        if (m_cacheFileReader) {
            frames = m_cacheFileReader->getInterleavedFrames(start, count);
        }
        break;

    case CacheInMemory:
    {
        if (!isOK()) return {};
        if (count == 0) return {};

        sv_frame_t ix0 = start * m_channelCount;
        sv_frame_t ix1 = ix0 + (count * m_channelCount);

        // This lock used to be a QReadWriteLock, but it appears that
        // its lock mechanism is significantly slower than QMutex so
        // it's not a good idea in cases like this where we don't
        // really have threads taking a long time to read concurrently
        m_dataLock.lock();
        sv_frame_t n = sv_frame_t(m_data.size());
        if (ix0 > n) ix0 = n;
        if (ix1 > n) ix1 = n;
        frames = floatvec_t(m_data.begin() + ix0, m_data.begin() + ix1);
        m_dataLock.unlock();
        break;
    }
    }

    if (m_normalised) {
        for (auto &f: frames) f *= m_gain;
    }

    return frames;
}

