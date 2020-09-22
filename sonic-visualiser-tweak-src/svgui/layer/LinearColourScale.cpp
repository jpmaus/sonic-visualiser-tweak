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

#include "LinearColourScale.h"
#include "ColourScaleLayer.h"

#include <QPainter>

#include <cmath>

#include "LayerGeometryProvider.h"

int
LinearColourScale::getWidth(LayerGeometryProvider *,
                            QPainter &paint)
{
    // Qt 5.13 deprecates QFontMetrics::width(), but its suggested
    // replacement (horizontalAdvance) was only added in Qt 5.11
    // which is too new for us
#pragma GCC diagnostic ignored "-Wdeprecated-declarations"

    return paint.fontMetrics().width("-000.00") + 15;
}

void
LinearColourScale::paintVertical(LayerGeometryProvider *v,
                                 const ColourScaleLayer *layer,
                                 QPainter &paint,
                                 int /* x0 */,
                                 double min,
                                 double max)
{
    int h = v->getPaintHeight();

    int n = 10;

    double val = min;
    double inc = (max - val) / n;
    
    const int buflen = 40;
    char buffer[buflen];

    int boxx = 5, boxy = 5;
    if (layer->getScaleUnits() != "") {
        boxy += paint.fontMetrics().height();
    }
    int boxw = 10, boxh = h - boxy - 5;

    int tx = 5 + boxx + boxw;
    paint.drawRect(boxx, boxy, boxw, boxh);

    paint.save();
    for (int y = 0; y < boxh; ++y) {
        double val = ((boxh - y) * (max - min)) / boxh + min;
        paint.setPen(layer->getColourForValue(v, val));
        paint.drawLine(boxx + 1, y + boxy + 1, boxx + boxw, y + boxy + 1);
    }
    paint.restore();

//    double round = 1.f;
    int dp = 0;
    if (inc > 0) {
        int prec = int(trunc(log10(inc)));
        prec -= 1;
        if (prec < 0) dp = -prec;
//        round = powf(10.f, prec);
//#ifdef DEBUG_TIME_VALUE_LAYER
//        cerr << "inc = " << inc << ", round = " << round << ", dp = " << dp << endl;
//#endif
    }

    for (int i = 0; i < n; ++i) {

        int y, ty;

        y = boxy + int(boxh - ((val - min) * boxh) / (max - min));

        ty = y - paint.fontMetrics().height() +
            paint.fontMetrics().ascent() + 2;

        snprintf(buffer, buflen, "%.*f", dp, val);
        QString label = QString(buffer);

        paint.drawLine(boxx + boxw - boxw/3, y, boxx + boxw, y);
        paint.drawText(tx, ty, label);

        val += inc;
    }
}
