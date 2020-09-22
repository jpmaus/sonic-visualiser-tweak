/* -*- c-basic-offset: 4 indent-tabs-mode: nil -*-  vi:set ts=8 sts=4 sw=4: */

/*
    Sonic Visualiser
    An audio file viewer and annotation editor.
    Centre for Digital Music, Queen Mary, University of London.
    This file copyright 2006-2014 Chris Cannam and QMUL.
    
    This program is free software; you can redistribute it and/or
    modify it under the terms of the GNU General Public License as
    published by the Free Software Foundation; either version 2 of the
    License, or (at your option) any later version.  See the file
    COPYING included with this distribution for more information.
*/

#ifndef ALIGNMENT_VIEW_H
#define ALIGNMENT_VIEW_H

#include "View.h"

class AlignmentView : public View
{
    Q_OBJECT

public:
    AlignmentView(QWidget *parent = 0);
    QString getPropertyContainerIconName() const override { return "alignment"; }
    
    void setViewAbove(View *view);
    void setViewBelow(View *view);

public slots:
    void globalCentreFrameChanged(sv_frame_t) override;
    void viewCentreFrameChanged(View *, sv_frame_t) override;
    
    virtual void viewAboveZoomLevelChanged(ZoomLevel, bool);
    virtual void viewBelowZoomLevelChanged(ZoomLevel, bool);
    
    void viewManagerPlaybackFrameChanged(sv_frame_t) override;

    void keyFramesChanged();

protected:
    void paintEvent(QPaintEvent *e) override;
    bool shouldLabelSelections() const override { return false; }

    void buildKeyFrameMap();

    std::vector<sv_frame_t> getKeyFrames(View *, sv_frame_t &resolution);
    std::vector<sv_frame_t> getDefaultKeyFrames();

    ModelId getSalientModel(View *);

    void reconnectModels();

    View *m_above;
    View *m_below;

    QMutex m_keyFrameMutex;
    std::multimap<sv_frame_t, sv_frame_t> m_keyFrameMap;
};

#endif
