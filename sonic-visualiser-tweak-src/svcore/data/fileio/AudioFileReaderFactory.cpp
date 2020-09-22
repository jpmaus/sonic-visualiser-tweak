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

#include "AudioFileReaderFactory.h"

#include "WavFileReader.h"
#include "DecodingWavFileReader.h"
#include "MP3FileReader.h"
#include "BQAFileReader.h"
#include "AudioFileSizeEstimator.h"

#include "base/StorageAdviser.h"

#include <QString>
#include <QFileInfo>
#include <iostream>

using namespace std;

QString
AudioFileReaderFactory::getKnownExtensions()
{
    set<QString> extensions;

    WavFileReader::getSupportedExtensions(extensions);
#ifdef HAVE_MAD
    MP3FileReader::getSupportedExtensions(extensions);
#endif
    BQAFileReader::getSupportedExtensions(extensions);

    QString rv;
    for (set<QString>::const_iterator i = extensions.begin();
         i != extensions.end(); ++i) {
        if (i != extensions.begin()) rv += " ";
        rv += "*." + *i;
    }

    return rv;
}

bool
AudioFileReaderFactory::isSupported(FileSource source)
{
#ifdef HAVE_MAD
    if (MP3FileReader::supports(source)) {
        return true;
    }
#endif
    if (WavFileReader::supports(source)) {
        return true;
    }
    if (BQAFileReader::supports(source)) {
        return true;
    }
    return false;
}

AudioFileReader *
AudioFileReaderFactory::createReader(FileSource source,
                                     Parameters params,
                                     ProgressReporter *reporter)
{
    QString err;

    SVDEBUG << "AudioFileReaderFactory: url \"" << source.getLocation() << "\": requested rate: " << params.targetRate << (params.targetRate == 0 ? " (use source rate)" : "") << endl;
    SVDEBUG << "AudioFileReaderFactory: local filename \"" << source.getLocalFilename() << "\", content type \"" << source.getContentType() << "\"" << endl;

    if (!source.isOK()) {
        SVDEBUG << "AudioFileReaderFactory::createReader(\"" << source.getLocation() << "\": Failed to retrieve source (transmission error?): " << source.getErrorString() << endl;
        return nullptr;
    }

    if (!source.isAvailable()) {
        SVDEBUG << "AudioFileReaderFactory::createReader(\"" << source.getLocation() << "\": Source not found" << endl;
        return nullptr;
    }

    AudioFileReader *reader = nullptr;

    sv_samplerate_t targetRate = params.targetRate;
    bool normalised = (params.normalisation == Normalisation::Peak);
  
    sv_frame_t estimatedSamples = 
        AudioFileSizeEstimator::estimate(source, targetRate);
    
    CodedAudioFileReader::CacheMode cacheMode =
        CodedAudioFileReader::CacheInTemporaryFile;

    if (estimatedSamples > 0) {
        size_t kb = (estimatedSamples * sizeof(float)) / 1024;
        SVDEBUG << "AudioFileReaderFactory: checking where to potentially cache "
                << kb << "K of sample data" << endl;
        StorageAdviser::Recommendation rec =
            StorageAdviser::recommend(StorageAdviser::SpeedCritical, kb, kb);
        if ((rec & StorageAdviser::UseMemory) ||
            (rec & StorageAdviser::PreferMemory)) {
            SVDEBUG << "AudioFileReaderFactory: cacheing (if at all) in memory" << endl;
            cacheMode = CodedAudioFileReader::CacheInMemory;
        } else {
            SVDEBUG << "AudioFileReaderFactory: cacheing (if at all) on disc" << endl;
        }
    }
    
    CodedAudioFileReader::DecodeMode decodeMode =
        (params.threadingMode == ThreadingMode::Threaded ?
         CodedAudioFileReader::DecodeThreaded :
         CodedAudioFileReader::DecodeAtOnce);

    // We go through the set of supported readers at most twice: once
    // picking out only the readers that claim to support the given
    // file's extension or MIME type, and (if that fails) again
    // providing the file to every reader in turn regardless of
    // extension or type. (If none of the readers claim to support a
    // file, that may just mean its extension is missing or
    // misleading. We have to be confident that the reader won't open
    // just any old text file or whatever and pretend it's succeeded.)

    for (int any = 0; any <= 1; ++any) {

        bool anyReader = (any > 0);

        if (!anyReader) {
            SVDEBUG << "AudioFileReaderFactory: Checking whether any reader officially handles this source" << endl;
        } else {
            SVDEBUG << "AudioFileReaderFactory: Source not officially handled by any reader, trying again with each reader in turn"
                    << endl;
        }

#ifdef HAVE_MAD
        // Having said we'll try any reader on the second pass, we
        // actually don't want to try the mp3 reader for anything not
        // identified as an mp3 - it can't identify files by header,
        // it'll try to read any data and then fail with
        // synchronisation errors - causing misleading and potentially
        // alarming warning messages at the least
        if (!anyReader) {
            if (MP3FileReader::supports(source)) {

                MP3FileReader::GaplessMode gapless =
                    params.gaplessMode == GaplessMode::Gapless ?
                    MP3FileReader::GaplessMode::Gapless :
                    MP3FileReader::GaplessMode::Gappy;
            
                reader = new MP3FileReader
                    (source, decodeMode, cacheMode, gapless,
                     targetRate, normalised, reporter);

                if (reader->isOK()) {
                    SVDEBUG << "AudioFileReaderFactory: MP3 file reader is OK, returning it" << endl;
                    return reader;
                } else {
                    delete reader;
                }
            }
        }
#endif

        if (anyReader || WavFileReader::supports(source)) {

            reader = new WavFileReader(source);

            sv_samplerate_t fileRate = reader->getSampleRate();

            if (reader->isOK() &&
                (!reader->isQuicklySeekable() ||
                 normalised ||
                 (cacheMode == CodedAudioFileReader::CacheInMemory) ||
                 (targetRate != 0 && fileRate != targetRate))) {

                SVDEBUG << "AudioFileReaderFactory: WAV file reader rate: " << reader->getSampleRate() << ", normalised " << normalised << ", seekable " << reader->isQuicklySeekable() << ", in memory " << (cacheMode == CodedAudioFileReader::CacheInMemory) << ", creating decoding reader" << endl;
            
                delete reader;
                reader = new DecodingWavFileReader
                    (source,
                     decodeMode, cacheMode,
                     targetRate ? targetRate : fileRate,
                     normalised,
                     reporter);
            }

            if (reader->isOK()) {
                SVDEBUG << "AudioFileReaderFactory: WAV file reader is OK, returning it" << endl;
                return reader;
            } else {
                delete reader;
            }
        }
    
        if (anyReader || BQAFileReader::supports(source)) {

            reader = new BQAFileReader
                (source, decodeMode, cacheMode, 
                 targetRate, normalised, reporter);

            if (reader->isOK()) {
                SVDEBUG << "AudioFileReaderFactory: BQA reader is OK, returning it" << endl;
                return reader;
            } else {
                delete reader;
            }
        }
    }
    
    SVDEBUG << "AudioFileReaderFactory::Failed to create a reader for "
            << "url \"" << source.getLocation()
            << "\" (local filename \"" << source.getLocalFilename()
            << "\", content type \""
            << source.getContentType() << "\")" << endl;
    return nullptr;
}

