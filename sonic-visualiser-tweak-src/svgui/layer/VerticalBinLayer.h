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

#ifndef VERTICAL_BIN_LAYER_H
#define VERTICAL_BIN_LAYER_H

#include "SliceableLayer.h"

/**
 * Interface for layers in which the Y axis corresponds to bin number
 * rather than scale value. Colour3DPlotLayer and SpectrogramLayer are
 * obvious examples.  Conceptually these are always SliceableLayers as
 * well, and this subclasses from SliceableLayer to avoid a big
 * inheritance mess.
 */
class VerticalBinLayer : public SliceableLayer
{
public:
    /**
     * Return the y coordinate at which the given bin "starts"
     * (i.e. at the bottom of the bin, if the given bin is an integer
     * and the vertical scale is the usual way up). Bin number may be
     * fractional, to obtain a position part-way through a bin.
     */
    virtual double getYForBin(const LayerGeometryProvider *, double bin) const = 0;

    /**
     * As getYForBin, but rounding to integer values.
     */
    virtual int getIYForBin(const LayerGeometryProvider *v, int bin) const {
        return int(round(getYForBin(v, bin)));
    }
    
    /**
     * Return the bin number, possibly fractional, at the given y
     * coordinate. Note that the whole numbers occur at the positions
     * at which the bins "start" (i.e. the bottom of the visible bin,
     * if the vertical scale is the usual way up).
     */
    virtual double getBinForY(const LayerGeometryProvider *, double y) const = 0;

    /**
     * As getBinForY, but rounding to integer values.
     */
    virtual int getIBinForY(const LayerGeometryProvider *v, int y) const {
        return int(floor(getBinForY(v, y)));
    }
};

#endif

