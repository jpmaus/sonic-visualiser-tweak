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

#include "HorizontalFrequencyScale.h"
#include "HorizontalScaleProvider.h"
#include "LayerGeometryProvider.h"

#include "base/ScaleTickIntervals.h"

#include <QPainter>

#include <cmath>

int
HorizontalFrequencyScale::getHeight(LayerGeometryProvider *,
                                    QPainter &paint)
{
    return paint.fontMetrics().height() + 10;
}

void
HorizontalFrequencyScale::paintScale(LayerGeometryProvider *v,
                                     const HorizontalScaleProvider *p,
                                     QPainter &paint,
                                     QRect r,
                                     bool logarithmic)
{
    int x0 = r.x(), y0 = r.y(), x1 = r.x() + r.width(), y1 = r.y() + r.height();

    paint.drawLine(x0, y0, x1, y0);

    double f0 = p->getFrequencyForX(v, x0 ? x0 : 1);
    double f1 = p->getFrequencyForX(v, x1);

    int n = 20;

    auto ticks =
        logarithmic ?
        ScaleTickIntervals::logarithmic({ f0, f1, n }) :
        ScaleTickIntervals::linear({ f0, f1, n });

    n = int(ticks.size());

    int marginx = -1;

    for (int i = 0; i < n; ++i) {

        // Qt 5.13 deprecates QFontMetrics::width(), but its suggested
        // replacement (horizontalAdvance) was only added in Qt 5.11
        // which is too new for us
#pragma GCC diagnostic ignored "-Wdeprecated-declarations"
        
        double val = ticks[i].value;
        QString label = QString::fromStdString(ticks[i].label);
        int tw = paint.fontMetrics().width(label);
        
        int x = int(round(p->getXForFrequency(v, val)));

        if (x < marginx) continue;
        
        //!!! todo: pixel scaling (here & elsewhere in these classes)
        
        paint.drawLine(x, y0, x, y1);

        paint.drawText(x + 5, y0 + paint.fontMetrics().ascent() + 5, label);

        marginx = x + tw + 10;
    }
}

