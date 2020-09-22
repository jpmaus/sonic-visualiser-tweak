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

#ifndef SV_TIME_INSTANT_LAYER_H
#define SV_TIME_INSTANT_LAYER_H

#include "SingleColourLayer.h"
#include "data/model/SparseOneDimensionalModel.h"

#include <QObject>
#include <QColor>

class View;
class QPainter;

class TimeInstantLayer : public SingleColourLayer
{
    Q_OBJECT

public:
    TimeInstantLayer();
    virtual ~TimeInstantLayer();

    void paint(LayerGeometryProvider *v, QPainter &paint, QRect rect) const override;

    QString getLabelPreceding(sv_frame_t) const override;
    QString getFeatureDescription(LayerGeometryProvider *v, QPoint &) const override;

    bool snapToFeatureFrame(LayerGeometryProvider *v, sv_frame_t &frame,
                            int &resolution,
                            SnapType snap, int ycoord) const override;

    void drawStart(LayerGeometryProvider *v, QMouseEvent *) override;
    void drawDrag(LayerGeometryProvider *v, QMouseEvent *) override;
    void drawEnd(LayerGeometryProvider *v, QMouseEvent *) override;

    void eraseStart(LayerGeometryProvider *v, QMouseEvent *) override;
    void eraseDrag(LayerGeometryProvider *v, QMouseEvent *) override;
    void eraseEnd(LayerGeometryProvider *v, QMouseEvent *) override;

    void editStart(LayerGeometryProvider *v, QMouseEvent *) override;
    void editDrag(LayerGeometryProvider *v, QMouseEvent *) override;
    void editEnd(LayerGeometryProvider *v, QMouseEvent *) override;

    bool editOpen(LayerGeometryProvider *, QMouseEvent *) override;

    void moveSelection(Selection s, sv_frame_t newStartFrame) override;
    void resizeSelection(Selection s, Selection newSize) override;
    void deleteSelection(Selection s) override;

    void copy(LayerGeometryProvider *v, Selection s, Clipboard &to) override;
    bool paste(LayerGeometryProvider *v, const Clipboard &from, sv_frame_t frameOffset,
                       bool interactive) override;

    ModelId getModel() const override { return m_model; }
    void setModel(ModelId model); // a SparseOneDimensionalModel

    PropertyList getProperties() const override;
    QString getPropertyLabel(const PropertyName &) const override;
    PropertyType getPropertyType(const PropertyName &) const override;
    int getPropertyRangeAndValue(const PropertyName &,
                                         int *min, int *max, int *deflt) const override;
    QString getPropertyValueLabel(const PropertyName &,
                                          int value) const override;
    void setProperty(const PropertyName &, int value) override;

    enum PlotStyle {
        PlotInstants,
        PlotSegmentation
    };

    void setPlotStyle(PlotStyle style);
    PlotStyle getPlotStyle() const { return m_plotStyle; }

    bool isLayerScrollable(const LayerGeometryProvider *v) const override;

    bool isLayerEditable() const override { return true; }

    int getCompletion(LayerGeometryProvider *) const override;

    bool needsTextLabelHeight() const override;

    bool getValueExtents(double &, double &, bool &, QString &) const override {
        return false;
    }

    void toXml(QTextStream &stream, QString indent = "",
                       QString extraAttributes = "") const override;

    void setProperties(const QXmlAttributes &attributes) override;

    ColourSignificance getLayerColourSignificance() const override {
        if (m_plotStyle == PlotSegmentation) {
            return ColourHasMeaningfulValue;
        } else {
            return ColourDistinguishes;
        }
    }

    int getVerticalScaleWidth(LayerGeometryProvider *, bool, QPainter &) const override { return 0; }

protected:
    EventVector getLocalPoints(LayerGeometryProvider *v, int) const;

    int getDefaultColourHint(bool dark, bool &impose) override;

    bool clipboardAlignmentDiffers(LayerGeometryProvider *v, const Clipboard &) const;

    ModelId m_model;
    bool m_editing;
    Event m_editingPoint;
    ChangeEventsCommand *m_editingCommand;
    PlotStyle m_plotStyle;

    void finish(ChangeEventsCommand *command) {
        Command *c = command->finish();
        if (c) CommandHistory::getInstance()->addCommand(c, false);
    }
};

#endif

