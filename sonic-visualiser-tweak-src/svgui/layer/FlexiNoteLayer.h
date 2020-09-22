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

#ifndef SV_FLEXINOTE_LAYER_H
#define SV_FLEXINOTE_LAYER_H

#include "SingleColourLayer.h"
#include "VerticalScaleLayer.h"

#include "data/model/NoteModel.h"

#include <QObject>
#include <QColor>

class View;
class QPainter;
class SparseTimeValueModel;

class FlexiNoteLayer : public SingleColourLayer,
                       public VerticalScaleLayer
{
    Q_OBJECT

public:
    FlexiNoteLayer();

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

    void splitStart(LayerGeometryProvider *v, QMouseEvent *) override;
    void splitEnd(LayerGeometryProvider *v, QMouseEvent *) override;
    
    void addNote(LayerGeometryProvider *v, QMouseEvent *e) override;

    virtual void mouseMoveEvent(LayerGeometryProvider *v, QMouseEvent *);

    bool editOpen(LayerGeometryProvider *v, QMouseEvent *) override;

    void moveSelection(Selection s, sv_frame_t newStartFrame) override;
    void resizeSelection(Selection s, Selection newSize) override;
    void deleteSelection(Selection s) override;
    virtual void deleteSelectionInclusive(Selection s);

    void copy(LayerGeometryProvider *v, Selection s, Clipboard &to) override;
    bool paste(LayerGeometryProvider *v, const Clipboard &from, sv_frame_t frameOffset,
                       bool interactive) override;

    void splitNotesAt(LayerGeometryProvider *v, sv_frame_t frame);
    void snapSelectedNotesToPitchTrack(LayerGeometryProvider *v, Selection s);
    void mergeNotes(LayerGeometryProvider *v, Selection s, bool inclusive);

    ModelId getModel() const override { return m_model; }
    void setModel(ModelId model); // a NoteModel please

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
    
    //GF: Tonioni: context sensitive note edit actions (denoted clockwise from top).
    enum EditMode {
        DragNote,
        RightBoundary,
        SplitNote,
        LeftBoundary
    };
    
    void setIntelligentActions(bool on) { m_intelligentActions=on; }

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
    
    void setVerticalRangeToNoteRange(LayerGeometryProvider *v);

    /// VerticalScaleLayer methods
    int getYForValue(LayerGeometryProvider *v, double value) const override;
    double getValueForY(LayerGeometryProvider *v, int y) const override;
    QString getScaleUnits() const override;

signals:
    void reAnalyseRegion(sv_frame_t, sv_frame_t, float, float);
    void materialiseReAnalysis();
    
protected:
    void getScaleExtents(LayerGeometryProvider *, double &min, double &max, bool &log) const;
    bool shouldConvertMIDIToHz() const;

    int getDefaultColourHint(bool dark, bool &impose) override;

    EventVector getLocalPoints(LayerGeometryProvider *v, int) const;

    bool getPointToDrag(LayerGeometryProvider *v, int x, int y, Event &) const;
    bool getNoteToEdit(LayerGeometryProvider *v, int x, int y, Event &) const;
    void getRelativeMousePosition(LayerGeometryProvider *v, Event &note, int x, int y, bool &closeToLeft, bool &closeToRight, bool &closeToTop, bool &closeToBottom) const;
    ModelId getAssociatedPitchModel(LayerGeometryProvider *v) const;
    bool updateNoteValueFromPitchCurve(LayerGeometryProvider *v, Event &note) const;
    void splitNotesAt(LayerGeometryProvider *v, sv_frame_t frame, QMouseEvent *e);

    ModelId m_model;
    bool m_editing;
    bool m_intelligentActions;
    int m_dragPointX;
    int m_dragPointY;
    int m_dragStartX;
    int m_dragStartY;
    Event m_originalPoint;
    Event m_editingPoint;
    sv_frame_t m_greatestLeftNeighbourFrame;
    sv_frame_t m_smallestRightNeighbourFrame;
    ChangeEventsCommand *m_editingCommand;
    VerticalScale m_verticalScale;
    EditMode m_editMode;

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

