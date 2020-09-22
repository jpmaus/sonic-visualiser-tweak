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

#ifndef SV_VIEW_H
#define SV_VIEW_H

#include <QFrame>
#include <QProgressBar>

#include "layer/LayerGeometryProvider.h"

#include "base/ZoomConstraint.h"
#include "base/PropertyContainer.h"
#include "ViewManager.h"
#include "base/XmlExportable.h"
#include "base/BaseTypes.h"

#include "data/model/Model.h"

// #define DEBUG_VIEW_WIDGET_PAINT 1

class Layer;
class ViewPropertyContainer;

class QPushButton;

#include <map>
#include <set>

/**
 * View is the base class of widgets that display one or more
 * overlaid views of data against a horizontal time scale. 
 *
 * A View may have any number of attached Layers, each of which
 * is expected to have one data Model (although multiple views may
 * share the same model).
 *
 * A View may be panned in time and zoomed, although the
 * mechanisms for doing so (as well as any other operations and
 * properties available) depend on the subclass.
 */

class View : public QFrame,
             public XmlExportable,
             public LayerGeometryProvider
{
    Q_OBJECT

public:
    /**
     * Deleting a View does not delete any of its layers.  They should
     * be managed elsewhere (e.g. by the Document).
     */
    virtual ~View();

    /**
     * Retrieve the id of this object. Views have their own unique
     * ids, but ViewProxy objects share the id of their View.
     */
    int getId() const override { return m_id; }
    
    /**
     * Retrieve the first visible sample frame on the widget.
     * This is a calculated value based on the centre-frame, widget
     * width and zoom level.  The result may be negative.
     */
    sv_frame_t getStartFrame() const override;

    /**
     * Set the widget pan based on the given first visible frame.  The
     * frame value may be negative.
     */
    void setStartFrame(sv_frame_t);

    /**
     * Return the centre frame of the visible widget.  This is an
     * exact value that does not depend on the zoom block size.  Other
     * frame values (start, end) are calculated from this based on the
     * zoom and other factors.
     */
    sv_frame_t getCentreFrame() const override { return m_centreFrame; }

    /**
     * Set the centre frame of the visible widget.
     */
    void setCentreFrame(sv_frame_t f) { setCentreFrame(f, true); }

    /**
     * Retrieve the last visible sample frame on the widget.
     * This is a calculated value based on the centre-frame, widget
     * width and zoom level.
     */
    sv_frame_t getEndFrame() const override;

    /**
     * Return the pixel x-coordinate corresponding to a given sample
     * frame. The frame is permitted to be negative, and the result
     * may be outside the currently visible area. But this should not
     * be called with frame values very far away from the currently
     * visible area, as that could lead to overflow. In that situation
     * an error will be logged and 0 returned.
     */
    int getXForFrame(sv_frame_t frame) const override;

    /**
     * Return the closest frame to the given pixel x-coordinate.
     */
    sv_frame_t getFrameForX(int x) const override;

    /**
     * Return the closest pixel x-coordinate corresponding to a given
     * view x-coordinate. Default is no scaling, ViewProxy handles
     * scaling case.
     */
    int getXForViewX(int viewx) const override { return viewx; }

    /**
     * Return the closest view x-coordinate corresponding to a given
     * pixel x-coordinate. Default is no scaling, ViewProxy handles
     * scaling case.
     */
    int getViewXForX(int x) const override { return x; }

    /**
     * Return the pixel y-coordinate corresponding to a given
     * frequency, if the frequency range is as specified.  This does
     * not imply any policy about layer frequency ranges, but it might
     * be useful for layers to match theirs up if desired.
     *
     * Not thread-safe in logarithmic mode.  Call only from GUI thread.
     */
    double getYForFrequency(double frequency, double minFreq, double maxFreq, 
                           bool logarithmic) const override;

    /**
     * Return the closest frequency to the given pixel y-coordinate,
     * if the frequency range is as specified.
     *
     * Not thread-safe in logarithmic mode.  Call only from GUI thread.
     */
    double getFrequencyForY(double y, double minFreq, double maxFreq,
                            bool logarithmic) const override;

    /**
     * Return the zoom level, i.e. the number of frames per pixel or
     * pixels per frame
     */
    ZoomLevel getZoomLevel() const override;

    /**
     * Set the zoom level, i.e. the number of frames per pixel or
     * pixels per frame.  The centre frame will be unchanged; the
     * start and end frames will change.
     */
    virtual void setZoomLevel(ZoomLevel z);

    /**
     * Zoom in or out.
     */
    virtual void zoom(bool in);

    /**
     * Scroll left or right by a smallish or largish amount.
     */
    virtual void scroll(bool right, bool lots, bool doEmit = true);

    /**
     * Add a layer to the view. (Normally this should be handled
     * through some command abstraction instead of using this function
     * directly.)
     */
    virtual void addLayer(Layer *v);

    /**
     * Remove a layer from the view. Does not delete the
     * layer. (Normally this should be handled through some command
     * abstraction instead of using this function directly.)
     */
    virtual void removeLayer(Layer *v);

    /**
     * Return the number of layers, regardless of whether visible or
     * dormant, i.e. invisible, in this view.
     */
    virtual int getLayerCount() const { return int(m_layerStack.size()); }

    /**
     * Return the nth layer, counted in stacking order.  That is,
     * layer 0 is the bottom layer and layer "getLayerCount()-1" is
     * the top one. The returned layer may be visible or it may be
     * dormant, i.e. invisible.
     */
    virtual Layer *getLayer(int n) {
        if (in_range_for(m_layerStack, n)) return m_layerStack[n];
        else return 0;
    }

    /**
     * Return the nth layer, counted in the order they were
     * added. Unlike the stacking order used in getLayer(), which
     * changes each time a layer is selected, this ordering remains
     * fixed. The returned layer may be visible or it may be dormant,
     * i.e. invisible.
     */
    virtual Layer *getFixedOrderLayer(int n) {
        if (n < int(m_fixedOrderLayers.size())) return m_fixedOrderLayers[n];
        else return 0;
    }

    /**
     * Return the layer currently active for tool interaction. This is
     * the topmost non-dormant (i.e. visible) layer in the view. If
     * there are no visible layers in the view, return 0.
     */
    virtual Layer *getInteractionLayer();

    virtual const Layer *getInteractionLayer() const;

    /**
     * Return the layer most recently selected by the user. This is
     * the layer that any non-tool-driven commands should operate on,
     * in the case where this view is the "current" one.
     *
     * If the user has selected the view itself more recently than any
     * of the layers on it, this function will return 0, and any
     * non-tool-driven layer commands should be deactivated while this
     * view is current. It will also return 0 if there are no layers
     * in the view.
     *
     * Note that, unlike getInteractionLayer(), this could return an
     * invisible (dormant) layer.
     */
    virtual Layer *getSelectedLayer();

    virtual const Layer *getSelectedLayer() const;

    /**
     * Return the "top" layer in the view, whether visible or dormant.
     * This is the same as getLayer(getLayerCount()-1) if there is at
     * least one layer, and 0 otherwise.
     *
     * For most purposes involving interaction or commands, you
     * probably want either getInteractionLayer() or
     * getSelectedLayer() instead.
     */
    virtual Layer *getTopLayer() {
        return m_layerStack.empty() ? 0 : m_layerStack[m_layerStack.size()-1];
    }

    virtual void setViewManager(ViewManager *m);
    virtual void setViewManager(ViewManager *m, sv_frame_t initialFrame);
    ViewManager *getViewManager() const override { return m_manager; }

    virtual void setFollowGlobalPan(bool f);
    virtual bool getFollowGlobalPan() const { return m_followPan; }

    virtual void setFollowGlobalZoom(bool f);
    virtual bool getFollowGlobalZoom() const { return m_followZoom; }

    bool hasLightBackground() const override;
    QColor getForeground() const override;
    QColor getBackground() const override;

    void drawMeasurementRect(QPainter &p, const Layer *,
                                     QRect rect, bool focus) const override;

    bool shouldShowFeatureLabels() const override {
        return m_manager && m_manager->shouldShowFeatureLabels();
    }
    bool shouldIlluminateLocalFeatures(const Layer *, QPoint &) const override {
        return false;
    }
    virtual bool shouldIlluminateLocalSelection(QPoint &, bool &, bool &) const {
        return false;
    }

    virtual void setPlaybackFollow(PlaybackFollowMode m);
    virtual PlaybackFollowMode getPlaybackFollow() const { return m_followPlay; }

    typedef PropertyContainer::PropertyName PropertyName;

    // We implement the PropertyContainer API, although we don't
    // actually subclass PropertyContainer.  We have our own
    // PropertyContainer that we can return on request that just
    // delegates back to us.
    virtual PropertyContainer::PropertyList getProperties() const;
    virtual QString getPropertyLabel(const PropertyName &) const;
    virtual PropertyContainer::PropertyType getPropertyType(const PropertyName &) const;
    virtual int getPropertyRangeAndValue(const PropertyName &,
                                         int *min, int *max, int *deflt) const;
    virtual QString getPropertyValueLabel(const PropertyName &,
                                          int value) const;
    virtual void setProperty(const PropertyName &, int value);
    virtual QString getPropertyContainerName() const {
        return objectName();
    }
    virtual QString getPropertyContainerIconName() const = 0;

    virtual int getPropertyContainerCount() const;

    // The 0th property container is the view's own; the rest are the
    // layers in fixed-order series
    virtual const PropertyContainer *getPropertyContainer(int i) const;
    virtual PropertyContainer *getPropertyContainer(int i);

    /** 
     * Render the view contents to a new QImage (which may be wider
     * than the visible View).
     */
    virtual QImage *renderToNewImage();

    /** 
     * Render the view contents between the given frame extents to a
     * new QImage (which may be wider than the visible View).
     */
    virtual QImage *renderPartToNewImage(sv_frame_t f0, sv_frame_t f1);

    /**
     * Calculate and return the size of image that will be generated
     * by renderToNewImage().
     */
    virtual QSize getRenderedImageSize();

    /**
     * Calculate and return the size of image that will be generated
     * by renderPartToNewImage(f0, f1).
     */
    virtual QSize getRenderedPartImageSize(sv_frame_t f0, sv_frame_t f1);

    /**
     * Render the view contents to a new SVG file.
     */
    virtual bool renderToSvgFile(QString filename);

    /**
     * Render the view contents between the given frame extents to a
     * new SVG file.
     */
    virtual bool renderPartToSvgFile(QString filename,
                                     sv_frame_t f0, sv_frame_t f1);

    /**
     * Return the visible vertical extents for the given unit, if any.
     * Overridden from LayerGeometryProvider (see docs there).
     */
    bool getVisibleExtentsForUnit(QString unit, double &min, double &max,
                                  bool &log) const override;

    /**
     * Return some visible vertical extents and unit. That is, if at
     * least one non-dormant layer has a non-empty unit and returns
     * some values from its getDisplayExtents() method, return the
     * extents and unit from the topmost of those. Otherwise return
     * false.
     */
    bool getVisibleExtentsForAnyUnit(double &min, double &max,
                                     bool &logarithmic, QString &unit) const;
    
    int getTextLabelYCoord(const Layer *layer, QPainter &) const override;

    void toXml(QTextStream &stream, QString indent = "",
                       QString extraAttributes = "") const override;

    // First frame actually in model, to right of scale, if present
    virtual sv_frame_t getFirstVisibleFrame() const;
    virtual sv_frame_t getLastVisibleFrame() const;

    sv_frame_t getModelsStartFrame() const override;
    sv_frame_t getModelsEndFrame() const override;

    /**
     * To be called from a layer, to obtain the extent of the surface
     * that the layer is currently painting to. This may be the extent
     * of the view (if 1x display scaling is in effect) or of a larger
     * cached pixmap (if greater display scaling is in effect).
     */
    QRect getPaintRect() const override;

    QSize getPaintSize() const override { return getPaintRect().size(); }
    int getPaintWidth() const override { return getPaintRect().width(); }
    int getPaintHeight() const override { return getPaintRect().height(); }

    double scaleSize(double size) const override;
    int scalePixelSize(int size) const override;
    double scalePenWidth(double width) const override;
    QPen scalePen(QPen pen) const override;

    typedef std::set<ModelId> ModelSet;
    ModelSet getModels();

    //!!!??? poor name, probably poor api, consider this
    void setUseAligningProxy(bool uap) {
        m_useAligningProxy = uap;
    }
    
    //!!!
    ModelId getAligningModel() const;
    void getAligningAndReferenceModels(ModelId &aligning, ModelId &reference) const;
    sv_frame_t alignFromReference(sv_frame_t) const;
    sv_frame_t alignToReference(sv_frame_t) const;
    sv_frame_t getAlignedPlaybackFrame() const;

    void updatePaintRect(QRect r) override { update(r); }
    
    View *getView() override { return this; } 
    const View *getView() const override { return this; } 
    
signals:
    void propertyContainerAdded(PropertyContainer *pc);
    void propertyContainerRemoved(PropertyContainer *pc);
    void propertyContainerPropertyChanged(PropertyContainer *pc);
    void propertyContainerPropertyRangeChanged(PropertyContainer *pc);
    void propertyContainerNameChanged(PropertyContainer *pc);
    void propertyContainerSelected(PropertyContainer *pc);
    void propertyChanged(PropertyContainer::PropertyName);

    void layerModelChanged();

    void cancelButtonPressed(Layer *);
    
    void centreFrameChanged(sv_frame_t frame,
                            bool globalScroll,
                            PlaybackFollowMode followMode);

    void zoomLevelChanged(ZoomLevel level, bool locked);

    void contextHelpChanged(const QString &);

public slots:
    virtual void modelChanged(ModelId);
    virtual void modelChangedWithin(ModelId, sv_frame_t startFrame, sv_frame_t endFrame);
    virtual void modelCompletionChanged(ModelId);
    virtual void modelAlignmentCompletionChanged(ModelId);
    virtual void modelReplaced();
    virtual void layerParametersChanged();
    virtual void layerParameterRangesChanged();
    virtual void layerMeasurementRectsChanged();
    virtual void layerNameChanged();

    virtual void globalCentreFrameChanged(sv_frame_t);
    virtual void viewCentreFrameChanged(View *, sv_frame_t);
    virtual void viewManagerPlaybackFrameChanged(sv_frame_t);
    virtual void viewZoomLevelChanged(View *, ZoomLevel, bool);

    /**
     * A property container has been selected, for example in the
     * associated property stack. The property container may be a
     * layer, in which case the effect should be to raise that layer
     * to the front of the view and select it; or it may be the view's
     * own property container, in which case the effect is to switch
     * to a mode in which no layer is selected.
     *
     * (This is the main slot for raising a layer.)
     */
    virtual void propertyContainerSelected(View *, PropertyContainer *pc);

    virtual void selectionChanged();
    virtual void toolModeChanged();
    virtual void overlayModeChanged();
    virtual void zoomWheelsEnabledChanged();

    virtual void cancelClicked();

    virtual void progressCheckStalledTimerElapsed();

protected:
    View(QWidget *, bool showProgress);

    int m_id;
    
    void paintEvent(QPaintEvent *e) override;
    virtual void drawSelections(QPainter &);
    virtual bool shouldLabelSelections() const { return true; }
    virtual void drawPlayPointer(QPainter &);
    virtual bool render(QPainter &paint, int x0, sv_frame_t f0, sv_frame_t f1);
    virtual void setPaintFont(QPainter &paint);

    QSize scaledSize(const QSize &s, int factor) {
        return QSize(s.width() * factor, s.height() * factor);
    }
    QRect scaledRect(const QRect &r, int factor) {
        return QRect(r.x() * factor, r.y() * factor,
                     r.width() * factor, r.height() * factor);
    }
    
    typedef std::vector<Layer *> LayerList;

    sv_samplerate_t getModelsSampleRate() const;
    bool areLayersScrollable() const;
    LayerList getScrollableBackLayers(bool testChanged, bool &changed) const;
    LayerList getNonScrollableFrontLayers(bool testChanged, bool &changed) const;

    Layer *getScaleProvidingLayerForUnit(QString unit) const;
    
    ZoomLevel getZoomConstraintLevel(ZoomLevel level,
                                     ZoomConstraint::RoundingDirection dir =
                                     ZoomConstraint::RoundNearest) const;

    // These three are slow, intended for indexing GUI thumbwheel stuff
    int countZoomLevels() const;
    int getZoomLevelIndex(ZoomLevel level) const;
    ZoomLevel getZoomLevelByIndex(int ix) const;
    
    // True if the top layer(s) use colours for meaningful things.  If
    // this is the case, selections will be shown using unfilled boxes
    // rather than with a translucent fill.
    bool areLayerColoursSignificant() const;

    // True if the top layer has a time axis on the x coordinate (this
    // is generally the case except for spectrum/slice layers).  It
    // will not be possible to make or display selections if this is
    // false.
    bool hasTopLayerTimeXAxis() const;

    bool setCentreFrame(sv_frame_t f, bool doEmit);

    void movePlayPointer(sv_frame_t f);

    void checkProgress(ModelId);
    void checkAlignmentProgress(ModelId);
    
    int getProgressBarWidth() const; // if visible

    int effectiveDevicePixelRatio() const;

    sv_frame_t          m_centreFrame;
    ZoomLevel           m_zoomLevel;
    bool                m_followPan;
    bool                m_followZoom;
    PlaybackFollowMode  m_followPlay;
    bool                m_followPlayIsDetached;
    sv_frame_t          m_playPointerFrame;
    bool                m_lightBackground;
    bool                m_showProgress;

    QPixmap            *m_cache;  // I own this
    QPixmap            *m_buffer; // I own this
    bool                m_cacheValid;
    sv_frame_t          m_cacheCentreFrame;
    ZoomLevel           m_cacheZoomLevel;
    bool                m_selectionCached;

    bool                m_deleting;

    LayerList           m_layerStack; // I don't own these, but see dtor note above
    LayerList           m_fixedOrderLayers;
    bool                m_haveSelectedLayer;

    bool                m_useAligningProxy;

    QString             m_lastError;

    // caches for use in getScrollableBackLayers, getNonScrollableFrontLayers
    mutable LayerList m_lastScrollableBackLayers;
    mutable LayerList m_lastNonScrollableBackLayers;

    struct ProgressBarRec {
        QPushButton *cancel;
        QProgressBar *bar;
        int lastStallCheckValue;
        QTimer *stallCheckTimer;
    };
    typedef std::map<Layer *, ProgressBarRec> ProgressMap;
    ProgressMap m_progressBars; // I own the ProgressBarRecs and their contents

    struct AlignmentProgressBarRec {
        ModelId alignedModel;
        QProgressBar *bar;
    };
    AlignmentProgressBarRec m_alignmentProgressBar;

    ViewManager *m_manager; // I don't own this
    ViewPropertyContainer *m_propertyContainer; // I own this
};


// Use this for delegation, because we can't subclass from
// PropertyContainer (which is a QObject) ourselves because of
// ambiguity with QFrame parent

class ViewPropertyContainer : public PropertyContainer
{
    Q_OBJECT

public:
    ViewPropertyContainer(View *v);
    virtual ~ViewPropertyContainer();

    PropertyList getProperties() const override { return m_v->getProperties(); }
    QString getPropertyLabel(const PropertyName &n) const override {
        return m_v->getPropertyLabel(n);
    }
    PropertyType getPropertyType(const PropertyName &n) const override {
        return m_v->getPropertyType(n);
    }
    int getPropertyRangeAndValue(const PropertyName &n, int *min, int *max,
                                 int *deflt) const override {
        return m_v->getPropertyRangeAndValue(n, min, max, deflt);
    }
    QString getPropertyValueLabel(const PropertyName &n, int value) const override {
        return m_v->getPropertyValueLabel(n, value);
    }
    QString getPropertyContainerName() const override {
        return m_v->getPropertyContainerName();
    }
    QString getPropertyContainerIconName() const override {
        return m_v->getPropertyContainerIconName();
    }

public slots:
    void setProperty(const PropertyName &n, int value) override {
        m_v->setProperty(n, value);
    }

protected:
    View *m_v;
};

#endif

