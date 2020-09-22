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

#ifndef AUDIO_FILE_SIZE_ESTIMATOR_H
#define AUDIO_FILE_SIZE_ESTIMATOR_H

#include "base/BaseTypes.h"
#include "data/fileio/FileSource.h"

/**
 * Estimate the number of samples in an audio file. For many
 * compressed files this returns only a very approximate estimate,
 * based on a rough estimate of compression ratio. Initially we're
 * only aiming for a conservative estimate for purposes like "will
 * this file fit in memory?" (and if unsure, say no).
 */
class AudioFileSizeEstimator
{
public:
    /**
     * Return an estimate of the number of samples (across all
     * channels) in the given audio file, once it has been decoded and
     * (if applicable) resampled to the given rate.
     *
     * This function is intended to be reasonably fast -- it may open
     * the file, but it should not do any decoding. (However, if the
     * file source is remote, it will probably be downloaded in its
     * entirety before anything can be estimated.)
     *
     * The returned value is an estimate, and is deliberately usually
     * on the high side. If the estimator has no idea at all, this
     * will return 0.
     */
    static sv_frame_t estimate(FileSource source,
                               sv_samplerate_t targetRate = 0);
};

#endif
