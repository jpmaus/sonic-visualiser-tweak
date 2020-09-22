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

#ifndef SV_PLAY_SPEED_RANGE_MAPPER_H
#define SV_PLAY_SPEED_RANGE_MAPPER_H

#include "base/RangeMapper.h"

class PlaySpeedRangeMapper : public RangeMapper
{
public:
    PlaySpeedRangeMapper();

    int getMinPosition() const { return m_minpos; }
    int getMaxPosition() const { return m_maxpos; }
    
    int getPositionForValue(double value) const override;
    int getPositionForValueUnclamped(double value) const override;

    double getValueForPosition(int position) const override;
    double getValueForPositionUnclamped(int position) const override;

    int getPositionForFactor(double factor) const;
    double getValueForFactor(double factor) const;

    double getFactorForPosition(int position) const;
    double getFactorForValue(double value) const;

    QString getUnit() const override;
    
protected:
    int m_minpos;
    int m_maxpos;
};


#endif
