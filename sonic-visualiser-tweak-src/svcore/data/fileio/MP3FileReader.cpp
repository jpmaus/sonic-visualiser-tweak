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

#ifdef HAVE_MAD

#include "MP3FileReader.h"
#include "base/ProgressReporter.h"

#include "system/System.h"

#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>

#include <iostream>

#include <cstdlib>

#ifdef HAVE_ID3TAG
#include <id3tag.h>
#endif

#ifdef _WIN32
#include <io.h>
#include <fcntl.h>
#else
#include <fcntl.h>
#include <unistd.h>
#endif

#include <QFileInfo>

#include <QTextCodec>

using std::string;

static sv_frame_t DEFAULT_DECODER_DELAY = 529;

MP3FileReader::MP3FileReader(FileSource source, DecodeMode decodeMode, 
                             CacheMode mode, GaplessMode gaplessMode,
                             sv_samplerate_t targetRate,
                             bool normalised,
                             ProgressReporter *reporter) :
    CodedAudioFileReader(mode, targetRate, normalised),
    m_source(source),
    m_path(source.getLocalFilename()),
    m_gaplessMode(gaplessMode),
    m_decodeErrorShown(false),
    m_decodeThread(nullptr)
{
    SVDEBUG << "MP3FileReader: local path: \"" << m_path
            << "\", decode mode: " << decodeMode << " ("
            << (decodeMode == DecodeAtOnce ? "DecodeAtOnce" : "DecodeThreaded")
            << ")" << endl;
    
    m_channelCount = 0;
    m_fileRate = 0;
    m_fileSize = 0;
    m_bitrateNum = 0;
    m_bitrateDenom = 0;
    m_cancelled = false;
    m_mp3FrameCount = 0;
    m_completion = 0;
    m_done = false;
    m_reporter = reporter;

    if (m_gaplessMode == GaplessMode::Gapless) {
        CodedAudioFileReader::setFramesToTrim(DEFAULT_DECODER_DELAY, 0);
    }
    
    m_fileSize = 0;

    m_fileBuffer = nullptr;
    m_fileBufferSize = 0;

    m_sampleBuffer = nullptr;
    m_sampleBufferSize = 0;

    QFile qfile(m_path);
    if (!qfile.open(QIODevice::ReadOnly)) {
        m_error = QString("Failed to open file %1 for reading.").arg(m_path);
        SVDEBUG << "MP3FileReader: " << m_error << endl;
        return;
    }   

    m_fileSize = qfile.size();
    
    try {
        // We need a mysterious MAD_BUFFER_GUARD (== 8) zero bytes at
        // end of input, to ensure libmad decodes the last frame
        // correctly. Otherwise the decoded audio is truncated.
        SVDEBUG << "file size = " << m_fileSize << ", buffer guard = " << MAD_BUFFER_GUARD << endl;
        m_fileBufferSize = m_fileSize + MAD_BUFFER_GUARD;
        m_fileBuffer = new unsigned char[m_fileBufferSize];
        memset(m_fileBuffer + m_fileSize, 0, MAD_BUFFER_GUARD);
    } catch (...) {
        m_error = QString("Out of memory");
        SVDEBUG << "MP3FileReader: " << m_error << endl;
        return;
    }

    auto amountRead = qfile.read(reinterpret_cast<char *>(m_fileBuffer),
                                 m_fileSize);

    if (amountRead < m_fileSize) {
        SVCERR << QString("MP3FileReader::MP3FileReader: Warning: reached EOF after only %1 of %2 bytes")
            .arg(amountRead).arg(m_fileSize) << endl;
        memset(m_fileBuffer + amountRead, 0, m_fileSize - amountRead);
        m_fileSize = amountRead;
    }
        
    loadTags(qfile.handle());

    qfile.close();

    if (decodeMode == DecodeAtOnce) {

        if (m_reporter) {
            connect(m_reporter, SIGNAL(cancelled()), this, SLOT(cancelled()));
            m_reporter->setMessage
                (tr("Decoding %1...").arg(QFileInfo(m_path).fileName()));
        }

        if (!decode(m_fileBuffer, m_fileBufferSize)) {
            m_error = QString("Failed to decode file %1.").arg(m_path);
        }

        if (m_sampleBuffer) {
            for (int c = 0; c < m_channelCount; ++c) {
                delete[] m_sampleBuffer[c];
            }
            delete[] m_sampleBuffer;
            m_sampleBuffer = nullptr;
        }
        
        delete[] m_fileBuffer;
        m_fileBuffer = nullptr;

        if (isDecodeCacheInitialised()) finishDecodeCache();
        endSerialised();

    } else {

        if (m_reporter) m_reporter->setProgress(100);

        m_decodeThread = new DecodeThread(this);
        m_decodeThread->start();

        while ((m_channelCount == 0 || m_fileRate == 0 || m_sampleRate == 0)
               && !m_done) {
            usleep(10);
        }
        
        SVDEBUG << "MP3FileReader: decoding startup complete, file rate = " << m_fileRate << endl;
    }

    if (m_error != "") {
        SVDEBUG << "MP3FileReader::MP3FileReader(\"" << m_path << "\"): ERROR: " << m_error << endl;
    }
}

MP3FileReader::~MP3FileReader()
{
    if (m_decodeThread) {
        m_cancelled = true;
        m_decodeThread->wait();
        delete m_decodeThread;
    }
}

void
MP3FileReader::cancelled()
{
    m_cancelled = true;
}

void
MP3FileReader::loadTags(int fd)
{
    m_title = "";

#ifdef HAVE_ID3TAG

#ifdef _WIN32
    int id3fd = _dup(fd);
#else
    int id3fd = dup(fd);
#endif

    id3_file *file = id3_file_fdopen(id3fd, ID3_FILE_MODE_READONLY);
    if (!file) return;

    // We can do this a lot more elegantly, but we'll leave that for
    // when we implement support for more than just the one tag!
    
    id3_tag *tag = id3_file_tag(file);
    if (!tag) {
        SVDEBUG << "MP3FileReader::loadTags: No ID3 tag found" << endl;
        id3_file_close(file); // also closes our dup'd fd
        return;
    }

    m_title = loadTag(tag, "TIT2"); // work title
    if (m_title == "") m_title = loadTag(tag, "TIT1");
    if (m_title == "") SVDEBUG << "MP3FileReader::loadTags: No title found" << endl;

    m_maker = loadTag(tag, "TPE1"); // "lead artist"
    if (m_maker == "") m_maker = loadTag(tag, "TPE2");
    if (m_maker == "") SVDEBUG << "MP3FileReader::loadTags: No artist/maker found" << endl;

    for (unsigned int i = 0; i < tag->nframes; ++i) {
        if (tag->frames[i]) {
            QString value = loadTag(tag, tag->frames[i]->id);
            if (value != "") {
                m_tags[tag->frames[i]->id] = value;
            }
        }
    }

    id3_file_close(file); // also closes our dup'd fd

#else
    SVDEBUG << "MP3FileReader::loadTags: ID3 tag support not compiled in" << endl;
#endif
}

QString
MP3FileReader::loadTag(void *vtag, const char *name)
{
#ifdef HAVE_ID3TAG
    id3_tag *tag = (id3_tag *)vtag;

    id3_frame *frame = id3_tag_findframe(tag, name, 0);
    if (!frame) {
        SVDEBUG << "MP3FileReader::loadTag: No \"" << name << "\" frame found in ID3 tag" << endl;
        return "";
    }
        
    if (frame->nfields < 2) {
        cerr << "MP3FileReader::loadTag: WARNING: Not enough fields (" << frame->nfields << ") for \"" << name << "\" in ID3 tag" << endl;
        return "";
    }

    unsigned int nstrings = id3_field_getnstrings(&frame->fields[1]);
    if (nstrings == 0) {
        SVDEBUG << "MP3FileReader::loadTag: No strings for \"" << name << "\" in ID3 tag" << endl;
        return "";
    }

    id3_ucs4_t const *ustr = id3_field_getstrings(&frame->fields[1], 0);
    if (!ustr) {
        SVDEBUG << "MP3FileReader::loadTag: Invalid or absent data for \"" << name << "\" in ID3 tag" << endl;
        return "";
    }
        
    id3_utf8_t *u8str = id3_ucs4_utf8duplicate(ustr);
    if (!u8str) {
        SVDEBUG << "MP3FileReader::loadTag: ERROR: Internal error: Failed to convert UCS4 to UTF8 in ID3 tag" << endl;
        return "";
    }
        
    QString rv = QString::fromUtf8((const char *)u8str);
    free(u8str);

    SVDEBUG << "MP3FileReader::loadTag: Tag \"" << name << "\" -> \""
            << rv << "\"" << endl;

    return rv;

#else
    return "";
#endif
}

void
MP3FileReader::DecodeThread::run()
{
    if (!m_reader->decode(m_reader->m_fileBuffer, m_reader->m_fileBufferSize)) {
        m_reader->m_error = QString("Failed to decode file %1.").arg(m_reader->m_path);
    }

    delete[] m_reader->m_fileBuffer;
    m_reader->m_fileBuffer = nullptr;

    if (m_reader->m_sampleBuffer) {
        for (int c = 0; c < m_reader->m_channelCount; ++c) {
            delete[] m_reader->m_sampleBuffer[c];
        }
        delete[] m_reader->m_sampleBuffer;
        m_reader->m_sampleBuffer = nullptr;
    }

    if (m_reader->isDecodeCacheInitialised()) m_reader->finishDecodeCache();

    m_reader->m_done = true;
    m_reader->m_completion = 100;

    m_reader->endSerialised();
} 

bool
MP3FileReader::decode(void *mm, sv_frame_t sz)
{
    DecoderData data;
    struct mad_decoder decoder;

    data.start = (unsigned char const *)mm;
    data.length = sz;
    data.finished = false;
    data.reader = this;

    mad_decoder_init(&decoder,          // decoder to initialise
                     &data,             // our own data block for callbacks
                     input_callback,    // provides (entire) input to mad
                     nullptr,                 // checks header
                     filter_callback,   // filters frame before decoding
                     output_callback,   // receives decoded output
                     error_callback,    // handles decode errors
                     nullptr);                // "message_func"

    mad_decoder_run(&decoder, MAD_DECODER_MODE_SYNC);
    mad_decoder_finish(&decoder);

    SVDEBUG << "MP3FileReader: Decoding complete, decoded " << m_mp3FrameCount
            << " mp3 frames" << endl;
    
    m_done = true;
    return true;
}

enum mad_flow
MP3FileReader::input_callback(void *dp, struct mad_stream *stream)
{
    DecoderData *data = (DecoderData *)dp;

    if (!data->length) {
        data->finished = true;
        return MAD_FLOW_STOP;
    }

    unsigned char const *start = data->start;
    sv_frame_t length = data->length;

#ifdef HAVE_ID3TAG
    while (length > ID3_TAG_QUERYSIZE) {
        ssize_t taglen = id3_tag_query(start, ID3_TAG_QUERYSIZE);
        if (taglen <= 0) {
            break;
        }
        SVDEBUG << "MP3FileReader: ID3 tag length to skip: " << taglen << endl;
        start += taglen;
        length -= taglen;
    }
#endif

    mad_stream_buffer(stream, start, length);
    data->length = 0;

    return MAD_FLOW_CONTINUE;
}

enum mad_flow
MP3FileReader::filter_callback(void *dp,
                               struct mad_stream const *stream,
                               struct mad_frame *frame)
{
    DecoderData *data = (DecoderData *)dp;
    return data->reader->filter(stream, frame);
}

static string toMagic(unsigned long fourcc)
{
    string magic("....");
    for (int i = 0; i < 4; ++i) {
        magic[3-i] = char((fourcc >> (8*i)) & 0xff);
    }
    return magic;
}

enum mad_flow
MP3FileReader::filter(struct mad_stream const *stream,
                      struct mad_frame *)
{
    if (m_mp3FrameCount > 0) {
        // only handle info frame if it appears as first mp3 frame
        return MAD_FLOW_CONTINUE;
    }

    if (m_gaplessMode == GaplessMode::Gappy) {
        // Our non-gapless mode does not even filter out the Xing/LAME
        // frame. That's because the main reason non-gapless mode
        // exists is for backward compatibility with MP3FileReader
        // behaviour before the gapless support was added, so we even
        // need to keep the spurious 1152 samples resulting from
        // feeding Xing/LAME frame to the decoder as otherwise we'd
        // have different output from before.
        SVDEBUG << "MP3FileReader: Not gapless mode, not checking Xing/LAME frame"
                << endl;
        return MAD_FLOW_CONTINUE;
    }
    
    struct mad_bitptr ptr = stream->anc_ptr;
    string magic = toMagic(mad_bit_read(&ptr, 32));

    if (magic == "Xing" || magic == "Info") {

        SVDEBUG << "MP3FileReader: Found Xing/LAME metadata frame (magic = \""
                << magic << "\")" << endl;

        // All we want at this point is the LAME encoder delay and
        // padding values. We expect to see the Xing/Info magic (which
        // we've already read), then 116 bytes of Xing data, then LAME
        // magic, 5 byte version string, 12 bytes of LAME data that we
        // aren't currently interested in, then the delays encoded as
        // two 12-bit numbers into three bytes.
        //
        // (See gabriel.mp3-tech.org/mp3infotag.html)
        
        for (int skip = 0; skip < 116; ++skip) {
            (void)mad_bit_read(&ptr, 8);
        }

        magic = toMagic(mad_bit_read(&ptr, 32));

        if (magic == "LAME") {

            SVDEBUG << "MP3FileReader: Found LAME-specific metadata" << endl;

            for (int skip = 0; skip < 5 + 12; ++skip) {
                (void)mad_bit_read(&ptr, 8);
            }

            auto delay = mad_bit_read(&ptr, 12);
            auto padding = mad_bit_read(&ptr, 12);

            sv_frame_t delayToDrop = DEFAULT_DECODER_DELAY + delay;
            sv_frame_t paddingToDrop = padding - DEFAULT_DECODER_DELAY;
            if (paddingToDrop < 0) paddingToDrop = 0;

            SVDEBUG << "MP3FileReader: LAME encoder delay = " << delay
                    << ", padding = " << padding << endl;

            SVDEBUG << "MP3FileReader: Will be trimming " << delayToDrop
                    << " samples from start and " << paddingToDrop
                    << " from end" << endl;

            CodedAudioFileReader::setFramesToTrim(delayToDrop, paddingToDrop);
            
        } else {
            SVDEBUG << "MP3FileReader: Xing frame has no LAME metadata" << endl;
        }
            
        return MAD_FLOW_IGNORE;
        
    } else {
        return MAD_FLOW_CONTINUE;
    }
}

enum mad_flow
MP3FileReader::output_callback(void *dp,
                               struct mad_header const *header,
                               struct mad_pcm *pcm)
{
    DecoderData *data = (DecoderData *)dp;
    return data->reader->accept(header, pcm);
}

enum mad_flow
MP3FileReader::accept(struct mad_header const *header,
                      struct mad_pcm *pcm)
{
    int channels = pcm->channels;
    int frames = pcm->length;
    
    if (header) {
        m_bitrateNum = m_bitrateNum + double(header->bitrate);
        m_bitrateDenom ++;
    }

    if (frames < 1) return MAD_FLOW_CONTINUE;

    if (m_channelCount == 0) {

        m_fileRate = pcm->samplerate;
        m_channelCount = channels;

        SVDEBUG << "MP3FileReader::accept: file rate = " << pcm->samplerate
                << ", channel count = " << channels << ", about to init "
                << "decode cache" << endl;

        initialiseDecodeCache();

        if (m_cacheMode == CacheInTemporaryFile) {
//            SVDEBUG << "MP3FileReader::accept: channel count " << m_channelCount << ", file rate " << m_fileRate << ", about to start serialised section" << endl;
            startSerialised("MP3FileReader::Decode");
        }
    }
    
    if (m_bitrateDenom > 0) {
        double bitrate = m_bitrateNum / m_bitrateDenom;
        double duration = double(m_fileSize * 8) / bitrate;
        double elapsed = double(m_frameCount) / m_sampleRate;
        double percent = 100;
        if (duration > 0.0) percent = ((elapsed * 100.0) / duration);
        int p = int(percent);
        if (p < 1) p = 1;
        if (p > 99) p = 99;
        if (m_completion != p && m_reporter) {
            m_completion = p;
            m_reporter->setProgress(m_completion);
        }
    }

    if (m_cancelled) {
        SVDEBUG << "MP3FileReader: Decoding cancelled" << endl;
        return MAD_FLOW_STOP;
    }

    if (!isDecodeCacheInitialised()) {
        SVDEBUG << "MP3FileReader::accept: fallback case: file rate = " << pcm->samplerate
                << ", channel count = " << channels << ", about to init "
                << "decode cache" << endl;
        initialiseDecodeCache();
    }

    if (m_sampleBufferSize < size_t(frames)) {
        if (!m_sampleBuffer) {
            m_sampleBuffer = new float *[channels];
            for (int c = 0; c < channels; ++c) {
                m_sampleBuffer[c] = nullptr;
            }
        }
        for (int c = 0; c < channels; ++c) {
            delete[] m_sampleBuffer[c];
            m_sampleBuffer[c] = new float[frames];
        }
        m_sampleBufferSize = frames;
    }

    int activeChannels = int(sizeof(pcm->samples) / sizeof(pcm->samples[0]));

    for (int ch = 0; ch < channels; ++ch) {

        for (int i = 0; i < frames; ++i) {

            mad_fixed_t sample = 0;
            if (ch < activeChannels) {
                sample = pcm->samples[ch][i];
            }
            float fsample = float(sample) / float(MAD_F_ONE);
            
            m_sampleBuffer[ch][i] = fsample;
        }
    }

    addSamplesToDecodeCache(m_sampleBuffer, frames);

    ++m_mp3FrameCount;

    return MAD_FLOW_CONTINUE;
}

enum mad_flow
MP3FileReader::error_callback(void *dp,
                              struct mad_stream *stream,
                              struct mad_frame *)
{
    DecoderData *data = (DecoderData *)dp;

    sv_frame_t ix = stream->this_frame - data->start;
    
    if (stream->error == MAD_ERROR_LOSTSYNC &&
        (data->finished || ix >= data->length)) {
        // We are at end of file, losing sync is expected behaviour,
        // don't report it
        return MAD_FLOW_CONTINUE;
    }
    
    if (!data->reader->m_decodeErrorShown) {
        char buffer[256];
        snprintf(buffer, 255,
                 "MP3 decoding error 0x%04x (%s) at byte offset %lld",
                 stream->error, mad_stream_errorstr(stream), (long long int)ix);
        SVCERR << "Warning: in file \"" << data->reader->m_path << "\": "
               << buffer << " (continuing; will not report any further decode errors for this file)" << endl;
        data->reader->m_decodeErrorShown = true;
    }

    return MAD_FLOW_CONTINUE;
}

void
MP3FileReader::getSupportedExtensions(std::set<QString> &extensions)
{
    extensions.insert("mp3");
}

bool
MP3FileReader::supportsExtension(QString extension)
{
    std::set<QString> extensions;
    getSupportedExtensions(extensions);
    return (extensions.find(extension.toLower()) != extensions.end());
}

bool
MP3FileReader::supportsContentType(QString type)
{
    return (type == "audio/mpeg");
}

bool
MP3FileReader::supports(FileSource &source)
{
    return (supportsExtension(source.getExtension()) ||
            supportsContentType(source.getContentType()));
}


#endif
