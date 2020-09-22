/* -*- c-basic-offset: 4 indent-tabs-mode: nil -*-  vi:set ts=8 sts=4 sw=4: */

/*
    bqresample

    A small library wrapping various audio sample rate conversion
    implementations in C++.

    Copyright 2007-2015 Particular Programs Ltd.

    Permission is hereby granted, free of charge, to any person
    obtaining a copy of this software and associated documentation
    files (the "Software"), to deal in the Software without
    restriction, including without limitation the rights to use, copy,
    modify, merge, publish, distribute, sublicense, and/or sell copies
    of the Software, and to permit persons to whom the Software is
    furnished to do so, subject to the following conditions:

    The above copyright notice and this permission notice shall be
    included in all copies or substantial portions of the Software.

    THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND,
    EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF
    MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND
    NONINFRINGEMENT. IN NO EVENT SHALL THE AUTHORS BE LIABLE FOR
    ANY CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER IN AN ACTION OF
    CONTRACT, TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN CONNECTION
    WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.

    Except as contained in this notice, the names of Chris Cannam and
    Particular Programs Ltd shall not be used in advertising or
    otherwise to promote the sale, use or other dealings in this
    Software without prior written authorization.
*/

#ifndef BQ_RESAMPLER_H
#define BQ_RESAMPLER_H

#include "bqvec/Restrict.h"

namespace breakfastquay {

class Resampler
{
public:
    enum Quality { Best, FastestTolerable, Fastest };
    enum Exception { ImplementationError };

    struct Parameters {

        /**
         * Resampler filter quality level.
         */
        Quality quality;

        /** 
         * Rate of expected input prior to resampling: may be used to
         * determine the filter bandwidth for the quality setting. If
         * you don't know what this will be, you can provide an
         * arbitrary rate (such as the default) and the resampler will
         * work fine, but quality may not be as designed.
         */
        double initialSampleRate;

        /** 
         * Bound on the maximum incount size that may be passed to the
         * resample function before the resampler needs to reallocate
         * its internal buffers.
         */
        int maxBufferSize;

        /**
         * Debug output level, from 0 to 3. Controls the amount of
         * debug information printed to stderr.
         */
        int debugLevel;

        Parameters() :
            quality(FastestTolerable),
            initialSampleRate(44100),
            maxBufferSize(0),
            debugLevel(0) { }
    };
    
    /**
     * Construct a resampler to process the given number of channels,
     * with the given quality level, initial sample rate, and other
     * parameters.
     */
    Resampler(Parameters parameters, int channels);
    
    ~Resampler();

    /**
     * Resample the given multi-channel buffers, where incount is the
     * number of frames in the input buffers and outspace is the space
     * available in the output buffers. Generally you want outspace to
     * be at least ceil(incount * ratio).
     *
     * Returns the number of frames written to the output
     * buffers. This may be smaller than outspace even where the ratio
     * suggests otherwise, particularly at the start of processing
     * where there may be a filter tail to allow for.
     */
#ifdef __GNUC__
    __attribute__((warn_unused_result))
#endif
    int resample(float *const BQ_R__ *const BQ_R__ out,
                 int outspace,
                 const float *const BQ_R__ *const BQ_R__ in,
                 int incount,
                 double ratio,
                 bool final = false);

    /**
     * Resample the given interleaved buffer, where incount is the
     * number of frames in the input buffer (i.e. it has incount *
     * getChannelCount() samples) and outspace is the space available
     * in frames in the output buffer (i.e. it has space for at least
     * outspace * getChannelCount() samples). Generally you want
     * outspace to be at least ceil(incount * ratio).
     *
     * Returns the number of frames written to the output buffer. This
     * may be smaller than outspace even where the ratio suggests
     * otherwise, particularly at the start of processing where there
     * may be a filter tail to allow for.
     */
#ifdef __GNUC__
    __attribute__((warn_unused_result))
#endif
    int resampleInterleaved(float *const BQ_R__ out,
                            int outspace,
                            const float *const BQ_R__ in,
                            int incount,
                            double ratio,
                            bool final = false);

    int getChannelCount() const;

    void reset();

    class Impl;

protected:
    Impl *d;
    int m_method;
};

}

#endif
