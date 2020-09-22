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

#include "AudioFileReader.h"

using std::vector;

vector<floatvec_t>
AudioFileReader::getDeInterleavedFrames(sv_frame_t start, sv_frame_t count) const
{
    floatvec_t interleaved = getInterleavedFrames(start, count);
    
    int channels = getChannelCount();
    if (channels == 1) return { interleaved };
    
    sv_frame_t rc = interleaved.size() / channels;

    vector<floatvec_t> frames(channels, floatvec_t(rc, 0.f));
    
    for (int c = 0; c < channels; ++c) {
        for (sv_frame_t i = 0; i < rc; ++i) {
            frames[c][i] = interleaved[i * channels + c];
        }
    }

    return frames;
}

