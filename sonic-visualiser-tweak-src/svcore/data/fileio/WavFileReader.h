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

#ifndef SV_WAV_FILE_READER_H
#define SV_WAV_FILE_READER_H

#include "AudioFileReader.h"

#ifdef Q_OS_WIN
#include <windows.h>
#define ENABLE_SNDFILE_WINDOWS_PROTOTYPES 1
#endif

#include <sndfile.h>
#include <QMutex>

#include <set>

/**
 * Reader for audio files using libsndfile.
 *
 * This is typically intended for seekable file types that can be read
 * directly (e.g. WAV, AIFF etc).
 *
 * Compressed files supported by libsndfile (e.g. Ogg, FLAC) should
 * normally be read using DecodingWavFileReader instead (which decodes
 * to an intermediate cached file).
 */
class WavFileReader : public AudioFileReader
{
public:
    enum class Normalisation { None, Peak };

    WavFileReader(FileSource source,
                  bool fileUpdating = false,
                  Normalisation normalise = Normalisation::None);
    virtual ~WavFileReader();

    QString getLocation() const override { return m_source.getLocation(); }
    QString getError() const override { return m_error; }

    QString getTitle() const override { return m_title; }
    QString getMaker() const override { return m_maker; }
    
    QString getLocalFilename() const override { return m_path; }
    
    bool isQuicklySeekable() const override { return m_seekable; }
    
    /** 
     * Must be safe to call from multiple threads with different
     * arguments on the same object at the same time.
     */
    floatvec_t getInterleavedFrames(sv_frame_t start, sv_frame_t count) const override;
    
    static void getSupportedExtensions(std::set<QString> &extensions);
    static bool supportsExtension(QString ext);
    static bool supportsContentType(QString type);
    static bool supports(FileSource &source);

    int getDecodeCompletion() const override { return 100; }

    bool isUpdating() const override { return m_updating; }

    void updateFrameCount();
    void updateDone();

protected:
    SF_INFO m_fileInfo;
    SNDFILE *m_file;

    FileSource m_source;
    QString m_path;
    QString m_error;
    QString m_title;
    QString m_maker;

    bool m_seekable;

    mutable QMutex m_mutex;
    mutable floatvec_t m_buffer;
    mutable sv_frame_t m_lastStart;
    mutable sv_frame_t m_lastCount;

    Normalisation m_normalisation;
    float m_max;

    bool m_updating;

    floatvec_t getInterleavedFramesUnnormalised(sv_frame_t start,
                                                sv_frame_t count) const;
    float getMax() const;
};

#endif
