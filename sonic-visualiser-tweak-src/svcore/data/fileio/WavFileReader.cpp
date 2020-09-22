/* -*- c-basic-offset: 4 indent-tabs-mode: nil -*-  vi:set ts=8 sts=4 sw=4: */

/*
    Sonic Visualiser
    An audio file viewer and annotation editor.
    Centre for Digital Music, Queen Mary, University of London.
    This file copyright 2006 Chris Cannam and QMUL.
    
    This program is free software; you can redistribute it and/or
    modify it under the terms of the GNU General Public License as
    published by the Free Software Foundation; either version 2 of the
    License, or (at your option) any later version.  See the file
    COPYING included with this distribution for more information.
*/

#include "WavFileReader.h"

#include "base/HitCount.h"
#include "base/Profiler.h"

#include <iostream>

#include <QMutexLocker>
#include <QFileInfo>

using namespace std;

WavFileReader::WavFileReader(FileSource source,
                             bool fileUpdating,
                             Normalisation normalisation) :
    m_file(nullptr),
    m_source(source),
    m_path(source.getLocalFilename()),
    m_seekable(false),
    m_lastStart(0),
    m_lastCount(0),
    m_normalisation(normalisation),
    m_max(0.f),
    m_updating(fileUpdating)
{
    m_frameCount = 0;
    m_channelCount = 0;
    m_sampleRate = 0;

    m_fileInfo.format = 0;
    m_fileInfo.frames = 0;

#ifdef Q_OS_WIN
    m_file = sf_wchar_open((LPCWSTR)m_path.utf16(), SFM_READ, &m_fileInfo);
#else
    m_file = sf_open(m_path.toLocal8Bit(), SFM_READ, &m_fileInfo);
#endif

    if (!m_file || (!fileUpdating && m_fileInfo.channels <= 0)) {
        SVDEBUG << "WavFileReader::initialize: Failed to open file at \""
                << m_path << "\" ("
                << sf_strerror(m_file) << ")" << endl;

        if (m_file) {
            m_error = QString("Couldn't load audio file '%1':\n%2")
                .arg(m_path).arg(sf_strerror(m_file));
        } else {
            m_error = QString("Failed to open audio file '%1'")
                .arg(m_path);
        }
        return;
    }

    if (m_fileInfo.channels > 0) {

        m_frameCount = m_fileInfo.frames;
        m_channelCount = m_fileInfo.channels;
        m_sampleRate = m_fileInfo.samplerate;

        m_seekable = (m_fileInfo.seekable != 0);

        int type = m_fileInfo.format & SF_FORMAT_TYPEMASK;
        int subtype = m_fileInfo.format & SF_FORMAT_SUBMASK;

        if (type >= SF_FORMAT_FLAC || type >= SF_FORMAT_OGG) {
            // Our m_seekable reports whether a file is rapidly
            // seekable, so things like Ogg don't qualify. We
            // cautiously report every file type of "at least" the
            // historical period of Ogg or FLAC as non-seekable.
            m_seekable = false;
        } else if (type == SF_FORMAT_WAV && subtype <= SF_FORMAT_DOUBLE) {
            // libsndfile 1.0.26 has a bug (subsequently fixed in the
            // repo) that causes all files to be reported as
            // non-seekable. We know that certain common file types
            // are definitely seekable so, again cautiously, identify
            // and mark those (basically only non-adaptive WAVs).
            m_seekable = true;
        }

        if (m_normalisation != Normalisation::None && !m_updating) {
            m_max = getMax();
        }
        
        const char *str = sf_get_string(m_file, SF_STR_TITLE);
        if (str) {
            m_title = str;
        }
        str = sf_get_string(m_file, SF_STR_ARTIST);
        if (str) {
            m_maker = str;
        }
    }

    SVDEBUG << "WavFileReader: Filename " << m_path << ", frame count " << m_frameCount << ", channel count " << m_channelCount << ", sample rate " << m_sampleRate << ", format " << m_fileInfo.format << ", seekable " << m_fileInfo.seekable << " adjusted to " << m_seekable << ", normalisation " << int(m_normalisation) << endl;
}

WavFileReader::~WavFileReader()
{
    if (m_file) sf_close(m_file);
}

void
WavFileReader::updateFrameCount()
{
    QMutexLocker locker(&m_mutex);

    sv_frame_t prevCount = m_fileInfo.frames;

    if (m_file) {
        sf_close(m_file);
#ifdef Q_OS_WIN
        m_file = sf_wchar_open((LPCWSTR)m_path.utf16(), SFM_READ, &m_fileInfo);
#else
        m_file = sf_open(m_path.toLocal8Bit(), SFM_READ, &m_fileInfo);
#endif
        if (!m_file || m_fileInfo.channels <= 0) {
            SVDEBUG << "WavFileReader::updateFrameCount: Failed to open file at \"" << m_path << "\" ("
                    << sf_strerror(m_file) << ")" << endl;
        }
    }

//    SVDEBUG << "WavFileReader::updateFrameCount: now " << m_fileInfo.frames << endl;

    m_frameCount = m_fileInfo.frames;

    if (m_channelCount == 0) {
        m_channelCount = m_fileInfo.channels;
        m_sampleRate = m_fileInfo.samplerate;
    }

    if (m_frameCount != prevCount) {
        emit frameCountChanged();
    }
}

void
WavFileReader::updateDone()
{
    updateFrameCount();
    m_updating = false;
    if (m_normalisation != Normalisation::None) {
        m_max = getMax();
    }
}

floatvec_t
WavFileReader::getInterleavedFrames(sv_frame_t start, sv_frame_t count) const
{
    floatvec_t frames = getInterleavedFramesUnnormalised(start, count);

    if (m_normalisation == Normalisation::None || m_max == 0.f) {
        return frames;
    }

    for (int i = 0; in_range_for(frames, i); ++i) {
        frames[i] /= m_max;
    }
    
    return frames;
}

floatvec_t
WavFileReader::getInterleavedFramesUnnormalised(sv_frame_t start,
                                                sv_frame_t count) const
{
    static HitCount lastRead("WavFileReader: last read");

    if (count == 0) return {};

    QMutexLocker locker(&m_mutex);

    Profiler profiler("WavFileReader::getInterleavedFrames");
    
    if (!m_file || !m_channelCount) {
        return {};
    }

    if (start >= m_fileInfo.frames) {
//        SVDEBUG << "WavFileReader::getInterleavedFrames: " << start
//                  << " > " << m_fileInfo.frames << endl;
        return {};
    }

    if (start + count > m_fileInfo.frames) {
        count = m_fileInfo.frames - start;
    }

    // Because WaveFileModel::getSummaries() is called separately for
    // individual channels, it's quite common for us to be called
    // repeatedly for the same data. So this is worth cacheing.
    if (start == m_lastStart && count == m_lastCount) {
        lastRead.hit();
        return m_buffer;
    }

    // We don't actually support partial cache reads, but let's use
    // the term partial to refer to any forward seek and consider a
    // backward seek to be a miss
    if (start >= m_lastStart) {
        lastRead.partial();
    } else {
        lastRead.miss();
    }
    
    if (sf_seek(m_file, start, SEEK_SET) < 0) {
        return {};
    }

    floatvec_t data;
    sv_frame_t n = count * m_fileInfo.channels;
    data.resize(n);

    m_lastStart = start;
    m_lastCount = count;
    
    sf_count_t readCount = 0;
    if ((readCount = sf_readf_float(m_file, data.data(), count)) < 0) {
        return {};
    }

    m_buffer = data;
    return data;
}

float
WavFileReader::getMax() const
{
    if (!m_file || !m_channelCount) {
        return 0.f;
    }

    // First try for a PEAK chunk

    double sfpeak = 0.0;
    if (sf_command(m_file, SFC_GET_SIGNAL_MAX, &sfpeak, sizeof(sfpeak))
        == SF_TRUE) {
        SVDEBUG << "File has a PEAK chunk reporting max level " << sfpeak
                << endl;
        return float(fabs(sfpeak));
    }

    // Failing that, read all the samples

    float peak = 0.f;
    sv_frame_t ix = 0, chunk = 65536;

    while (ix < m_frameCount) {
        auto frames = getInterleavedFrames(ix, chunk);
        for (float x: frames) {
            float level = fabsf(x);
            if (level > peak) {
                peak = level;
            }
        }
        ix += chunk;
    }

    SVDEBUG << "Measured file peak max level as " << peak << endl;
    return peak;
}

void
WavFileReader::getSupportedExtensions(set<QString> &extensions)
{
    int count;

    if (sf_command(nullptr, SFC_GET_FORMAT_MAJOR_COUNT, &count, sizeof(count))) {
        extensions.insert("wav");
        extensions.insert("aiff");
        extensions.insert("aifc");
        extensions.insert("aif");
        return;
    }

    SF_FORMAT_INFO info;
    for (int i = 0; i < count; ++i) {
        info.format = i;
        if (!sf_command(nullptr, SFC_GET_FORMAT_MAJOR, &info, sizeof(info))) {
            QString ext = QString(info.extension).toLower();
            extensions.insert(ext);
            if (ext == "oga") {
                // libsndfile is awfully proper, it says it only
                // supports .oga but lots of Ogg audio files in the
                // wild are .ogg and it will accept that
                extensions.insert("ogg");
            }
        }
    }
}

bool
WavFileReader::supportsExtension(QString extension)
{
    set<QString> extensions;
    getSupportedExtensions(extensions);
    return (extensions.find(extension.toLower()) != extensions.end());
}

bool
WavFileReader::supportsContentType(QString type)
{
    return (type == "audio/x-wav" ||
            type == "audio/x-aiff" ||
            type == "audio/basic");
}

bool
WavFileReader::supports(FileSource &source)
{
    return (supportsExtension(source.getExtension()) ||
            supportsContentType(source.getContentType()));
}


