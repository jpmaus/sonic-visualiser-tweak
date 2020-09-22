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

#ifndef LEVEL_PAN_WIDGET_H
#define LEVEL_PAN_WIDGET_H

#include <QWidget>

#include "WheelCounter.h"

/**
 * A simple widget for coarse level and pan control.
 */

class LevelPanWidget : public QWidget
{
    Q_OBJECT

public:
    LevelPanWidget(QWidget *parent = 0);
    ~LevelPanWidget();
    
    /// Return level as a gain value. The basic level range is [0,1] but the
    /// gain scale may go up to 4.0
    float getLevel() const; 
    
    /// Return pan as a value in the range [-1,1]
    float getPan() const;

    /// Find out whether the widget is editable
    bool isEditable() const;

    /// Discover whether the level range includes muting or not
    bool includesMute() const;

    /// Draw a suitably sized copy of the widget's contents to the given device
    void renderTo(QPaintDevice *, QRectF, bool asIfEditable) const;

    QSize sizeHint() const override;
                                               
public slots:
    /// Set level. The basic level range is [0,1] but the scale may go
    /// higher. The value will be rounded.
    void setLevel(float);

    /// Set pan in the range [-1,1]. The value will be rounded
    void setPan(float);

    /// Set left and right peak monitoring levels in the range [0,1]
    void setMonitoringLevels(float, float);
    
    /// Specify whether the widget is editable or read-only (default editable)
    void setEditable(bool);

    /// Specify whether the level range should include muting or not
    void setIncludeMute(bool);

    /// Reset to default values
    void setToDefault();
    
    // public so it can be called from LevelPanToolButton (ew)
    void wheelEvent(QWheelEvent *ev) override;
    
signals:
    void levelChanged(float); // range [0,1]
    void panChanged(float); // range [-1,1]

    void mouseEntered();
    void mouseLeft();
    
protected:
    void mousePressEvent(QMouseEvent *ev) override;
    void mouseMoveEvent(QMouseEvent *ev) override;
    void mouseReleaseEvent(QMouseEvent *ev) override;
    void paintEvent(QPaintEvent *ev) override;
    void enterEvent(QEvent *) override;
    void leaveEvent(QEvent *) override;

    void emitLevelChanged();
    void emitPanChanged();

    int m_minNotch;
    int m_maxNotch;
    int m_notch;
    int m_pan;
    float m_monitorLeft;
    float m_monitorRight;
    bool m_editable;
    bool m_editing;
    bool m_includeMute;
    bool m_includeHalfSteps;

    WheelCounter m_wheelCounter;

    int clampNotch(int notch) const;
    int clampPan(int pan) const;

    int audioLevelToNotch(float audioLevel) const;
    float notchToAudioLevel(int notch) const;

    int audioPanToPan(float audioPan) const;
    float panToAudioPan(int pan) const;

    int coordsToNotch(QRectF rect, QPointF pos) const;
    int coordsToPan(QRectF rect, QPointF pos) const;

    QColor cellToColour(int cell) const;
    
    QSizeF cellSize(QRectF) const;
    QPointF cellCentre(QRectF, int row, int col) const;
    QSizeF cellLightSize(QRectF) const;
    QRectF cellLightRect(QRectF, int row, int col) const;
    QRectF cellOutlineRect(QRectF, int row, int col) const;
    double thinLineWidth(QRectF) const;
    double cornerRadius(QRectF) const;
};

#endif
