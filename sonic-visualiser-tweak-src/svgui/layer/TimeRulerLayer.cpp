/* -*- c-basic-offset: 4 indent-tabs-mode: nil -*-  vi:set ts=8 sts=4 sw=4: */

/*
    Sonic Visualiser
    An audio file viewer and annotation editor.
    Centre for Digital Music, Queen Mary, University of London.
    This file copyright 2006 Chris Cannam.
   
    This program is free software; you can redistribute it and/or
    modify it under the terms of the GNU General Public License as
    published by the Free Software Foundation; either version 2 of the
    License, or (at your option) any later version.  See the file
    COPYING included with this distribution for more information.
*/

#include "TimeRulerLayer.h"

#include "LayerFactory.h"

#include "data/model/Model.h"
#include "base/RealTime.h"
#include "base/Preferences.h"
#include "view/View.h"

#include "ColourDatabase.h"
#include "PaintAssistant.h"

#include <QPainter>

#include <iostream>
#include <cmath>
#include <stdexcept>

//#define DEBUG_TIME_RULER_LAYER 1


TimeRulerLayer::TimeRulerLayer() :
    SingleColourLayer(),
    m_labelHeight(LabelTop)
{
    
}

void
TimeRulerLayer::setModel(ModelId model)
{
    if (m_model == model) return;
    m_model = model;
    emit modelReplaced();
}

bool
TimeRulerLayer::snapToFeatureFrame(LayerGeometryProvider *v, sv_frame_t &frame,
                                   int &resolution, SnapType snap, int) const
{
    auto model = ModelById::get(m_model);
    if (!model) {
        resolution = 1;
        return false;
    }

    bool q;
    int64_t tickUSec = getMajorTickUSec(v, q);
    RealTime rtick = RealTime::fromMicroseconds(tickUSec);
    sv_samplerate_t rate = model->getSampleRate();
    
    RealTime rt = RealTime::frame2RealTime(frame, rate);
    double ratio = rt / rtick;

    int rounded = int(ratio);
    RealTime rdrt = rtick * rounded;

    sv_frame_t left = RealTime::realTime2Frame(rdrt, rate);
    resolution = int(RealTime::realTime2Frame(rtick, rate));
    sv_frame_t right = left + resolution;

//    SVDEBUG << "TimeRulerLayer::snapToFeatureFrame: type "
//              << int(snap) << ", frame " << frame << " (time "
//              << rt << ", tick " << rtick << ", rounded " << rdrt << ") ";

    switch (snap) {

    case SnapLeft:
        frame = left;
        break;

    case SnapRight:
        frame = right;
        break;

    case SnapNeighbouring:
    {
        int dl = -1, dr = -1;
        int x = v->getXForFrame(frame);

        if (left > v->getStartFrame() &&
            left < v->getEndFrame()) {
            dl = abs(v->getXForFrame(left) - x);
        }

        if (right > v->getStartFrame() &&
            right < v->getEndFrame()) {
            dr = abs(v->getXForFrame(right) - x);
        }

        int fuzz = ViewManager::scalePixelSize(2);

        if (dl >= 0 && dr >= 0) {
            if (dl < dr) {
                if (dl <= fuzz) {
                    frame = left;
                }
            } else {
                if (dr < fuzz) {
                    frame = right;
                }
            }
        } else if (dl >= 0) {
            if (dl <= fuzz) {
                frame = left;
            }
        } else if (dr >= 0) {
            if (dr <= fuzz) {
                frame = right;
            }
        }
    }
    }

//    SVDEBUG << " -> " << frame << " (resolution = " << resolution << ")" << endl;

    return true;
}

int64_t
TimeRulerLayer::getMajorTickUSec(LayerGeometryProvider *v,
                                 bool &quarterTicks) const
{
    // return value is in microseconds
    auto model = ModelById::get(m_model);
    if (!model || !v) return 1000 * 1000;

    sv_samplerate_t sampleRate = model->getSampleRate();
    if (!sampleRate) return 1000 * 1000;

    sv_frame_t startFrame = v->getStartFrame();
    sv_frame_t endFrame = v->getEndFrame();
    if (endFrame == startFrame) {
        endFrame = startFrame + 1;
    }

    // Qt 5.13 deprecates QFontMetrics::width(), but its suggested
    // replacement (horizontalAdvance) was only added in Qt 5.11
    // which is too new for us
#pragma GCC diagnostic ignored "-Wdeprecated-declarations"

    int exampleWidth = QFontMetrics(QFont()).width("10:42.987654");
    int minPixelSpacing = v->getXForViewX(exampleWidth);

    RealTime rtStart = RealTime::frame2RealTime(startFrame, sampleRate);
    RealTime rtEnd = RealTime::frame2RealTime(endFrame, sampleRate);

    int count = v->getPaintWidth() / minPixelSpacing;
    if (count < 1) count = 1;
    RealTime rtGap = (rtEnd - rtStart) / count;

#ifdef DEBUG_TIME_RULER_LAYER
    SVCERR << "zoomLevel = " << v->getZoomLevel()
           << ", startFrame = " << startFrame << ", endFrame = " << endFrame
           << ", rtStart = " << rtStart << ", rtEnd = " << rtEnd
           << ", paint width = " << v->getPaintWidth()
           << ", minPixelSpacing = " << minPixelSpacing
           << ", count = " << count << ", rtGap = " << rtGap << endl;
#endif

    int64_t incus;
    quarterTicks = false;

    if (rtGap.sec > 0) {
        incus = 1000 * 1000;
        int s = rtGap.sec;
        if (s > 0) { incus *= 5; s /= 5; }
        if (s > 0) { incus *= 2; s /= 2; }
        if (s > 0) { incus *= 6; s /= 6; quarterTicks = true; }
        if (s > 0) { incus *= 5; s /= 5; quarterTicks = false; }
        if (s > 0) { incus *= 2; s /= 2; }
        if (s > 0) { incus *= 6; s /= 6; quarterTicks = true; }
        while (s > 0) {
            incus *= 10;
            s /= 10;
            quarterTicks = false;
        }
    } else if (rtGap.msec() > 0) {
        incus = 1000;
        int ms = rtGap.msec();
        if (ms > 0) { incus *= 10; ms /= 10; }
        if (ms > 0) { incus *= 10; ms /= 10; }
        if (ms > 0) { incus *= 5; ms /= 5; }
        if (ms > 0) { incus *= 2; ms /= 2; }
    } else {
        incus = 1;
        int us = rtGap.usec();
        if (us > 0) { incus *= 10; us /= 10; }
        if (us > 0) { incus *= 10; us /= 10; }
        if (us > 0) { incus *= 5; us /= 5; }
        if (us > 0) { incus *= 2; us /= 2; }
    }

#ifdef DEBUG_TIME_RULER_LAYER
    SVCERR << "getMajorTickUSec: returning incus = " << incus << endl;
#endif

    return incus;
}

int
TimeRulerLayer::getXForUSec(LayerGeometryProvider *v, double us) const
{
    auto model = ModelById::get(m_model);
    sv_samplerate_t sampleRate = model->getSampleRate();
    double dframe = (us * sampleRate) / 1000000.0;
    double eps = 1e-7;
    sv_frame_t frame = sv_frame_t(floor(dframe + eps));
    int x;

    ZoomLevel zoom = v->getZoomLevel();

    if (zoom.zone == ZoomLevel::FramesPerPixel) {
            
        frame /= zoom.level;
        frame *= zoom.level; // so frame corresponds to an exact pixel
        
        x = v->getXForFrame(frame);
        
    } else {

        double off = dframe - double(frame);
        int x0 = v->getXForFrame(frame);
        int x1 = v->getXForFrame(frame + 1);
        
        x = int(x0 + off * (x1 - x0));
    }

#ifdef DEBUG_TIME_RULER_LAYER
    cerr << "Considering frame = " << frame << ", x = " << x << endl;
#endif
        
    return x;
}

void
TimeRulerLayer::paint(LayerGeometryProvider *v, QPainter &paint, QRect rect) const
{
#ifdef DEBUG_TIME_RULER_LAYER
    SVCERR << "TimeRulerLayer::paint (" << rect.x() << "," << rect.y()
           << ") [" << rect.width() << "x" << rect.height() << "]" << endl;
#endif
    
    auto model = ModelById::get(m_model);
    if (!model || !model->isOK()) return;

    sv_samplerate_t sampleRate = model->getSampleRate();
    if (!sampleRate) return;

    sv_frame_t startFrame = v->getFrameForX(rect.x() - 50);

#ifdef DEBUG_TIME_RULER_LAYER
    SVCERR << "start frame = " << startFrame << endl;
#endif

    bool quarter = false;
    int64_t incus = getMajorTickUSec(v, quarter);
    int64_t us = int64_t(floor(1000.0 * 1000.0 * (double(startFrame) /
                                                  double(sampleRate))));
    us = (us / incus) * incus - incus;

#ifdef DEBUG_TIME_RULER_LAYER
    SVCERR << "start us = " << us << " at step " << incus << endl;
#endif

    Preferences *prefs = Preferences::getInstance();
    auto origTimeTextMode = prefs->getTimeToTextMode();
    if (incus < 1000) {
        // Temporarily switch to usec display mode (if we aren't using
        // it already)
        prefs->blockSignals(true);
        prefs->setTimeToTextMode(Preferences::TimeToTextUs);
    }
    
    // Calculate the number of ticks per increment -- approximate
    // values for x and frame counts here will do, no rounding issue.
    // We always use the exact incus in our calculations for where to
    // draw the actual ticks or lines.

    int minPixelSpacing = v->getXForViewX(50);
    sv_frame_t incFrame = lrint((double(incus) * sampleRate) / 1000000);
    int incX = int(round(v->getZoomLevel().framesToPixels(double(incFrame))));
    int ticks = 10;
    if (incX < minPixelSpacing * 2) {
        ticks = quarter ? 4 : 5;
    }

    QColor greyColour = getPartialShades(v)[1];

    paint.save();

    // Do not label time zero - we now overlay an opaque area over
    // time < 0 which would cut it in half
    int minlabel = 1; // us
    
    while (1) {

        // frame is used to determine where to draw the lines, so it
        // needs to correspond to an exact pixel (so that we don't get
        // a different pixel when scrolling a small amount and
        // re-drawing with a different start frame).

        double dus = double(us);

        int x = getXForUSec(v, dus);

        if (x >= rect.x() + rect.width() + 50) {
#ifdef DEBUG_TIME_RULER_LAYER
            SVCERR << "X well out of range, ending here" << endl;
#endif
            break;
        }

        if (x >= rect.x() - 50 && us >= minlabel) {

            RealTime rt = RealTime::fromMicroseconds(us);

#ifdef DEBUG_TIME_RULER_LAYER
            SVCERR << "X in range, drawing line here for time " << rt.toText() << " (usec = " << us << ")" << endl;
#endif

            QString text(QString::fromStdString(rt.toText()));
            
            QFontMetrics metrics = paint.fontMetrics();
            int tw = metrics.width(text);

            if (tw < 50 &&
                (x < rect.x() - tw/2 ||
                 x >= rect.x() + rect.width() + tw/2)) {
#ifdef DEBUG_TIME_RULER_LAYER
                SVCERR << "hm, maybe X isn't in range after all (x = " << x << ", tw = " << tw << ", rect.x() = " << rect.x() << ", rect.width() = " << rect.width() << ")" << endl;
#endif
            }

            paint.setPen(greyColour);
            paint.drawLine(x, 0, x, v->getPaintHeight());

            paint.setPen(getBaseQColor());
            paint.drawLine(x, 0, x, 5);
            paint.drawLine(x, v->getPaintHeight() - 6, x, v->getPaintHeight() - 1);

            int y;
            switch (m_labelHeight) {
            default:
            case LabelTop:
                y = 6 + metrics.ascent();
                break;
            case LabelMiddle:
                y = v->getPaintHeight() / 2 - metrics.height() / 2 + metrics.ascent();
                break;
            case LabelBottom:
                y = v->getPaintHeight() - metrics.height() + metrics.ascent() - 6;
            }

            if (v->getViewManager() && v->getViewManager()->getOverlayMode() !=
                ViewManager::NoOverlays) {

                if (v->getView()->getLayer(0) == this) {
                    // backmost layer, don't worry about outlining the text
                    paint.drawText(x+2 - tw/2, y, text);
                } else {
                    PaintAssistant::drawVisibleText(v, paint, x+2 - tw/2, y, text, PaintAssistant::OutlinedText);
                }
            }
        }

        paint.setPen(greyColour);

        for (int i = 1; i < ticks; ++i) {

            dus = double(us) + (i * double(incus)) / ticks;

            x = getXForUSec(v, dus);

            if (x < rect.x() || x >= rect.x() + rect.width()) {
#ifdef DEBUG_TIME_RULER_LAYER
//                SVCERR << "tick " << i << ": X out of range, going on to next tick" << endl;
#endif
                continue;
            }

#ifdef DEBUG_TIME_RULER_LAYER
            SVCERR << "tick " << i << " in range, drawing at " << x << endl;
#endif

            int sz = 5;
            if (ticks == 10) {
                if ((i % 2) == 1) {
                    if (i == 5) {
                        paint.drawLine(x, 0, x, v->getPaintHeight());
                    } else sz = 3;
                } else {
                    sz = 7;
                }
            }
            paint.drawLine(x, 0, x, sz);
            paint.drawLine(x, v->getPaintHeight() - sz - 1, x, v->getPaintHeight() - 1);
        }

        us += incus;
    }
    
    prefs->setTimeToTextMode(origTimeTextMode);
    prefs->blockSignals(false);

    paint.restore();
}

int
TimeRulerLayer::getDefaultColourHint(bool darkbg, bool &impose)
{
    impose = true;
    return ColourDatabase::getInstance()->getColourIndex
        (QString(darkbg ? "White" : "Black"));
}

QString TimeRulerLayer::getLayerPresentationName() const
{
    LayerFactory *factory = LayerFactory::getInstance();
    QString layerName = factory->getLayerPresentationName
        (factory->getLayerType(this));
    return layerName;
}

void
TimeRulerLayer::toXml(QTextStream &stream,
                      QString indent, QString extraAttributes) const
{
    SingleColourLayer::toXml(stream, indent, extraAttributes);
}

void
TimeRulerLayer::setProperties(const QXmlAttributes &attributes)
{
    SingleColourLayer::setProperties(attributes);
}

