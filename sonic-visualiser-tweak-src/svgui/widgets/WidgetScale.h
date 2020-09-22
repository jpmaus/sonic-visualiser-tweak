/* -*- c-basic-offset: 4 indent-tabs-mode: nil -*-  vi:set ts=8 sts=4 sw=4: */

/*
    Sonic Visualiser
    An audio file viewer and annotation editor.
    Centre for Digital Music, Queen Mary, University of London.
    
    This program is free software; you can redistribute it and/or
    modify it under the terms of the GNU General Public License as
    published by the Free Software Foundation; either version 2 of the
    License, or (at your option) any later version.  See the file
    COPYING included with this distribution for more information.
*/

#ifndef SV_WIDGET_SCALE_H
#define SV_WIDGET_SCALE_H

#include <QFont>
#include <QFontMetrics>

#include "base/Debug.h"

class WidgetScale
{
public:   
    /**
     * Take a "design pixel" size and scale it for the actual
     * display. This is relevant to hi-dpi systems that do not do
     * pixel doubling (i.e. Windows and Linux rather than OS/X).
     */
    static int scalePixelSize(int pixels) {

        static double ratio = 0.0;
        if (ratio == 0.0) {
            double baseEm;
#ifdef Q_OS_MAC
            baseEm = 17.0;
#else
            baseEm = 15.0;
#endif
            double em = QFontMetrics(QFont()).height();
            ratio = em / baseEm;
            SVDEBUG << "WidgetScale::scalePixelSize: baseEm = " << baseEm
                    << ", platform default font height = " << em
                    << ", resulting scale factor = " << ratio << endl;
            if (ratio < 1.0) {
                SVDEBUG << "WidgetScale::scalePixelSize: rounding up to 1.0"
                        << endl;
                ratio = 1.0;
            }
        }

        int scaled = int(pixels * ratio + 0.5);
        if (pixels != 0 && scaled == 0) scaled = 1;
        return scaled;
    }

    static QSize scaleQSize(QSize size) {
        return QSize(scalePixelSize(size.width()),
                     scalePixelSize(size.height()));
    }
};

#endif
