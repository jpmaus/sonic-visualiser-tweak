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

#ifndef SV_BOX_LAYER_H
#define SV_BOX_LAYER_H

#include "SingleColourLayer.h"
#include "VerticalScaleLayer.h"

#include "data/model/BoxModel.h"

#include <QObject>
#include <QColor>

#include <map>

class View;
class QPainter;

class BoxLayer : public SingleColourLayer,
                              public VerticalScaleLayer
{
    Q_OBJECT

public:
    BoxLayer();

    void paint(LayerGeometryProvider *v, QPainter &paint, QRect rect) const override;

    int getVerticalScaleWidth(LayerGeometryProvider *v, bool, QPainter &) const override;
    void paintVerticalScale(LayerGeometryProvider *v, bool, QPainter &paint, QRect rect) const override;

    QString getFeatureDescription(LayerGeometryProvider *v, QPoint &) const override;
    QString getLabelPreceding(sv_frame_t) const override;

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

    bool editOpen(LayerGeometryProvider *v, QMouseEvent *) override;

    void moveSelection(Selection s, sv_frame_t newStartFrame) override;
    void resizeSelection(Selection s, Selection newSize) override;
    void deleteSelection(Selection s) override;

    void copy(LayerGeometryProvider *v, Selection s, Clipboard &to) override;
    bool paste(LayerGeometryProvider *v, const Clipboard &from,
               sv_frame_t frameOffset, bool interactive) override;

    ModelId getModel() const override { return m_model; }
    void setModel(ModelId model); // a BoxModel

    PropertyList getProperties() const override;
    QString getPropertyLabel(const PropertyName &) const override;
    PropertyType getPropertyType(const PropertyName &) const override;
    QString getPropertyGroupName(const PropertyName &) const override;
    int getPropertyRangeAndValue(const PropertyName &,
                                 int *min, int *max, int *deflt) const override;
    QString getPropertyValueLabel(const PropertyName &,
                                  int value) const override;
    void setProperty(const PropertyName &, int value) override;

    enum VerticalScale {
        AutoAlignScale,
        LinearScale,
        LogScale,
    };

    void setVerticalScale(VerticalScale scale);
    VerticalScale getVerticalScale() const { return m_verticalScale; }

    bool isLayerScrollable(const LayerGeometryProvider *v) const override;

    bool isLayerEditable() const override { return true; }

    int getCompletion(LayerGeometryProvider *) const override;

    bool getValueExtents(double &min, double &max,
                                 bool &log, QString &unit) const override;

    bool getDisplayExtents(double &min, double &max) const override;

    bool adoptExtents(double min, double max, QString unit) override;

    void toXml(QTextStream &stream, QString indent = "",
                       QString extraAttributes = "") const override;

    void setProperties(const QXmlAttributes &attributes) override;

    /// VerticalScaleLayer methods
    int getYForValue(LayerGeometryProvider *v, double value) const override;
    double getValueForY(LayerGeometryProvider *v, int y) const override;
    QString getScaleUnits() const override;

protected:
    void getScaleExtents(LayerGeometryProvider *, double &min, double &max, bool &log) const;

    // Return the event that "most closely contains" the given
    // coordinates, if any; or the closest event that spans the given
    // x coordinate in the time axis; or false otherwise. This
    // subsumes the uses of both getPointToDrag and getLocalPoints in
    // some other layer implementations.
    bool getLocalPoint(LayerGeometryProvider *v, int x, int y, Event &) const;

    ModelId m_model;
    bool m_editing;
    int m_dragPointX;
    int m_dragPointY;
    int m_dragStartX;
    int m_dragStartY;
    Event m_originalPoint;
    Event m_editingPoint;
    ChangeEventsCommand *m_editingCommand;
    VerticalScale m_verticalScale;

    std::pair<float, float> getRange(const Event &e) const {
        return { e.getValue(), e.getValue() + fabsf(e.getLevel()) };
    }
    
    void finish(ChangeEventsCommand *command) {
        Command *c = command->finish();
        if (c) CommandHistory::getInstance()->addCommand(c, false);
    }
};

#endif

