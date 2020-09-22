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

#include "AudioFileSizeEstimator.h"

#include "WavFileReader.h"

#include <QFile>

#include "base/Debug.h"

sv_frame_t
AudioFileSizeEstimator::estimate(FileSource source,
                                 sv_samplerate_t targetRate)
{
    sv_frame_t estimate = 0;
    
    SVDEBUG << "AudioFileSizeEstimator: Sample count estimate requested for file \""
            << source.getLocalFilename() << "\"" << endl;

    // Most of our file readers don't know the sample count until
    // after they've finished decoding. This is an exception:

    WavFileReader *reader = new WavFileReader(source);
    if (reader->isOK() &&
        reader->getChannelCount() > 0 &&
        reader->getFrameCount() > 0) {
        sv_frame_t samples =
            reader->getFrameCount() * reader->getChannelCount();
        sv_samplerate_t rate = reader->getSampleRate();
        if (targetRate != 0.0 && targetRate != rate) {
            samples = sv_frame_t(double(samples) * targetRate / rate);
        }
        SVDEBUG << "AudioFileSizeEstimator: WAV file reader accepts this file, reports "
                << samples << " samples" << endl;
        estimate = samples;
    } else {
        SVDEBUG << "AudioFileSizeEstimator: WAV file reader doesn't like this file, "
                << "estimating from file size and extension instead" << endl;
    }

    delete reader;
    reader = nullptr;

    if (estimate == 0) {

        // The remainder just makes an estimate based on the file size
        // and extension. We don't even know its sample rate at this
        // point, so the following is a wild guess.
        
        double rateRatio = 1.0;
        if (targetRate != 0.0) {
            rateRatio = targetRate / 44100.0;
        }
    
        QString extension = source.getExtension();

        source.waitForData();
        if (!source.isOK()) return 0;

        sv_frame_t sz = 0;

        {
            QFile f(source.getLocalFilename());
            if (f.open(QFile::ReadOnly)) {
                SVDEBUG << "AudioFileSizeEstimator: opened file, size is "
                        << f.size() << endl;
                sz = f.size();
                f.close();
            }
        }

        if (extension == "ogg" || extension == "oga" ||
            extension == "m4a" || extension == "mp3" ||
            extension == "wma" || extension == "opus") {

            // Usually a lossy file. Compression ratios can vary
            // dramatically, but don't usually exceed about 20x compared
            // to 16-bit PCM (e.g. a 128kbps mp3 has 11x ratio over WAV at
            // 44.1kHz). We can estimate the number of samples to be file
            // size x 20, divided by 2 as we're comparing with 16-bit PCM.

            estimate = sv_frame_t(double(sz) * 10 * rateRatio);
        }

        if (extension == "flac") {
        
            // FLAC usually takes up a bit more than half the space of
            // 16-bit PCM. So the number of 16-bit samples is roughly the
            // same as the file size in bytes. As above, let's be
            // conservative.

            estimate = sv_frame_t(double(sz) * 1.2 * rateRatio);
        }

        SVDEBUG << "AudioFileSizeEstimator: for extension \""
                << extension << "\", estimate = " << estimate << " samples" << endl;
    }
    
    return estimate;
}

