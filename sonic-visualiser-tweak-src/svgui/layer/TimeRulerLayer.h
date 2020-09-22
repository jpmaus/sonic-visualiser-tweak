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

#ifndef SV_TIME_RULER_H
#define SV_TIME_RULER_H

#include "SingleColourLayer.h"

#include <QRect>
#include <QColor>

class View;
class Model;
class QPainter;

class TimeRulerLayer : public SingleColourLayer
{
    Q_OBJECT

public:
    TimeRulerLayer();

    void paint(LayerGeometryProvider *v, QPainter &paint, QRect rect) const override;

    void setModel(ModelId);
    ModelId getModel() const override { return m_model; }

    enum LabelHeight { LabelTop, LabelMiddle, LabelBottom };
    void setLabelHeight(LabelHeight h) { m_labelHeight = h; }
    LabelHeight getLabelHeight() const { return m_labelHeight; }

    bool snapToFeatureFrame(LayerGeometryProvider *, sv_frame_t &, int &,
                            SnapType, int) const override;

    ColourSignificance getLayerColourSignificance() const override {
        return ColourIrrelevant;
    }

    bool getValueExtents(double &, double &, bool &, QString &) const override {
        return false;
    }

    QString getLayerPresentationName() const override;

    int getVerticalScaleWidth(LayerGeometryProvider *, bool, QPainter &) const override { return 0; }

    void toXml(QTextStream &stream, QString indent = "",
                       QString extraAttributes = "") const override;

    void setProperties(const QXmlAttributes &attributes) override;

    bool canExistWithoutModel() const override { return true; }

protected:
    ModelId m_model;
    LabelHeight m_labelHeight;

    int getDefaultColourHint(bool dark, bool &impose) override;

    int64_t getMajorTickUSec(LayerGeometryProvider *, bool &quarterTicks) const;
    int getXForUSec(LayerGeometryProvider *, double usec) const;
};

#endif
