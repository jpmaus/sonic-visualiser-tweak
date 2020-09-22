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

#ifndef SV_LINEAR_NUMERICAL_SCALE_H
#define SV_LINEAR_NUMERICAL_SCALE_H

#include <QRect>

class QPainter;
class LayerGeometryProvider;
class VerticalScaleLayer;

class LinearNumericalScale
{
public:
    int getWidth(LayerGeometryProvider *v, QPainter &paint);

    void paintVertical
    (LayerGeometryProvider *v, const VerticalScaleLayer *layer,
     QPainter &paint, int x0, double minf, double maxf);
};

#endif

