/* -*- c-basic-offset: 4 indent-tabs-mode: nil -*-  vi:set ts=8 sts=4 sw=4: */

/*
    Sonic Visualiser
    An audio file viewer and annotation editor.
    Centre for Digital Music, Queen Mary, University of London.
    This file copyright 2006-2013 Chris Cannam and QMUL.
    
    This program is free software; you can redistribute it and/or
    modify it under the terms of the GNU General Public License as
    published by the Free Software Foundation; either version 2 of the
    License, or (at your option) any later version.  See the file
    COPYING included with this distribution for more information.
*/

#ifndef COLOUR_SCALE_LAYER_H
#define COLOUR_SCALE_LAYER_H

#include <QString>
#include <QColor>

class LayerGeometryProvider;

/**
 * Interface for layers in which a colour scale represents (or can
 * sometimes represent, depending on the display mode) the sample
 * value. For example, TimeValueLayer uses colour scale when in
 * segment mode and so provides this interface for use by the
 * LogColourScale or LinearColourScale scale renderers.
 */
class ColourScaleLayer
{
public:
    virtual QString getScaleUnits() const = 0;
    virtual QColor getColourForValue(LayerGeometryProvider *v, double value) const = 0;
};

#endif

