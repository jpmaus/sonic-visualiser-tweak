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

#ifndef SV_OVERVIEW_H
#define SV_OVERVIEW_H

#include "View.h"

#include <QPoint>
#include <QTime>

class QWidget;
class QPaintEvent;
class Layer;
class View;

#include <map>

class Overview : public View
{
    Q_OBJECT

public:
    Overview(QWidget *parent = 0);

    void registerView(View *view);
    void unregisterView(View *view);

    QString getPropertyContainerIconName() const override { return "panner"; }

public slots:
    void modelChangedWithin(ModelId, sv_frame_t startFrame, sv_frame_t endFrame) override;
    void modelReplaced() override;

    void globalCentreFrameChanged(sv_frame_t) override;
    void viewCentreFrameChanged(View *, sv_frame_t) override;
    void viewZoomLevelChanged(View *, ZoomLevel, bool) override;
    void viewManagerPlaybackFrameChanged(sv_frame_t) override;

    virtual void setBoxColour(QColor);
    
protected:
    void paintEvent(QPaintEvent *e) override;
    void mousePressEvent(QMouseEvent *e) override;
    void mouseReleaseEvent(QMouseEvent *e) override;
    void mouseMoveEvent(QMouseEvent *e) override;
    void mouseDoubleClickEvent(QMouseEvent *e) override;
    void enterEvent(QEvent *) override;
    void leaveEvent(QEvent *) override;
    bool shouldLabelSelections() const override { return false; }

    QColor getFillWithin() const;
    QColor getFillWithout() const;
    
    QPoint m_clickPos;
    QPoint m_mousePos;
    bool m_clickedInRange;
    sv_frame_t m_dragCentreFrame;
    QTime m_modelTestTime;
    QColor m_boxColour;
    
    typedef std::set<View *> ViewSet;
    ViewSet m_views;
};

#endif

