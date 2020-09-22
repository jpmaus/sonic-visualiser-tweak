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

#ifndef SV_PANE_H
#define SV_PANE_H

#include <QFrame>
#include <QPoint>

#include "base/ZoomConstraint.h"
#include "View.h"
#include "base/Selection.h"

class QWidget;
class QPaintEvent;
class Layer;
class Thumbwheel;
class Panner;
class NotifyingPushButton;
class KeyReference;

class Pane : public View
{
    Q_OBJECT

public:
    Pane(QWidget *parent = 0);
    virtual QString getPropertyContainerIconName() const override { return "pane"; }

    virtual bool shouldIlluminateLocalFeatures(const Layer *layer,
                                               QPoint &pos) const override;
    virtual bool shouldIlluminateLocalSelection(QPoint &pos,
                                                bool &closeToLeft,
                                                bool &closeToRight) const override;

    void setCentreLineVisible(bool visible);
    bool getCentreLineVisible() const { return m_centreLineVisible; }

    virtual sv_frame_t getFirstVisibleFrame() const override;

    int getVerticalScaleWidth() const;

    virtual QImage *renderToNewImage() override {
        return View::renderToNewImage();
    }
    
    virtual QImage *renderPartToNewImage(sv_frame_t f0, sv_frame_t f1) override;

    virtual QSize getRenderedImageSize() override {
        return View::getRenderedImageSize();
    }
    
    virtual QSize getRenderedPartImageSize(sv_frame_t f0, sv_frame_t f1) override;

    virtual void toXml(QTextStream &stream, QString indent = "",
                       QString extraAttributes = "") const override;

    static void registerShortcuts(KeyReference &kr);

    enum PaneType {
        Normal = 0,
        TonyMain = 1,
        TonySelection = 2
    };

signals:
    void paneInteractedWith();
    void rightButtonMenuRequested(QPoint position);
    void dropAccepted(QStringList uriList);
    void dropAccepted(QString text);
    void doubleClickSelectInvoked(sv_frame_t frame);
    void regionOutlined(QRect rect);

public slots:
    // view slots
    virtual void toolModeChanged() override;
    virtual void zoomWheelsEnabledChanged() override;
    virtual void viewZoomLevelChanged(View *, ZoomLevel, bool locked) override;
    virtual void modelAlignmentCompletionChanged(ModelId) override;

    // local slots, not overrides
    virtual void horizontalThumbwheelMoved(int value);
    virtual void verticalThumbwheelMoved(int value);
    virtual void verticalZoomChanged();
    virtual void verticalPannerMoved(float x, float y, float w, float h);
    virtual void editVerticalPannerExtents();

    virtual void layerParametersChanged() override;

    virtual void propertyContainerSelected(View *, PropertyContainer *pc) override;

    void zoomToRegion(QRect r);

    void mouseEnteredWidget();
    void mouseLeftWidget();

protected slots:
    void playbackScheduleTimerElapsed();

protected:
    virtual void paintEvent(QPaintEvent *e) override;
    virtual void mousePressEvent(QMouseEvent *e) override;
    virtual void mouseReleaseEvent(QMouseEvent *e) override;
    virtual void mouseMoveEvent(QMouseEvent *e) override;
    virtual void mouseDoubleClickEvent(QMouseEvent *e) override;
    virtual void enterEvent(QEvent *e) override;
    virtual void leaveEvent(QEvent *e) override;
    virtual void wheelEvent(QWheelEvent *e) override;
    virtual void resizeEvent(QResizeEvent *e) override;
    virtual void dragEnterEvent(QDragEnterEvent *e) override;
    virtual void dropEvent(QDropEvent *e) override;

    void wheelVertical(int sign, Qt::KeyboardModifiers);
    void wheelHorizontal(int sign, Qt::KeyboardModifiers);
    void wheelHorizontalFine(int pixels, Qt::KeyboardModifiers);

    void drawVerticalScale(QRect r, Layer *, QPainter &);
    void drawFeatureDescription(Layer *, QPainter &);
    void drawCentreLine(sv_samplerate_t, QPainter &, bool omitLine);
    void drawModelTimeExtents(QRect, QPainter &, ModelId);
    void drawDurationAndRate(QRect, ModelId, sv_samplerate_t, QPainter &);
    void drawWorkTitle(QRect, QPainter &, ModelId);
    void drawLayerNames(QRect, QPainter &);
    void drawEditingSelection(QPainter &);
    void drawAlignmentStatus(QRect, QPainter &, ModelId, bool down);

    virtual bool render(QPainter &paint, int x0, sv_frame_t f0, sv_frame_t f1) override;

    Selection getSelectionAt(int x, bool &closeToLeft, bool &closeToRight) const;

    bool editSelectionStart(QMouseEvent *e);
    bool editSelectionDrag(QMouseEvent *e);
    bool editSelectionEnd(QMouseEvent *e);
    bool selectionIsBeingEdited() const;

    void updateHeadsUpDisplay();
    void updateVerticalPanner();

    bool canTopLayerMoveVertical();
    bool setTopLayerDisplayExtents(double displayMin, double displayMax);
    bool getTopLayerDisplayExtents(double &valueMin, double &valueMax,
                                   double &displayMin, double &displayMax,
                                   QString *unit = 0);

    void dragTopLayer(QMouseEvent *e);
    void dragExtendSelection(QMouseEvent *e);
    void updateContextHelp(const QPoint *pos);
    void edgeScrollMaybe(int x);

    Layer *getTopFlexiNoteLayer();

    void schedulePlaybackFrameMove(sv_frame_t frame);

    bool m_identifyFeatures;
    QPoint m_identifyPoint;
    QPoint m_clickPos;
    QPoint m_mousePos;
    bool m_clickedInRange;
    bool m_shiftPressed;
    bool m_ctrlPressed;
    bool m_altPressed;

    bool m_navigating;
    bool m_resizing;
    bool m_editing;
    bool m_releasing;
    sv_frame_t m_dragCentreFrame;
    double m_dragStartMinValue;
    bool m_centreLineVisible;
    sv_frame_t m_selectionStartFrame;
    Selection m_editingSelection;
    int m_editingSelectionEdge;
    mutable int m_scaleWidth;

    int m_pendingWheelAngle;

    enum DragMode {
        UnresolvedDrag,
        VerticalDrag,
        HorizontalDrag,
        FreeDrag
    };
    DragMode m_dragMode;

    DragMode updateDragMode(DragMode currentMode,
                            QPoint origin,
                            QPoint currentPoint,
                            bool canMoveHorizontal,
                            bool canMoveVertical,
                            bool resistHorizontal,
                            bool resistVertical);

    QWidget *m_headsUpDisplay;
    Panner *m_vpan;
    Thumbwheel *m_hthumb;
    Thumbwheel *m_vthumb;
    NotifyingPushButton *m_reset;

    bool m_mouseInWidget;

    bool m_playbackFrameMoveScheduled;
    sv_frame_t m_playbackFrameMoveTo;
    
    static QCursor *m_measureCursor1;
    static QCursor *m_measureCursor2;
};

#endif

