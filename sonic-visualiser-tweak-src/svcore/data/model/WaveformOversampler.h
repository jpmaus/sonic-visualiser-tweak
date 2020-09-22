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

#ifndef SV_WAVEFORM_OVERSAMPLER_H
#define SV_WAVEFORM_OVERSAMPLER_H

#include "base/BaseTypes.h"

class DenseTimeValueModel;

/** Oversample the sample data from a DenseTimeValueModel by an
 *  integer factor, on the assumption that the model represents
 *  audio. Oversampling is carried out using a windowed sinc filter
 *  for a fixed 8x ratio with further linear interpolation to handle
 *  other ratios. The aim is not to provide the "best-sounding"
 *  interpolation, but to provide accurate and predictable projections
 *  of the theoretical waveform shape for display rendering without
 *  leaving decisions about interpolation up to a resampler library.
 */
class WaveformOversampler
{
public:
    /** Return an oversampled version of the audio data from the given
     *  source sample range. Will query sufficient source audio before
     *  and after the requested range (where available) to ensure an
     *  accurate-looking result after filtering. The returned vector
     *  will have sourceFrameCount * oversampleBy samples, except when
     *  truncated because the end of the model was reached.
     */
    static floatvec_t getOversampledData(const DenseTimeValueModel &source,
                                         int channel,
                                         sv_frame_t sourceStartFrame,
                                         sv_frame_t sourceFrameCount,
                                         int oversampleBy);

private:
    static floatvec_t getFixedRatioData(const DenseTimeValueModel &source,
                                        int channel,
                                        sv_frame_t sourceStartFrame,
                                        sv_frame_t sourceFrameCount);
    
    static int m_filterRatio;
    static floatvec_t m_filter;
};

#endif
