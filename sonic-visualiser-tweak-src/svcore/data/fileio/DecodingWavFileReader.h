/* -*- c-basic-offset: 4 indent-tabs-mode: nil -*-  vi:set ts=8 sts=4 sw=4: */

/*
    Sonic Visualiser
    An audio file viewer and annotation editor.
    Centre for Digital Music, Queen Mary, University of London.
    This file copyright 2007 QMUL.
    
    This program is free software; you can redistribute it and/or
    modify it under the terms of the GNU General Public License as
    published by the Free Software Foundation; either version 2 of the
    License, or (at your option) any later version.  See the file
    COPYING included with this distribution for more information.
*/

#ifndef SV_DECODING_WAV_FILE_READER_H
#define SV_DECODING_WAV_FILE_READER_H

#include "CodedAudioFileReader.h"

#include "base/Thread.h"

#include <set>

class WavFileReader;
class ProgressReporter;

class DecodingWavFileReader : public CodedAudioFileReader
{
    Q_OBJECT
public:
    DecodingWavFileReader(FileSource source,
                          DecodeMode decodeMode, // determines when to resample
                          CacheMode cacheMode,
                          sv_samplerate_t targetRate = 0,
                          bool normalised = false,
                          ProgressReporter *reporter = 0);
    virtual ~DecodingWavFileReader();

    QString getTitle() const override { return m_title; }
    QString getMaker() const override { return m_maker; }
    
    QString getError() const override { return m_error; }
    QString getLocation() const override { return m_source.getLocation(); }

    static void getSupportedExtensions(std::set<QString> &extensions);
    static bool supportsExtension(QString ext);
    static bool supportsContentType(QString type);
    static bool supports(FileSource &source);

    int getDecodeCompletion() const override { return m_completion; }

    bool isUpdating() const override {
        return m_decodeThread && m_decodeThread->isRunning();
    }

public slots:
    void cancelled();

protected:
    FileSource m_source;
    QString m_title;
    QString m_maker;
    QString m_path;
    QString m_error;
    bool m_cancelled;
    sv_frame_t m_processed;
    int m_completion;

    WavFileReader *m_original;
    ProgressReporter *m_reporter;

    void addBlock(const floatvec_t &frames);
    
    class DecodeThread : public Thread
    {
    public:
        DecodeThread(DecodingWavFileReader *reader) : m_reader(reader) { }
        void run() override;

    protected:
        DecodingWavFileReader *m_reader;
    };

    DecodeThread *m_decodeThread;
};

#endif

