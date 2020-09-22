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

#ifndef SV_TIME_VALUE_LAYER_H
#define SV_TIME_VALUE_LAYER_H

#include "SingleColourLayer.h"
#include "VerticalScaleLayer.h"
#include "ColourScaleLayer.h"

#include "data/model/SparseTimeValueModel.h"

#include <QObject>
#include <QColor>

class View;
class QPainter;

class TimeValueLayer : public SingleColourLayer, 
                       public VerticalScaleLayer, 
                       public ColourScaleLayer
{
    Q_OBJECT

public:
    TimeValueLayer();

    void paint(LayerGeometryProvider *v, QPainter &paint, QRect rect) const override;

    int getVerticalScaleWidth(LayerGeometryProvider *v, bool, QPainter &) const override;
    void paintVerticalScale(LayerGeometryProvider *v, bool, QPainter &paint, QRect rect) const override;

    QString getFeatureDescription(LayerGeometryProvider *v, QPoint &) const override;
    QString getLabelPreceding(sv_frame_t) const override;

    bool snapToFeatureFrame(LayerGeometryProvider *v, sv_frame_t &frame,
                            int &resolution,
                            SnapType snap, int ycoord) const override;
    bool snapToSimilarFeature(LayerGeometryProvider *v, sv_frame_t &frame,
                              int &resolution,
                              SnapType snap) const override;

    void drawStart(LayerGeometryProvider *v, QMouseEvent *) override;
    void drawDrag(LayerGeometryProvider *v, QMouseEvent *) override;
    void drawEnd(LayerGeometryProvider *v, QMouseEvent *) override;

    void eraseStart(LayerGeometryProvider *v, QMouseEvent *) override;
    void eraseDrag(LayerGeometryProvider *v, QMouseEvent *) override;
    void eraseEnd(LayerGeometryProvider *v, QMouseEvent *) override;

    void editStart(LayerGeometryProvider *v, QMouseEvent *) override;
    void editDrag(LayerGeometryProvider *v, QMouseEvent *) override;
    void editEnd(LayerGeometryProvider *v, QMouseEvent *) override;

    bool editOpen(LayerGeometryProvider *v, QMouseEvent *) override;

    void moveSelection(Selection s, sv_frame_t newStartFrame) override;
    void resizeSelection(Selection s, Selection newSize) override;
    void deleteSelection(Selection s) override;

    void copy(LayerGeometryProvider *v, Selection s, Clipboard &to) override;
    bool paste(LayerGeometryProvider *v, const Clipboard &from, sv_frame_t frameOffset,
                       bool interactive) override;

    ModelId getModel() const override { return m_model; }
    void setModel(ModelId model); // a SparseTimeValueModel

    PropertyList getProperties() const override;
    QString getPropertyLabel(const PropertyName &) const override;
    QString getPropertyIconName(const PropertyName &) const override;
    PropertyType getPropertyType(const PropertyName &) const override;
    QString getPropertyGroupName(const PropertyName &) const override;
    int getPropertyRangeAndValue(const PropertyName &,
                                         int *min, int *max, int *deflt) const override;
    QString getPropertyValueLabel(const PropertyName &,
                                          int value) const override;
    void setProperty(const PropertyName &, int value) override;

    void setFillColourMap(int);
    int getFillColourMap() const { return m_colourMap; }

    enum PlotStyle {
        PlotPoints,
        PlotStems,
        PlotConnectedPoints,
        PlotLines,
        PlotCurve,
        PlotSegmentation,
        PlotDiscreteCurves
    };

    void setPlotStyle(PlotStyle style);
    PlotStyle getPlotStyle() const { return m_plotStyle; }

    enum VerticalScale {
        AutoAlignScale,
        LinearScale,
        LogScale,
        PlusMinusOneScale
    };
    
    void setVerticalScale(VerticalScale scale);
    VerticalScale getVerticalScale() const { return m_verticalScale; }

    void setDrawSegmentDivisions(bool);
    bool getDrawSegmentDivisions() const { return m_drawSegmentDivisions; }

    void setShowDerivative(bool);
    bool getShowDerivative() const { return m_derivative; }

    bool isLayerScrollable(const LayerGeometryProvider *v) const override;

    bool isLayerEditable() const override { return true; }

    int getCompletion(LayerGeometryProvider *) const override;

    bool needsTextLabelHeight() const override;

    bool getValueExtents(double &min, double &max,
                                 bool &logarithmic, QString &unit) const override;

    bool getDisplayExtents(double &min, double &max) const override;
    bool setDisplayExtents(double min, double max) override;

    int getVerticalZoomSteps(int &defaultStep) const override;
    int getCurrentVerticalZoomStep() const override;
    void setVerticalZoomStep(int) override;
    RangeMapper *getNewVerticalZoomRangeMapper() const override;

    void toXml(QTextStream &stream, QString indent = "",
                       QString extraAttributes = "") const override;

    void setProperties(const QXmlAttributes &attributes) override;

    /// Override from SingleColourLayer
    ColourSignificance getLayerColourSignificance() const override {
        if (m_plotStyle == PlotSegmentation) {
            return ColourHasMeaningfulValue;
        } else {
            return ColourDistinguishes;
        }
    }

    /// Override from SingleColourLayer
    bool hasLightBackground() const override {
        if (m_plotStyle == PlotSegmentation) {
            return true;
        } else {
            return SingleColourLayer::hasLightBackground();
        }
    }

    /// VerticalScaleLayer and ColourScaleLayer methods
    int getYForValue(LayerGeometryProvider *, double value) const override;
    double getValueForY(LayerGeometryProvider *, int y) const override;
    QString getScaleUnits() const override;
    QColor getColourForValue(LayerGeometryProvider *v, double value) const override;

protected:
    void getScaleExtents(LayerGeometryProvider *, double &min, double &max, bool &log) const;
    bool shouldAutoAlign() const;

    EventVector getLocalPoints(LayerGeometryProvider *v, int) const;

    int getDefaultColourHint(bool dark, bool &impose) override;

    ModelId m_model;
    bool m_editing;
    Event m_originalPoint;
    Event m_editingPoint;
    ChangeEventsCommand *m_editingCommand;
    int m_colourMap;
    bool m_colourInverted;
    PlotStyle m_plotStyle;
    VerticalScale m_verticalScale;
    bool m_drawSegmentDivisions;
    bool m_derivative;

    mutable double m_scaleMinimum;
    mutable double m_scaleMaximum;

    void finish(ChangeEventsCommand *command) {
        Command *c = command->finish();
        if (c) CommandHistory::getInstance()->addCommand(c, false);
    }
};

#endif

