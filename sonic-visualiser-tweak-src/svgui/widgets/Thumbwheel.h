/* -*- c-basic-offset: 4 indent-tabs-mode: nil -*-  vi:set ts=8 sts=4 sw=4: */

/*
    Sonic Visualiser
    An audio file viewer and annotation editor.
    Centre for Digital Music, Queen Mary, University of London.
    This file copyright 2006 QMUL.
    
    This program is free software; you can redistribute it and/or
    modify it under the terms of the GNU General Public License as
    published by the Free Software Foundation; either version 2 of the
    License, or (at your option) any later version.  See the file
    COPYING included with this distribution for more information.
*/

#ifndef SV_THUMBWHEEL_H
#define SV_THUMBWHEEL_H

#include <QWidget>
#include <QImage>

#include <map>

#include "WheelCounter.h"

class RangeMapper;

class Thumbwheel : public QWidget
{
    Q_OBJECT

public:
    Thumbwheel(Qt::Orientation orientation, QWidget *parent = 0);
    virtual ~Thumbwheel();

    int getMinimumValue() const;
    int getMaximumValue() const;
    int getDefaultValue() const;
    float getSpeed() const;
    bool getTracking() const;
    bool getShowScale() const;
    int getValue() const;

    void setRangeMapper(RangeMapper *mapper); // I take ownership, will delete
    const RangeMapper *getRangeMapper() const { return m_rangeMapper; }
    double getMappedValue() const;

    void setShowToolTip(bool show);

    QSize sizeHint() const override;

signals:
    void valueChanged(int);

    void mouseEntered();
    void mouseLeft();

public slots:
    void setMinimumValue(int min);
    void setMaximumValue(int max);
    void setDefaultValue(int deft);
    void setSpeed(float speed);
    void setTracking(bool tracking);
    void setShowScale(bool show);
    void setValue(int value);
    void setMappedValue(double mappedValue);
    void scroll(bool up);
    void resetToDefault();

protected slots:
    void updateMappedValue(int value);

protected:
    void mousePressEvent(QMouseEvent *e) override;
    void mouseDoubleClickEvent(QMouseEvent *e) override;
    void mouseMoveEvent(QMouseEvent *e) override;
    void mouseReleaseEvent(QMouseEvent *e) override;
    void wheelEvent(QWheelEvent *e) override;
    void paintEvent(QPaintEvent *e) override;
    void enterEvent(QEvent *) override;
    void leaveEvent(QEvent *) override;

    int m_min;
    int m_max;
    int m_default;
    int m_value;
    double m_mappedValue;
    bool m_noMappedUpdate;
    float m_rotation;
    Qt::Orientation m_orientation;
    float m_speed;
    bool m_tracking;
    bool m_showScale;
    bool m_clicked;
    bool m_atDefault;
    QPoint m_clickPos;
    float m_clickRotation;
    bool m_showTooltip;
    RangeMapper *m_rangeMapper;
    QImage m_cache;
    WheelCounter m_wheelCounter;
};

#endif
