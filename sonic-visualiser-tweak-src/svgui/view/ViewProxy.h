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

#ifndef VIEW_PROXY_H
#define VIEW_PROXY_H

#include "layer/LayerGeometryProvider.h"

#include "data/model/AlignmentModel.h"

class ViewProxy : public LayerGeometryProvider
{
public:
    /**
     * Create a standard ViewProxy for the given view, mapping using
     * the given scale factor. The scale factor is generally used with
     * pixel-doubled "retina" Mac displays and is usually 1 elsewhere.
     */
    ViewProxy(View *view, int scaleFactor) :
        m_view(view), m_scaleFactor(scaleFactor) { }

    /**
     * Create a re-aligning ViewProxy for the given view, mapping
     * using the given scale factor. The scale factor is generally
     * used with pixel-doubled "retina" Mac displays and is usually 1
     * elsewhere. 
     * 
     * Coordinates are mapped through the given alignment model, such
     * that frame values passed from the caller are mapped "from
     * reference" by that alignment before being used by the view or
     * converted to pixel coordinates, and returned values are mapped
     * back "to reference" before being passed back to the caller.
     * 
     * This form of proxy may be created specially for rendering a
     * single layer which comes from a different alignment to that of
     * the rest of the containing view.
     */
    ViewProxy(View *view, int scaleFactor, ModelId alignment) :
        m_view(view), m_scaleFactor(scaleFactor), m_alignment(alignment) { }

    int getId() const override {
        return m_view->getId();
    }
    sv_frame_t getStartFrame() const override {
        return alignToReference(m_view->getStartFrame());
    }
    sv_frame_t getCentreFrame() const override {
        return alignToReference(m_view->getCentreFrame());
    }
    sv_frame_t getEndFrame() const override {
        return alignToReference(m_view->getEndFrame());
    }
    int getXForFrame(sv_frame_t frame) const override {
        //!!! not actually correct, if frame lies between view's pixels
        return m_scaleFactor * m_view->getXForFrame(alignFromReference(frame));
    }
    sv_frame_t getFrameForX(int x) const override {
        sv_frame_t f0 = m_view->getFrameForX(x / m_scaleFactor);
        if (m_scaleFactor == 1) return alignToReference(f0);
        sv_frame_t f1 = m_view->getFrameForX((x / m_scaleFactor) + 1);
        sv_frame_t f = f0 + ((f1 - f0) * (x % m_scaleFactor)) / m_scaleFactor;
        return alignToReference(f);
    }
    int getXForViewX(int viewx) const override {
        return viewx * m_scaleFactor;
    }
    int getViewXForX(int x) const override {
        return x / m_scaleFactor;
    }
    sv_frame_t getModelsStartFrame() const override {
        return alignToReference(m_view->getModelsStartFrame());
    }
    sv_frame_t getModelsEndFrame() const override {
        return alignToReference(m_view->getModelsEndFrame());
    }
    double getYForFrequency(double frequency,
                                    double minFreq, double maxFreq, 
                                    bool logarithmic) const override {
        return m_scaleFactor *
            m_view->getYForFrequency(frequency, minFreq, maxFreq, logarithmic);
    }
    double getFrequencyForY(double y, double minFreq, double maxFreq,
                                    bool logarithmic) const override {
        return m_view->getFrequencyForY
            (y / m_scaleFactor, minFreq, maxFreq, logarithmic);
    }
    int getTextLabelYCoord(const Layer *layer, QPainter &paint) const override {
        return m_scaleFactor * m_view->getTextLabelYCoord(layer, paint);
    }
    bool getVisibleExtentsForUnit(QString unit, double &min, double &max,
                                  bool &log) const override {
        return m_view->getVisibleExtentsForUnit(unit, min, max, log);
    }
    ZoomLevel getZoomLevel() const override {
        ZoomLevel z = m_view->getZoomLevel();
        if (z.zone == ZoomLevel::FramesPerPixel) {
            z.level /= m_scaleFactor;
            if (z.level < 1) {
                z.level = 1;
            }
        } else {
            z.level *= m_scaleFactor;
        }
        return z;
    }
    QRect getPaintRect() const override {
        QRect r = m_view->getPaintRect();
        return QRect(r.x() * m_scaleFactor,
                     r.y() * m_scaleFactor,
                     r.width() * m_scaleFactor,
                     r.height() * m_scaleFactor);
    }
    QSize getPaintSize() const override {
        return getPaintRect().size();
    }
    int getPaintWidth() const override { 
        return getPaintRect().width();
    }
    int getPaintHeight() const override { 
        return getPaintRect().height();
    }
    bool hasLightBackground() const override {
        return m_view->hasLightBackground();
    }
    QColor getForeground() const override {
        return m_view->getForeground();
    }
    QColor getBackground() const override {
        return m_view->getBackground();
    }
    ViewManager *getViewManager() const override {
        return m_view->getViewManager();
    }
        
    bool shouldIlluminateLocalFeatures(const Layer *layer,
                                               QPoint &point) const override {
        QPoint p;
        bool should = m_view->shouldIlluminateLocalFeatures(layer, p);
        point = QPoint(p.x() * m_scaleFactor, p.y() * m_scaleFactor);
        return should;
    }

    bool shouldShowFeatureLabels() const override {
        return m_view->shouldShowFeatureLabels();
    }

    void drawMeasurementRect(QPainter &p, const Layer *layer,
                                     QRect rect, bool focus) const override {
        m_view->drawMeasurementRect(p, layer, rect, focus);
    }

    void updatePaintRect(QRect r) override {
        m_view->update(r.x() / m_scaleFactor,
                       r.y() / m_scaleFactor,
                       r.width() / m_scaleFactor,
                       r.height() / m_scaleFactor);
    }

    /**
     * Scale up a size in pixels for a hi-dpi display without pixel
     * doubling. This is like ViewManager::scalePixelSize, but taking
     * and returning floating-point values rather than integer
     * pixels. It is also a little more conservative - it never
     * shrinks the size, it can only increase or leave it unchanged.
     */
    double scaleSize(double size) const override {
        return m_view->scaleSize(size * m_scaleFactor);
    }

    /**
     * Integer version of scaleSize.
     */
    int scalePixelSize(int size) const override {
        return m_view->scalePixelSize(size * m_scaleFactor);
    }
    
    /**
     * Scale up pen width for a hi-dpi display without pixel doubling.
     * This is like scaleSize except that it also scales the
     * zero-width case.
     */
    double scalePenWidth(double width) const override {
        if (width <= 0) { // zero-width pen, produce a scaled one-pixel pen
            width = 1;
        }
        width *= sqrt(double(m_scaleFactor));
        return m_view->scalePenWidth(width);
    }

    /**
     * Apply scalePenWidth to a pen.
     */
    QPen scalePen(QPen pen) const override {
        return QPen(pen.color(), scalePenWidth(pen.width()));
    }
    
    View *getView() override { return m_view; }
    const View *getView() const override { return m_view; }

private:
    View *m_view;
    int m_scaleFactor;
    ModelId m_alignment;

    sv_frame_t alignToReference(sv_frame_t frame) const {
        if (auto am = ModelById::getAs<AlignmentModel>(m_alignment)) {
            return am->toReference(frame);
        } else {
            return frame;
        }
    }

    sv_frame_t alignFromReference(sv_frame_t frame) const {
        if (auto am = ModelById::getAs<AlignmentModel>(m_alignment)) {
            return am->fromReference(frame);
        } else {
            return frame;
        }
    }
};

#endif
