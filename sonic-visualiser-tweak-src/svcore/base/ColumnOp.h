/* -*- c-basic-offset: 4 indent-tabs-mode: nil -*-  vi:set ts=8 sts=4 sw=4: */

/*
    Sonic Visualiser
    An audio file viewer and annotation editor.
    Centre for Digital Music, Queen Mary, University of London.
    This file copyright 2006-2016 Chris Cannam and QMUL.
    
    This program is free software; you can redistribute it and/or
    modify it under the terms of the GNU General Public License as
    published by the Free Software Foundation; either version 2 of the
    License, or (at your option) any later version.  See the file
    COPYING included with this distribution for more information.
*/

#ifndef COLUMN_OP_H
#define COLUMN_OP_H

#include "BaseTypes.h"

#include <vector>

/**
 * Display normalization types for columns in e.g. grid plots.
 *
 * Max1 means to normalize to max value = 1.0.
 * Sum1 means to normalize to sum of values = 1.0.
 *
 * Range01 means to normalize such that the max value = 1.0 and the
 * min value (if different from the max value) = 0.0.
 *
 * Hybrid means normalize to max = 1.0 and then multiply by
 * log10 of the max value, to retain some difference between
 * levels of neighbouring columns.
 *
 * Area normalization is handled separately.
 */
enum class ColumnNormalization {
    None,
    Max1,
    Sum1,
    Range01,
    Hybrid
};

/**
 * Class containing static functions for simple operations on data
 * columns, for use by display layers.
 */
class ColumnOp
{
public:
    /** 
     * Column type. 
     */
    typedef std::vector<float> Column;

    /**
     * Scale the given column using the given gain multiplier.
     */
    static Column applyGain(const Column &in, double gain) {
        if (gain == 1.0) return in;
        Column out;
        out.reserve(in.size());
        for (auto v: in) out.push_back(float(v * gain));
        return out;
    }

    /**
     * Shift the values in the given column by the given offset.
     */
    static Column applyShift(const Column &in, float offset) {
        if (offset == 0.f) return in;
        Column out;
        out.reserve(in.size());
        for (auto v: in) out.push_back(v + offset);
        return out;
    }

    /**
     * Scale an FFT output downward by half the FFT size.
     */
    static Column fftScale(const Column &in, int fftSize);

    /**
     * Determine whether an index points to a local peak.
     */
    static bool isPeak(const Column &in, int ix) {
        if (!in_range_for(in, ix)) {
            return false;
        }
        if (ix == 0) {
            return in[0] >= in[1];
        }
        if (!in_range_for(in, ix+1)) {
            return in[ix] > in[ix-1];
        }
        if (in[ix] < in[ix+1]) {
            return false;
        }
        if (in[ix] <= in[ix-1]) {
            return false;
        }
        return true;
    }

    /**
     * Return a column containing only the local peak values (all
     * others zero).
     */
    static Column peakPick(const Column &in);

    /**
     * Return a column normalized from the input column according to
     * the given normalization scheme.
     *
     * Note that the sum or max (as appropriate) used for
     * normalisation will be calculated from the absolute values of
     * the column elements, should any of them be negative.
     */
    static Column normalize(const Column &in, ColumnNormalization n);
    
    /**
     * Distribute the given column into a target vector of a different
     * size, optionally using linear interpolation. The binfory vector
     * contains a mapping from y coordinate (i.e. index into the
     * target vector) to bin (i.e. index into the source column). The
     * source column ("in") may be a partial column; it's assumed to
     * contain enough bins to span the destination range, starting
     * with the bin of index minbin.
     */
    static Column distribute(const Column &in,
                             int h,
                             const std::vector<double> &binfory,
                             int minbin,
                             bool interpolate);

};

#endif

