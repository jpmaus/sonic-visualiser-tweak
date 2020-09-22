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

#ifndef SV_MP3_FILE_READER_H
#define SV_MP3_FILE_READER_H

#ifdef HAVE_MAD

#include "CodedAudioFileReader.h"

#include "base/Thread.h"
#include <mad.h>

#include <set>

class ProgressReporter;

class MP3FileReader : public CodedAudioFileReader
{
    Q_OBJECT

public:
    /**
     * How the MP3FileReader should handle leading and trailing gaps.
     * See http://lame.sourceforge.net/tech-FAQ.txt for a technical
     * explanation of the numbers here.
     */
    enum class GaplessMode {
        /**
         * Trim unwanted samples from the start and end of the decoded
         * audio. From the start, trim a number of samples equal to
         * the decoder delay (a fixed 529 samples) plus any encoder
         * delay that may be specified in Xing/LAME metadata. From the
         * end, trim any padding specified in Xing/LAME metadata, less
         * the fixed decoder delay. This usually results in "gapless"
         * audio, i.e. with no spurious zero padding at either end.
         */
        Gapless,

        /**
         * Do not trim any samples. Also do not suppress any frames
         * from being passed to the mp3 decoder, even Xing/LAME
         * metadata frames. This will result in the audio being padded
         * with zeros at either end: at the start, typically
         * 529+576+1152 = 2257 samples for LAME-encoded mp3s; at the
         * end an unknown number depending on the fill ratio of the
         * final coded frame, but typically less than 1152-529 = 623.
         *
         * This mode produces the same output as produced by older
         * versions of this code before the gapless option was added,
         * and is present mostly for backward compatibility.
         */
        Gappy
    };
    
    MP3FileReader(FileSource source,
                  DecodeMode decodeMode,
                  CacheMode cacheMode,
                  GaplessMode gaplessMode,
                  sv_samplerate_t targetRate = 0,
                  bool normalised = false,
                  ProgressReporter *reporter = 0);
    virtual ~MP3FileReader();

    QString getError() const override { return m_error; }

    QString getLocation() const override { return m_source.getLocation(); }
    QString getTitle() const override { return m_title; }
    QString getMaker() const override { return m_maker; }
    TagMap getTags() const override { return m_tags; }
    
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
    TagMap m_tags;
    GaplessMode m_gaplessMode;
    sv_frame_t m_fileSize;
    double m_bitrateNum;
    int m_bitrateDenom;
    int m_mp3FrameCount;
    int m_completion;
    bool m_done;

    unsigned char *m_fileBuffer;
    size_t m_fileBufferSize;
    
    float **m_sampleBuffer;
    size_t m_sampleBufferSize;

    ProgressReporter *m_reporter;
    bool m_cancelled;

    bool m_decodeErrorShown;

    struct DecoderData {
        unsigned char const *start;
        sv_frame_t length;
        bool finished;
        MP3FileReader *reader;
    };

    bool decode(void *mm, sv_frame_t sz);
    enum mad_flow filter(struct mad_stream const *, struct mad_frame *);
    enum mad_flow accept(struct mad_header const *, struct mad_pcm *);

    static enum mad_flow input_callback(void *, struct mad_stream *);
    static enum mad_flow output_callback(void *, struct mad_header const *,
                                         struct mad_pcm *);
    static enum mad_flow filter_callback(void *, struct mad_stream const *,
                                         struct mad_frame *);
    static enum mad_flow error_callback(void *, struct mad_stream *,
                                        struct mad_frame *);

    class DecodeThread : public Thread
    {
    public:
        DecodeThread(MP3FileReader *reader) : m_reader(reader) { }
        void run() override;

    protected:
        MP3FileReader *m_reader;
    };

    DecodeThread *m_decodeThread;

    void loadTags(int fd);
    QString loadTag(void *vtag, const char *name);
};

#endif

#endif
