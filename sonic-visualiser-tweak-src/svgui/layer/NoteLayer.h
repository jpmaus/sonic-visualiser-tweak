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

#ifndef SV_NOTE_LAYER_H
#define SV_NOTE_LAYER_H

#include "SingleColourLayer.h"
#include "VerticalScaleLayer.h"

#include "data/model/NoteModel.h"

#include <QObject>
#include <QColor>

class View;
class QPainter;

/**
 * Layer for displaying and editing notes, i.e. discrete events with
 * start time, duration, value that represents pitch, and optionally a
 * level that represents velocity.
 *
 * For the purposes of public API, integration with other classes, and
 * display alignment, the y-coordinate (value) of the layer always has
 * a unit of Hz. The model itself may have another unit, such as MIDI
 * pitch, but the layer always converts to and from Hz behind the
 * scenes.
 */
class NoteLayer : public SingleColourLayer,
                  public VerticalScaleLayer
{
    Q_OBJECT

public:
    NoteLayer();

    void paint(LayerGeometryProvider *v, QPainter &paint, QRect rect) const override;

    int getVerticalScaleWidth(LayerGeometryProvider *v, bool, QPainter &) const override;
    void paintVerticalScale(LayerGeometryProvider *v, bool, QPainter &paint, QRect rect) const override;

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

    bool editOpen(LayerGeometryProvider *v, QMouseEvent *) override;

    void moveSelection(Selection s, sv_frame_t newStartFrame) override;
    void resizeSelection(Selection s, Selection newSize) override;
    void deleteSelection(Selection s) override;

    void copy(LayerGeometryProvider *v, Selection s, Clipboard &to) override;
    bool paste(LayerGeometryProvider *v, const Clipboard &from,
               sv_frame_t frameOffset, bool interactive) override;

    ModelId getModel() const override { return m_model; }
    void setModel(ModelId model); // a NoteModel

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
        MIDIRangeScale
    };

    void setVerticalScale(VerticalScale scale);
    VerticalScale getVerticalScale() const { return m_verticalScale; }

    bool isLayerScrollable(const LayerGeometryProvider *v) const override;

    bool isLayerEditable() const override { return true; }

    int getCompletion(LayerGeometryProvider *) const override;

    bool getValueExtents(double &min, double &max,
                         bool &log, QString &unit) const override;

    bool getDisplayExtents(double &min, double &max) const override;
    bool setDisplayExtents(double min, double max) override;

    int getVerticalZoomSteps(int &defaultStep) const override;
    int getCurrentVerticalZoomStep() const override;
    void setVerticalZoomStep(int) override;
    RangeMapper *getNewVerticalZoomRangeMapper() const override;

    /**
     * Add a note-on.  Used when recording MIDI "live".  The note will
     * not be finally added to the layer until the corresponding
     * note-off.
     */
    void addNoteOn(sv_frame_t frame, int pitch, int velocity);
    
    /**
     * Add a note-off.  This will cause a note to appear, if and only
     * if there is a matching pending note-on.
     */
    void addNoteOff(sv_frame_t frame, int pitch);

    /**
     * Abandon all pending note-on events.
     */
    void abandonNoteOns();

    void toXml(QTextStream &stream, QString indent = "",
                       QString extraAttributes = "") const override;

    void setProperties(const QXmlAttributes &attributes) override;

    /// VerticalScaleLayer methods
    int getYForValue(LayerGeometryProvider *v, double value) const override;
    double getValueForY(LayerGeometryProvider *v, int y) const override;
    QString getScaleUnits() const override;

protected:
    void getScaleExtents(LayerGeometryProvider *, double &min, double &max, bool &log) const;

    int getDefaultColourHint(bool dark, bool &impose) override;

    EventVector getLocalPoints(LayerGeometryProvider *v, int) const;

    bool getPointToDrag(LayerGeometryProvider *v, int x, int y, Event &) const;

    double convertValueFromEventValue(float eventValue) const;
    float convertValueToEventValue(double value) const;
    
    double valueOf(const Event &e) const;
    Event eventWithValue(const Event &e, double value) const;
    
    ModelId m_model;
    bool m_modelUsesHz;
    bool m_editing;
    int m_dragPointX;
    int m_dragPointY;
    int m_dragStartX;
    int m_dragStartY;
    Event m_originalPoint;
    Event m_editingPoint;
    ChangeEventsCommand *m_editingCommand;
    bool m_editIsOpen;
    VerticalScale m_verticalScale;

    typedef std::set<Event> NoteSet;
    NoteSet m_pendingNoteOns;

    mutable double m_scaleMinimum;
    mutable double m_scaleMaximum;

    bool shouldAutoAlign() const;

    void finish(ChangeEventsCommand *command) {
        Command *c = command->finish();
        if (c) CommandHistory::getInstance()->addCommand(c, false);
    }
};

#endif

