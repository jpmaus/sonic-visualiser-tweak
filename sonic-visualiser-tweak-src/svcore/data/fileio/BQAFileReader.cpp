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

#include "BQAFileReader.h"

#include <bqaudiostream/AudioReadStreamFactory.h>
#include <bqaudiostream/AudioReadStream.h>
#include <bqaudiostream/Exceptions.h>

#include "base/Profiler.h"
#include "base/ProgressReporter.h"

#include <QFileInfo>

using namespace std;

BQAFileReader::BQAFileReader(FileSource source,
			     DecodeMode decodeMode,
			     CacheMode mode,
			     sv_samplerate_t targetRate,
			     bool normalised,
			     ProgressReporter *reporter) :
    CodedAudioFileReader(mode, targetRate, normalised),
    m_source(source),
    m_path(source.getLocalFilename()),
    m_cancelled(false),
    m_completion(0),
    m_reporter(reporter),
    m_decodeThread(0)
{
    SVDEBUG << "BQAFileReader: local path: \"" << m_path
            << "\", decode mode: " << decodeMode << " ("
            << (decodeMode == DecodeAtOnce ? "DecodeAtOnce" : "DecodeThreaded")
            << ")" << endl;

    m_channelCount = 0;
    m_fileRate = 0;

    Profiler profiler("BQAFileReader::BQAFileReader");

    try {
	m_stream = breakfastquay::AudioReadStreamFactory::createReadStream
	    (m_path.toUtf8().data());
    } catch (const std::exception &e) {
	m_error = e.what();
        SVDEBUG << "BQAFileReader: createReadStream failed: " << m_error << endl;
	m_stream = 0;
	return;
    }

    m_channelCount = int(m_stream->getChannelCount());
    m_fileRate = sv_samplerate_t(m_stream->getSampleRate());
    m_title = QString::fromUtf8(m_stream->getTrackName().c_str());
    m_maker = QString::fromUtf8(m_stream->getArtistName().c_str());

    initialiseDecodeCache();

    if (decodeMode == DecodeAtOnce) {

        if (m_reporter) {
            connect(m_reporter, SIGNAL(cancelled()), this, SLOT(cancelled()));
            m_reporter->setMessage
                (tr("Decoding %1...").arg(QFileInfo(m_path).fileName()));
        }

        sv_frame_t blockSize = 65536;
        floatvec_t block(blockSize * m_channelCount, 0.f);

	while (true) {
	    try {
		sv_frame_t retrieved = 
		    m_stream->getInterleavedFrames(blockSize, block.data());

		addSamplesToDecodeCache(block.data(), retrieved);

		if (retrieved < blockSize) {
		    break;
		}
	    } catch (const breakfastquay::InvalidFileFormat &f) {
		m_error = f.what();
                SVDEBUG << "BQAFileReader: init failed: " << m_error << endl;
		break;
	    }

	    if (m_cancelled) break;
        }

        if (isDecodeCacheInitialised()) finishDecodeCache();
        endSerialised();

        if (m_reporter) m_reporter->setProgress(100);

        delete m_stream;
        m_stream = 0;

    } else {

        if (m_reporter) m_reporter->setProgress(100);

        m_decodeThread = new DecodeThread(this);
        m_decodeThread->start();
    }
}

BQAFileReader::~BQAFileReader()
{
    if (m_decodeThread) {
        m_cancelled = true;
        m_decodeThread->wait();
        delete m_decodeThread;
    }
    
    delete m_stream;
}

void
BQAFileReader::cancelled()
{
    m_cancelled = true;
}

void
BQAFileReader::DecodeThread::run()
{
    if (m_reader->m_cacheMode == CacheInTemporaryFile) {
        m_reader->startSerialised("BQAFileReader::Decode");
    }

    sv_frame_t blockSize = 65536;
    floatvec_t block(blockSize * m_reader->getChannelCount(), 0.f);
    
    while (true) {
	try {
	    sv_frame_t retrieved = 
		m_reader->m_stream->getInterleavedFrames
		(blockSize, block.data());

	    m_reader->addSamplesToDecodeCache(block.data(), retrieved);

	    if (retrieved < blockSize) {
		break;
	    }
	} catch (const breakfastquay::InvalidFileFormat &f) {
	    m_reader->m_error = f.what();
            SVDEBUG << "BQAFileReader: decode failed: " << m_reader->m_error << endl;
	    break;
	}

	if (m_reader->m_cancelled) break;
    }
    
    if (m_reader->isDecodeCacheInitialised()) m_reader->finishDecodeCache();
    m_reader->m_completion = 100;

    m_reader->endSerialised();

    delete m_reader->m_stream;
    m_reader->m_stream = 0;
} 

void
BQAFileReader::getSupportedExtensions(set<QString> &extensions)
{
    vector<string> exts = 
        breakfastquay::AudioReadStreamFactory::getSupportedFileExtensions();
    for (auto e: exts) {
        extensions.insert(QString::fromUtf8(e.c_str()));
    }
}

bool
BQAFileReader::supportsExtension(QString extension)
{
    set<QString> extensions;
    getSupportedExtensions(extensions);
    return (extensions.find(extension.toLower()) != extensions.end());
}

bool
BQAFileReader::supportsContentType(QString type)
{
    // extremely optimistic, but it's better than rejecting everything
    //!!! todo: be more sensible
    return (type.startsWith("audio/"));
}

bool
BQAFileReader::supports(FileSource &source)
{
    return (supportsExtension(source.getExtension()) ||
            supportsContentType(source.getContentType()));
}

