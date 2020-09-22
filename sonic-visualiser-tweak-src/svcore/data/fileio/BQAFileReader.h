/* -*- c-basic-offset: 4 indent-tabs-mode: nil -*-  vi:set ts=8 sts=4 sw=4: */

/*
    Sonic Visualiser
    An audio file viewer and annotation editor.
    Centre for Digital Music, Queen Mary, University of London.
    
    This program is free software; you can redistribute it and/or
    modify it under the terms of the GNU General Public License as
    published by the Free Software Foundation; either version 2 of the
    License, or (at your option) any later version.  See the file
    COPYING included with this distribution for more information.
*/

#ifndef SV_BQA_FILE_READER_H
#define SV_BQA_FILE_READER_H

#include <bqaudiostream/AudioReadStreamFactory.h>

#include "CodedAudioFileReader.h"
#include "base/Thread.h"

#include <set>

class ProgressReporter;

/**
 * Audio file reader using bqaudiostream library AudioReadStream
 * classes.
 */
class BQAFileReader : public CodedAudioFileReader
{
    Q_OBJECT

public:
    BQAFileReader(FileSource source,
                  DecodeMode decodeMode,
                  CacheMode cacheMode,
                  sv_samplerate_t targetRate = 0,
                  bool normalised = false,
                  ProgressReporter *reporter = 0);
    virtual ~BQAFileReader();

    QString getError() const override { return m_error; }
    QString getLocation() const override { return m_source.getLocation(); }
    QString getTitle() const override { return m_title; }
    QString getMaker() const override { return m_maker; }
    
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
    QString m_path;
    QString m_error;
    QString m_title;
    QString m_maker;

    breakfastquay::AudioReadStream *m_stream;

    bool m_cancelled;
    int m_completion;
    ProgressReporter *m_reporter;
    
    class DecodeThread : public Thread {
    public:
        DecodeThread(BQAFileReader *reader) : m_reader(reader) { }
        virtual void run();
    protected:
	BQAFileReader *m_reader;
    };
    DecodeThread *m_decodeThread;
};

#endif

