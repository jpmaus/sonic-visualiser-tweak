/* -*- c-basic-offset: 4 indent-tabs-mode: nil -*-  vi:set ts=8 sts=4 sw=4: */

/*
    Sonic Visualiser
    An audio file viewer and annotation editor.
    Centre for Digital Music, Queen Mary, University of London.
    This file copyright 2006 Chris Cannam and QMUL.
    
    This program is free software; you can redistribute it and/or
    modify it under the terms of the GNU General Public License as
    published by the Free Software Foundation; either version 2 of the
    License, or (at your option) any later version.  See the file
    COPYING included with this distribution for more information.
*/

#ifndef SV_LAYER_H
#define SV_LAYER_H

#include "base/PropertyContainer.h"
#include "base/XmlExportable.h"
#include "base/Selection.h"

#include "data/model/Model.h"

#include "widgets/CommandHistory.h"

#include "system/System.h"

#include <QObject>
#include <QRect>
#include <QXmlAttributes>
#include <QMutex>
#include <QPixmap>

#include <map>
#include <set>

#include <iostream>

class ZoomConstraint;
class QPainter;
class View;
class LayerGeometryProvider;
class QMouseEvent;
class Clipboard;
class RangeMapper;

/**
 * The base class for visual representations of the data found in a
 * Model.  Layers are expected to be able to draw themselves onto a
 * View, and may also be editable.
 */

class Layer : public PropertyContainer,
              public XmlExportable
{
    Q_OBJECT

public:
    Layer();
    virtual ~Layer();

    /**
     * Return the ID of the model represented in this layer.
     */
    virtual ModelId getModel() const = 0;

    /**
     * Return the ID of the source model for the model represented in
     * this layer. If the model has no other source, or there is no
     * model here, return None.
     */
    ModelId getSourceModel() const;
    
    /**
     * Return a zoom constraint object defining the supported zoom
     * levels for this layer.  If this returns zero, the layer will
     * support any integer zoom level.
     */
    virtual const ZoomConstraint *getZoomConstraint() const { return 0; }

    /**
     * Return true if this layer can handle zoom levels other than
     * those supported by its zoom constraint (presumably less
     * efficiently or accurately than the officially supported zoom
     * levels).  If true, the layer will unenthusistically accept any
     * integer zoom level from 1 to the maximum returned by its zoom
     * constraint.
     */
    virtual bool supportsOtherZoomLevels() const { return true; }

    /**
     * Paint the given rectangle of this layer onto the given view
     * using the given painter, superimposing it on top of any
     * existing material in that view.  The LayerGeometryProvider (an
     * interface implemented by View) is provided here because it is
     * possible for one layer to exist in more than one view, so the
     * dimensions of the view may vary from one paint call to another
     * (without any view having been resized).
     */
    virtual void paint(LayerGeometryProvider *, QPainter &, QRect) const = 0;   

    /**
     * Enable or disable synchronous painting.  If synchronous
     * painting is enabled, a call to paint() must complete painting
     * the entire rectangle before it returns.  If synchronous
     * painting is disabled (which should be the default), the paint()
     * call may defer painting some regions if data is not yet
     * available, by calling back on its view to schedule another
     * update.  Synchronous painting is necessary when rendering to an
     * image.  Simple layer types will always paint synchronously, and
     * so may ignore this.
     */
    virtual void setSynchronousPainting(bool /* synchronous */) { }

    enum VerticalPosition {
        PositionTop, PositionMiddle, PositionBottom
    };
    virtual VerticalPosition getPreferredTimeRulerPosition() const {
        return PositionMiddle;
    }
    virtual VerticalPosition getPreferredFrameCountPosition() const {
        return PositionBottom;
    }
    virtual bool hasLightBackground() const {
        return true;
    }

    QString getPropertyContainerIconName() const override;

    QString getPropertyContainerName() const override {
        if (m_presentationName != "") return m_presentationName;
        else return objectName();
    }

    virtual void setPresentationName(QString name);

    virtual QString getLayerPresentationName() const;
    virtual QPixmap getLayerPresentationPixmap(QSize) const { return QPixmap(); }

    virtual int getVerticalScaleWidth(LayerGeometryProvider *, bool detailed,
                                      QPainter &) const = 0;

    virtual void paintVerticalScale(LayerGeometryProvider *, bool /* detailed */,
                                    QPainter &, QRect) const { }

    virtual int getHorizontalScaleHeight(LayerGeometryProvider *, QPainter &) const { return 0; }
    
    virtual bool getCrosshairExtents(LayerGeometryProvider *, QPainter &, QPoint /* cursorPos */,
                                     std::vector<QRect> &) const {
        return false;
    }
    virtual void paintCrosshairs(LayerGeometryProvider *, QPainter &, QPoint) const { }

    virtual void paintMeasurementRects(LayerGeometryProvider *, QPainter &,
                                       bool showFocus, QPoint focusPoint) const;

    virtual bool nearestMeasurementRectChanged(LayerGeometryProvider *, QPoint prev,
                                               QPoint now) const;

    virtual QString getFeatureDescription(LayerGeometryProvider *, QPoint &) const {
        return "";
    }

    virtual QString getLabelPreceding(sv_frame_t /* frame */) const {
        return "";
    }

    enum SnapType {
        SnapLeft,
        SnapRight,
        SnapNeighbouring
    };

    /**
     * Adjust the given frame to snap to the nearest feature, if
     * possible.
     *
     * If snap is SnapLeft or SnapRight, adjust the frame to match
     * that of the nearest feature in the given direction regardless
     * of how far away it is. If snap is SnapNeighbouring, adjust the
     * frame to that of the nearest feature in either direction if it
     * is close, and leave it alone (returning false) otherwise.
     * SnapNeighbouring should always choose the same feature that
     * would be used in an editing operation through calls to
     * editStart etc.
     *
     * If ycoord is non-negative, it contains the y coordinate at
     * which the interaction that prompts this snap is taking place
     * (e.g. of the mouse press used for a selection action). Layers
     * that have objects at multiple different heights may choose to
     * use this information. If the current action has no particular y
     * coordinate associated with it, ycoord will be passed as -1.
     *
     * Return true if a suitable feature was found and frame adjusted
     * accordingly.  Return false if no suitable feature was available
     * (and leave frame unmodified).  If returning true, also return
     * the resolution of the model in this layer in sample frames.
     */
    virtual bool snapToFeatureFrame(LayerGeometryProvider * /* v */,
                                    sv_frame_t & /* frame */,
                                    int &resolution,
                                    SnapType /* snap */,
                                    int /* ycoord */) const {
        resolution = 1;
        return false;
    }

    /**
     * Adjust the given frame to snap to the next feature that has
     * "effectively" the same value as the feature prior to the given
     * frame, if possible.
     *
     * The snap type must be SnapLeft (snap to the time of the next
     * feature prior to the one preceding the given frame that has a
     * similar value to it) or SnapRight (snap to the time of the next
     * feature following the given frame that has a similar value to
     * the feature preceding it).  Other values are not permitted.
     *
     * Return true if a suitable feature was found and frame adjusted
     * accordingly.  Return false if no suitable feature was available
     * (and leave frame unmodified).  If returning true, also return
     * the resolution of the model in this layer in sample frames.
     */
    virtual bool snapToSimilarFeature(LayerGeometryProvider * /* v */,
                                      sv_frame_t & /* source frame */,
                                      int &resolution,
                                      SnapType /* snap */) const {
        resolution = 1;
        return false;
    }

    // Draw, erase, and edit modes:
    //
    // Layer needs to get actual mouse events, I guess.  Draw mode is
    // probably the easier.

    virtual void drawStart(LayerGeometryProvider *, QMouseEvent *) { }
    virtual void drawDrag(LayerGeometryProvider *, QMouseEvent *) { }
    virtual void drawEnd(LayerGeometryProvider *, QMouseEvent *) { }

    virtual void eraseStart(LayerGeometryProvider *, QMouseEvent *) { }
    virtual void eraseDrag(LayerGeometryProvider *, QMouseEvent *) { }
    virtual void eraseEnd(LayerGeometryProvider *, QMouseEvent *) { }

    virtual void editStart(LayerGeometryProvider *, QMouseEvent *) { }
    virtual void editDrag(LayerGeometryProvider *, QMouseEvent *) { }
    virtual void editEnd(LayerGeometryProvider *, QMouseEvent *) { }

    virtual void splitStart(LayerGeometryProvider *, QMouseEvent *) { }
    virtual void splitEnd(LayerGeometryProvider *, QMouseEvent *) { }
    virtual void addNote(LayerGeometryProvider *, QMouseEvent *) { };

    // Measurement rectangle (or equivalent).  Unlike draw and edit,
    // the base Layer class can provide working implementations of
    // these for most situations.
    //
    virtual void measureStart(LayerGeometryProvider *, QMouseEvent *);
    virtual void measureDrag(LayerGeometryProvider *, QMouseEvent *);
    virtual void measureEnd(LayerGeometryProvider *, QMouseEvent *);
    virtual void measureDoubleClick(LayerGeometryProvider *, QMouseEvent *);

    virtual bool haveCurrentMeasureRect() const {
        return m_haveCurrentMeasureRect;
    }
    virtual void deleteCurrentMeasureRect(); // using a command

    /**
     * Open an editor on the item under the mouse (e.g. on
     * double-click).  If there is no item or editing is not
     * supported, return false.
     */
    virtual bool editOpen(LayerGeometryProvider *, QMouseEvent *) { return false; }

    virtual void moveSelection(Selection, sv_frame_t /* newStartFrame */) { }
    virtual void resizeSelection(Selection, Selection /* newSize */) { }
    virtual void deleteSelection(Selection) { }

    virtual void copy(LayerGeometryProvider *, Selection, Clipboard & /* to */) { }

    /**
     * Paste from the given clipboard onto the layer at the given
     * frame offset.  If interactive is true, the layer may ask the
     * user about paste options through a dialog if desired, and may
     * return false if the user cancelled the paste operation.  This
     * function should return true if a paste actually occurred.
     */
    virtual bool paste(LayerGeometryProvider *,
                       const Clipboard & /* from */,
                       sv_frame_t /* frameOffset */,
                       bool /* interactive */) { return false; }

    // Text mode:
    //
    // Label nearest feature.  We need to get the feature coordinates
    // and current label from the layer, and then the pane can pop up
    // a little text entry dialog at the right location.  Or we edit
    // in place?  Probably the dialog is easier.

    /**
     * This should return true if the layer can safely be scrolled
     * automatically by a given view (simply copying the existing data
     * and then refreshing the exposed area) without altering its
     * meaning.  For the view widget as a whole this is usually not
     * possible because of invariant (non-scrolling) material
     * displayed over the top, but the widget may be able to optimise
     * scrolling better if it is known that individual views can be
     * scrolled safely in this way.
     */
    virtual bool isLayerScrollable(const LayerGeometryProvider *) const { return true; }

    /**
     * This should return true if the layer completely obscures any
     * underlying layers.  It's used to determine whether the view can
     * safely draw any selection rectangles under the layer instead of
     * over it, in the case where the layer is not scrollable and
     * therefore needs to be redrawn each time (so that the selection
     * rectangle can be cached).
     */
    virtual bool isLayerOpaque() const { return false; }

    enum ColourSignificance {
        ColourAbsent,
        ColourIrrelevant,
        ColourDistinguishes,
        ColourAndBackgroundSignificant,
        ColourHasMeaningfulValue
    };

    /**
     * This should return the degree of meaning associated with colour
     * in this layer.
     *
     * If ColourAbsent, the layer does not use colour.  If
     * ColourIrrelevant, the layer is coloured and the colour may be
     * set by the user, but it doesn't really matter what the colour
     * is (for example, in a time ruler layer).  If
     * ColourDistinguishes, then the colour is used to distinguish
     * this layer from other similar layers (e.g. for data layers).
     * If ColourAndBackgroundSignificant, then the layer should be
     * given greater weight than ColourDistinguishes layers when
     * choosing a background colour (e.g. for waveforms).  If
     * ColourHasMeaningfulValue, colours are actually meaningful --
     * the view will then show selections using unfilled rectangles
     * instead of translucent filled rectangles, so as not to disturb
     * the colours underneath.
     */
    virtual ColourSignificance getLayerColourSignificance() const = 0;

    /**
     * This should return true if the layer can be edited by the user.
     * If this is the case, the appropriate edit tools may be made
     * available by the application and the layer's drawStart/Drag/End
     * and editStart/Drag/End methods should be implemented.
     */
    virtual bool isLayerEditable() const { return false; }

    /**
     * Return the proportion of background work complete in drawing
     * this view, as a percentage -- in most cases this will be the
     * value returned by pointer from a call to the underlying model's
     * isReady(int *) call.  The view may choose to show a progress
     * meter if it finds that this returns < 100 at any given moment.
     */
    virtual int getCompletion(LayerGeometryProvider *) const { return 100; }

    /**
     * Return an error string if any errors have occurred while
     * loading or processing data for the given view.  Return the
     * empty string if no error has occurred.
     */
    virtual QString getError(LayerGeometryProvider *) const { return ""; }

    virtual void setObjectName(const QString &name);

    /**
     * Convert the layer's data (though not those of the model it
     * refers to) into XML for file output.  This class implements the
     * basic name/type/model-id output; subclasses will typically call
     * this superclass implementation with extra attributes describing
     * their particular properties.
     */
    void toXml(QTextStream &stream, QString indent = "",
                       QString extraAttributes = "") const override;

    /**
     * Set the particular properties of a layer (those specific to the
     * subclass) from a set of XML attributes.  This is the effective
     * inverse of the toXml method.
     */
    virtual void setProperties(const QXmlAttributes &) = 0;

    /**
     * Produce XML containing the layer's ID and type.  This is used
     * to refer to the layer in the display section of the SV session
     * file, for a layer that has already been described in the data
     * section.
     */
    virtual void toBriefXml(QTextStream &stream,
                            QString indent = "",
                            QString extraAttributes = "") const;

    /**
     * Add a measurement rectangle from the given XML attributes
     * (presumably taken from a measurement element).
     * Does not use a command.
     */
    virtual void addMeasurementRect(const QXmlAttributes &);

    /**
     * Indicate that a layer is not currently visible in the given
     * view and is not expected to become visible in the near future
     * (for example because the user has explicitly removed or hidden
     * it).  The layer may respond by (for example) freeing any cache
     * memory it is using, until next time its paint method is called,
     * when it should set itself un-dormant again.
     *
     * A layer class that overrides this function must also call this
     * class's implementation.
     */
    virtual void setLayerDormant(const LayerGeometryProvider *v, bool dormant);

    /**
     * Return whether the layer is dormant (i.e. hidden) in the given
     * view.
     */
    virtual bool isLayerDormant(const LayerGeometryProvider *v) const;

    /**
     * Return the play parameters for this layer, if any. The return
     * value is a shared_ptr that can be passed to (e.g.)
     * PlayParameterRepository::EditCommand to change the parameters.
     */
    std::shared_ptr<PlayParameters> getPlayParameters() override;

    /**
     * True if this layer will need to place text labels when it is
     * painted. The view will take into account how many layers are
     * requesting this, and will provide a distinct y-coord to each
     * layer on request via View::getTextLabelHeight().
     */
    virtual bool needsTextLabelHeight() const { return false; }

    /**
     * Return true if the X axis on the layer is time proportional to
     * audio frames, false otherwise. Almost all layer types return
     * true here: the exceptions are spectrum and slice layers.
     */
    virtual bool hasTimeXAxis() const { return true; }

    /**
     * Update the X and Y axis scales, where appropriate, to focus on
     * the given rectangular region. This should *only* be overridden
     * by layers whose hasTimeXAxis() returns false - the pane handles
     * zooming appropriately in every "normal" case.
     */
    virtual void zoomToRegion(const LayerGeometryProvider *, QRect) {
        return;
    }

    /**
     * Return the minimum and maximum values for the y axis of the
     * model in this layer, as well as whether the layer is configured
     * to use a logarithmic y axis display.  Also return the unit for
     * these values if known.
     *
     * This function returns the "normal" extents for the layer, not
     * necessarily the extents actually in use in the display (see
     * getDisplayExtents).
     */
    virtual bool getValueExtents(double &min, double &max,
                                 bool &logarithmic, QString &unit) const = 0;

    /**
     * Return the minimum and maximum values within the visible area
     * for the y axis of this layer.
     *
     * Return false if the layer has no display extents of its
     * own. This could be because the layer is "auto-aligning" against
     * another layer with the same units elsewhere in the view, or
     * because the layer has no concept of a vertical scale at all.
     */
    virtual bool getDisplayExtents(double & /* min */,
                                   double & /* max */) const {
        return false;
    }

    /**
     * Set the displayed minimum and maximum values for the y axis to
     * the given range, if supported.  Return false if not supported
     * on this layer (and set nothing).  In most cases, layers that
     * return false for getDisplayExtents should also return false for
     * this function.
     */
    virtual bool setDisplayExtents(double /* min */,
                                   double /* max */) {
        return false;
    }

    /**
     * Consider using the given value extents and units for this
     * layer. This may be called on a new layer when added, to prepare
     * it for editing, and the extents are those of the layer
     * underneath it. May not be appropriate for most layer types.
     */
    virtual bool adoptExtents(double /* min */, double /* max */,
                              QString /* unit */) {
        return false;
    }
    
    /**
     * Return the value and unit at the given x coordinate in the
     * given view.  This is for descriptive purposes using the
     * measurement tool.  The default implementation works correctly
     * if the layer hasTimeXAxis().
     */
    virtual bool getXScaleValue(const LayerGeometryProvider *v, int x,
                                double &value, QString &unit) const;

    /** 
     * Return the value and unit at the given y coordinate in the
     * given view.
     */
    virtual bool getYScaleValue(const LayerGeometryProvider *, int /* y */,
                                double &/* value */, QString &/* unit */) const {
        return false;
    }

    /**
     * Return the difference between the values at the given y
     * coordinates in the given view, and the unit of the difference.
     * The default implementation just calls getYScaleValue twice and
     * returns the difference, with the same unit.
     */
    virtual bool getYScaleDifference(const LayerGeometryProvider *v, int y0, int y1,
                                     double &diff, QString &unit) const;
        
    /**
     * Get the number of vertical zoom steps available for this layer.
     * If vertical zooming is not available, return 0.  The meaning of
     * "zooming" is entirely up to the layer -- changing the zoom
     * level may cause the layer to reset its display extents or
     * change another property such as display gain.  However, layers
     * are advised for consistency to treat smaller zoom steps as
     * "more distant" or "zoomed out" and larger ones as "closer" or
     * "zoomed in".
     * 
     * Layers that provide this facility should also emit the
     * verticalZoomChanged signal if their vertical zoom changes
     * due to factors other than setVerticalZoomStep being called.
     */
    virtual int getVerticalZoomSteps(int & /* defaultStep */) const { return 0; }

    /**
     * Get the current vertical zoom step.  A layer may support finer
     * control over ranges etc than is available through the integer
     * zoom step mechanism; if this one does, it should just return
     * the nearest of the available zoom steps to the current settings.
     */
    virtual int getCurrentVerticalZoomStep() const { return 0; }

    /**
     * Set the vertical zoom step.  The meaning of "zooming" is
     * entirely up to the layer -- changing the zoom level may cause
     * the layer to reset its display extents or change another
     * property such as display gain.
     */
    virtual void setVerticalZoomStep(int) { }

    /**
     * Create and return a range mapper for vertical zoom step values.
     * See the RangeMapper documentation for more details.  The
     * returned value is allocated on the heap and will be deleted by
     * the caller.
     */
    virtual RangeMapper *getNewVerticalZoomRangeMapper() const { return 0; }

    /**
     * Return true if this layer type can function without a model
     * being set. If false (the default), the layer will not be loaded
     * from a session if its model cannot be found.
     */
    virtual bool canExistWithoutModel() const { return false; }

public slots:
    /**
     * Change the visibility status (dormancy) of the layer in the
     * given view.
     */
    void showLayer(LayerGeometryProvider *, bool show);

signals:
    void modelChanged(ModelId);
    void modelCompletionChanged(ModelId);
    void modelAlignmentCompletionChanged(ModelId);
    void modelChangedWithin(ModelId, sv_frame_t startFrame, sv_frame_t endFrame);
    void modelReplaced();

    void layerParametersChanged();
    void layerParameterRangesChanged();
    void layerMeasurementRectsChanged();
    void layerNameChanged();

    void verticalZoomChanged();

protected:
    void connectSignals(ModelId);

    virtual sv_frame_t alignToReference(LayerGeometryProvider *v, sv_frame_t frame) const;
    virtual sv_frame_t alignFromReference(LayerGeometryProvider *v, sv_frame_t frame) const;
    bool clipboardHasDifferentAlignment(LayerGeometryProvider *v, const Clipboard &clip) const;

    struct MeasureRect {

        mutable QRect pixrect;
        bool haveFrames;
        sv_frame_t startFrame; // only valid if haveFrames
        sv_frame_t endFrame;   // ditto
        double startY;
        double endY;

        bool operator<(const MeasureRect &mr) const;
        void toXml(QTextStream &stream, QString indent) const;
    };

    class AddMeasurementRectCommand : public Command
    {
    public:
        AddMeasurementRectCommand(Layer *layer, MeasureRect rect) :
            m_layer(layer), m_rect(rect) { }

        QString getName() const override;
        void execute() override;
        void unexecute() override;

    private:
        Layer *m_layer;
        MeasureRect m_rect;
    };

    class DeleteMeasurementRectCommand : public Command
    {
    public:
        DeleteMeasurementRectCommand(Layer *layer, MeasureRect rect) :
            m_layer(layer), m_rect(rect) { }

        QString getName() const override;
        void execute() override;
        void unexecute() override;

    private:
        Layer *m_layer;
        MeasureRect m_rect;
    };

    void addMeasureRectToSet(const MeasureRect &r) {
        m_measureRects.insert(r);
        emit layerMeasurementRectsChanged();
    }

    void deleteMeasureRectFromSet(const MeasureRect &r) {
        m_measureRects.erase(r); 
        emit layerMeasurementRectsChanged();
    }

    typedef std::set<MeasureRect> MeasureRectSet;
    MeasureRectSet m_measureRects;
    MeasureRect m_draggingRect;
    bool m_haveDraggingRect;
    mutable bool m_haveCurrentMeasureRect;
    mutable QPoint m_currentMeasureRectPoint;
   
    // Note that pixrects are only correct for a single view.
    // So we should update them at the start of the paint procedure
    // (painting is single threaded) and only use them after that.
    void updateMeasurePixrects(LayerGeometryProvider *v) const;

    virtual void updateMeasureRectYCoords(LayerGeometryProvider *v, const MeasureRect &r) const;
    virtual void setMeasureRectYCoord(LayerGeometryProvider *v, MeasureRect &r, bool start, int y) const;
    virtual void setMeasureRectFromPixrect(LayerGeometryProvider *v, MeasureRect &r, QRect pixrect) const;

    // This assumes updateMeasurementPixrects has been called
    MeasureRectSet::const_iterator findFocusedMeasureRect(QPoint) const;

    void paintMeasurementRect(LayerGeometryProvider *v, QPainter &paint,
                              const MeasureRect &r, bool focus) const;

    bool valueExtentsMatchMine(LayerGeometryProvider *v) const;
    
    QString m_presentationName;

private:
    mutable QMutex m_dormancyMutex;
    mutable std::map<const void *, bool> m_dormancy;
};

#endif

