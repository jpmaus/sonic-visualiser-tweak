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

#include "LogNumericalScale.h"
#include "VerticalScaleLayer.h"
#include "LayerGeometryProvider.h"

#include "base/LogRange.h"
#include "base/ScaleTickIntervals.h"

#include <QPainter>

#include <cmath>

int
LogNumericalScale::getWidth(LayerGeometryProvider *,
                            QPainter &paint)
{
    // Qt 5.13 deprecates QFontMetrics::width(), but its suggested
    // replacement (horizontalAdvance) was only added in Qt 5.11
    // which is too new for us
#pragma GCC diagnostic ignored "-Wdeprecated-declarations"

    return paint.fontMetrics().width("-000.00") + 10;
}

void
LogNumericalScale::paintVertical(LayerGeometryProvider *v,
                                 const VerticalScaleLayer *layer,
                                 QPainter &paint,
                                 int x0,
                                 double minlog,
                                 double maxlog)
{
    int n = 10;
    auto ticks = ScaleTickIntervals::logarithmicAlready({ minlog, maxlog, n });
    n = int(ticks.size());

    int w = getWidth(v, paint) + x0;

    int prevy = -1;
                
    for (int i = 0; i < n; ++i) {

        int y, ty;
        bool drawText = true;

        if (i == n-1 &&
            v->getPaintHeight() < paint.fontMetrics().height() * (n*2)) {
            if (layer->getScaleUnits() != "") drawText = false;
        }

        double val = ticks[i].value;
        QString label = QString::fromStdString(ticks[i].label);

        y = layer->getYForValue(v, val);

        ty = y - paint.fontMetrics().height() + paint.fontMetrics().ascent() + 2;
        
        if (prevy >= 0 && (prevy - y) < paint.fontMetrics().height()) {
            continue;
        }

        paint.drawLine(w - 5, y, w, y);

        if (drawText) {
            paint.drawText(w - paint.fontMetrics().width(label) - 6,
                           ty, label);
        }

        prevy = y;
    }
}

