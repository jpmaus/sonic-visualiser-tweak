/* -*- c-basic-offset: 4 indent-tabs-mode: nil -*-  vi:set ts=8 sts=4 sw=4: */

/*
    Sonic Visualiser
    An audio file viewer and annotation editor.
    Centre for Digital Music, Queen Mary, University of London.
    This file copyright 2006 QMUL.
    
    This program is free software; you can redistribute it and/or
    modify it under the terms of the GNU General Public License as
    published by the Free Software Foundation; either version 2 of the
    License, or (at your option) any later version.  See the file
    COPYING included with this distribution for more information.
*/

#include "PlaySpeedRangeMapper.h"

#include <iostream>
#include <cmath>

// PlaySpeedRangeMapper maps a position in the range [0,120] on to a
// play speed factor on a logarithmic scale in the range 0.125 ->
// 8. This ensures that the desirable speed factors 0.25, 0.5, 1, 2,
// and 4 are all mapped to exact positions (respectively 20, 40, 60,
// 80, 100).

// Note that the "factor" referred to below is a play speed factor
// (higher = faster, 1.0 = normal speed), the "value" is a percentage
// (higher = faster, 100 = normal speed), and the "position" is an
// integer step on the dial's scale (0-120, 60 = centre).

PlaySpeedRangeMapper::PlaySpeedRangeMapper() :
    m_minpos(0),
    m_maxpos(120)
{
}

int
PlaySpeedRangeMapper::getPositionForValue(double value) const
{
    // value is percent
    double factor = getFactorForValue(value);
    int position = getPositionForFactor(factor);
    return position;
}

int
PlaySpeedRangeMapper::getPositionForValueUnclamped(double value) const
{
    // We don't really provide this
    return getPositionForValue(value);
}

double
PlaySpeedRangeMapper::getValueForPosition(int position) const
{
    double factor = getFactorForPosition(position);
    double pc = getValueForFactor(factor);
    return pc;
}

double
PlaySpeedRangeMapper::getValueForPositionUnclamped(int position) const
{
    // We don't really provide this
    return getValueForPosition(position);
}

double
PlaySpeedRangeMapper::getValueForFactor(double factor) const
{
    return factor * 100.0;
}

double
PlaySpeedRangeMapper::getFactorForValue(double value) const
{
    return value / 100.0;
}

int
PlaySpeedRangeMapper::getPositionForFactor(double factor) const
{
    if (factor == 0) return m_minpos;
    int pos = int(lrint((log2(factor) + 3.0) * 20.0));
    if (pos < m_minpos) pos = m_minpos;
    if (pos > m_maxpos) pos = m_maxpos;
    return pos;
}

double
PlaySpeedRangeMapper::getFactorForPosition(int position) const
{
    return pow(2.0, double(position) * 0.05 - 3.0);
}

QString
PlaySpeedRangeMapper::getUnit() const
{
    return "%";
}
