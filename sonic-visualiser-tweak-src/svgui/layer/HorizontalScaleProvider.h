/* -*- c-basic-offset: 4 indent-tabs-mode: nil -*-  vi:set ts=8 sts=4 sw=4: */

/*
    Sonic Visualiser
    An audio file viewer and annotation editor.
    Centre for Digital Music, Queen Mary, University of London.
    This file copyright 2006-2018 Chris Cannam and QMUL.
    
    This program is free software; you can redistribute it and/or
    modify it under the terms of the GNU General Public License as
    published by the Free Software Foundation; either version 2 of the
    License, or (at your option) any later version.  See the file
    COPYING included with this distribution for more information.
*/

#ifndef SV_HORIZONTAL_SCALE_PROVIDER_H
#define SV_HORIZONTAL_SCALE_PROVIDER_H

class LayerGeometryProvider;

/**
 * Interface to be implemented by objects, such as layers or objects
 * they delegate to, in which the X axis corresponds to frequency. For
 * example, SpectrumLayer.
 */
class HorizontalScaleProvider
{
public:
    virtual double getFrequencyForX(const LayerGeometryProvider *,
                                    double x)
        const = 0;
    
    virtual double getXForFrequency(const LayerGeometryProvider *,
                                    double freq)
        const = 0;
};

#endif
