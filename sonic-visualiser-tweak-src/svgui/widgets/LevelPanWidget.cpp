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

#include "LevelPanWidget.h"

#include <QPainter>
#include <QMouseEvent>
#include <QWheelEvent>

#include "layer/ColourMapper.h"
#include "base/AudioLevel.h"

#include "WidgetScale.h"

#include <iostream>
#include <cmath>
#include <cassert>

using std::cerr;
using std::endl;

/**
 * Gain and pan scales:
 *
 * Gain: we have 5 circles vertically in the display, each of which
 * has half-circle and full-circle versions, and we also have "no
 * circles", so there are in total 11 distinct levels, which we refer
 * to as "notches" and number 0-10. (We use "notch" because "level" is
 * used by the external API to refer to audio gain.)
 * 
 * i.e. the levels are represented by these (schematic, rotated to
 * horizontal) displays:
 *
 *  0  X
 *  1  [
 *  2  []
 *  3  [][
 *  ...
 *  9  [][][][][
 *  10 [][][][][]
 * 
 * If we have mute enabled, then we map the range 0-10 to gain using
 * AudioLevel::fader_to_* with the ShortFader type, which treats fader
 * 0 as muted. If mute is disabled, then we map the range 1-10.
 * 
 * We can also disable half-circles, which leaves the range unchanged
 * but limits the notches to even values.
 * 
 * Pan: we have 5 columns with no finer resolution, so we only have 2
 * possible pan values on each side of centre.
 */

static const int maxPan = 2; // range is -maxPan to maxPan

LevelPanWidget::LevelPanWidget(QWidget *parent) :
    QWidget(parent),
    m_minNotch(0),
    m_maxNotch(10),
    m_notch(m_maxNotch),
    m_pan(0),
    m_monitorLeft(-1),
    m_monitorRight(-1),
    m_editable(true),
    m_editing(false),
    m_includeMute(true),
    m_includeHalfSteps(true)
{
    setToolTip(tr("Drag vertically to adjust level, horizontally to adjust pan"));
    setLevel(1.0);
    setPan(0.0);
}

LevelPanWidget::~LevelPanWidget()
{
}

void
LevelPanWidget::setToDefault()
{
    setLevel(1.0);
    setPan(0.0);
    emitLevelChanged();
    emitPanChanged();
}

QSize
LevelPanWidget::sizeHint() const
{
    return WidgetScale::scaleQSize(QSize(40, 40));
}

int
LevelPanWidget::clampNotch(int notch) const
{
    if (notch < m_minNotch) notch = m_minNotch;
    if (notch > m_maxNotch) notch = m_maxNotch;
    if (!m_includeHalfSteps) {
        notch = (notch / 2) * 2;
    }
    return notch;
}

int
LevelPanWidget::clampPan(int pan) const
{
    if (pan < -maxPan) pan = -maxPan;
    if (pan > maxPan) pan = maxPan;
    return pan;
}

int
LevelPanWidget::audioLevelToNotch(float audioLevel) const
{
    int notch = AudioLevel::multiplier_to_fader
        (audioLevel, m_maxNotch, AudioLevel::ShortFader);
    return clampNotch(notch);
}

float
LevelPanWidget::notchToAudioLevel(int notch) const
{
    return float(AudioLevel::fader_to_multiplier
                 (notch, m_maxNotch, AudioLevel::ShortFader));
}

void
LevelPanWidget::setLevel(float level)
{
    int notch = audioLevelToNotch(level);
    if (notch != m_notch) {
        m_notch = notch;
        float convertsTo = getLevel();
        if (fabsf(convertsTo - level) > 1e-5) {
            emitLevelChanged();
        }
        update();
    }
}

float
LevelPanWidget::getLevel() const
{
    return notchToAudioLevel(m_notch);
}

int
LevelPanWidget::audioPanToPan(float audioPan) const
{
    int pan = int(round(audioPan * maxPan));
    pan = clampPan(pan);
    return pan;
}

float
LevelPanWidget::panToAudioPan(int pan) const
{
    return float(pan) / float(maxPan);
}

void
LevelPanWidget::setPan(float fpan)
{
    int pan = audioPanToPan(fpan);
    if (pan != m_pan) {
        m_pan = pan;
        update();
    }
}

float
LevelPanWidget::getPan() const
{
    return panToAudioPan(m_pan);
}

void
LevelPanWidget::setMonitoringLevels(float left, float right)
{
    m_monitorLeft = left;
    m_monitorRight = right;
    update();
}

bool
LevelPanWidget::isEditable() const
{
    return m_editable;
}

bool
LevelPanWidget::includesMute() const
{
    return m_includeMute;
}

void
LevelPanWidget::setEditable(bool editable)
{
    m_editable = editable;
    update();
}

void
LevelPanWidget::setIncludeMute(bool include)
{
    m_includeMute = include;
    if (m_includeMute) {
        m_minNotch = 0;
    } else {
        m_minNotch = 1;
    }
    emitLevelChanged();
    update();
}

void
LevelPanWidget::emitLevelChanged()
{
    emit levelChanged(getLevel());
}

void
LevelPanWidget::emitPanChanged()
{
    emit panChanged(getPan());
}

void
LevelPanWidget::mousePressEvent(QMouseEvent *e)
{
    if (e->button() == Qt::MidButton ||
        ((e->button() == Qt::LeftButton) &&
         (e->modifiers() & Qt::ControlModifier))) {
        setToDefault();
    } else if (e->button() == Qt::LeftButton) {
        m_editing = true;
        mouseMoveEvent(e);
    }
}

void
LevelPanWidget::mouseReleaseEvent(QMouseEvent *e)
{
    mouseMoveEvent(e);
    m_editing = false;
}

void
LevelPanWidget::mouseMoveEvent(QMouseEvent *e)
{
    if (!m_editable) return;
    if (!m_editing) return;
    
    int notch = coordsToNotch(rect(), e->pos());
    int pan = coordsToPan(rect(), e->pos());

    if (notch == m_notch && pan == m_pan) {
        return;
    }
    if (notch != m_notch) {
        m_notch = notch;
        emitLevelChanged();
    }
    if (pan != m_pan) {
        m_pan = pan;
        emitPanChanged();
    }
    update();
}

void
LevelPanWidget::wheelEvent(QWheelEvent *e)
{
    int delta = m_wheelCounter.count(e);

    if (delta == 0) {
        return;
    }

    if (e->modifiers() & Qt::ControlModifier) {
        m_pan = clampPan(m_pan + delta);
        emitPanChanged();
        update();
    } else {
        m_notch = clampNotch(m_notch + delta);
        emitLevelChanged();
        update();
    }
}

int
LevelPanWidget::coordsToNotch(QRectF rect, QPointF loc) const
{
    double h = rect.height();

    int nnotch = m_maxNotch + 1;
    double cell = h / nnotch;

    int notch = int((h - (loc.y() - rect.y())) / cell);
    notch = clampNotch(notch);

    return notch;
}    

int
LevelPanWidget::coordsToPan(QRectF rect, QPointF loc) const
{
    double w = rect.width();

    int npan = maxPan * 2 + 1;
    double cell = w / npan;

    int pan = int((loc.x() - rect.x()) / cell) - maxPan;
    pan = clampPan(pan);

    return pan;
}

QSizeF
LevelPanWidget::cellSize(QRectF rect) const
{
    double w = rect.width(), h = rect.height();
    int ncol = maxPan * 2 + 1;
    int nrow = m_maxNotch/2;
    double wcell = w / ncol, hcell = h / nrow;
    return QSizeF(wcell, hcell);
}

QPointF
LevelPanWidget::cellCentre(QRectF rect, int row, int col) const
{
    QSizeF cs = cellSize(rect);
    return QPointF(rect.x() +
                   cs.width() * (col + maxPan) + cs.width() / 2.,
                   rect.y() + rect.height() -
                   cs.height() * (row + 1) + cs.height() / 2.);
}

QSizeF
LevelPanWidget::cellLightSize(QRectF rect) const
{
    double extent = 0.7;
    QSizeF cs = cellSize(rect);
    double m = std::min(cs.width(), cs.height());
    return QSizeF(m * extent, m * extent);
}

QRectF
LevelPanWidget::cellLightRect(QRectF rect, int row, int col) const
{
    QSizeF cls = cellLightSize(rect);
    QPointF cc = cellCentre(rect, row, col);
    return QRectF(cc.x() - cls.width() / 2., 
                  cc.y() - cls.height() / 2.,
                  cls.width(),
                  cls.height());
}

double
LevelPanWidget::thinLineWidth(QRectF rect) const
{
    double tw = ceil(rect.width() / (maxPan * 2. * 10.));
    double th = ceil(rect.height() / (m_maxNotch/2 * 10.));
    return std::min(th, tw);
}

double
LevelPanWidget::cornerRadius(QRectF rect) const
{
    QSizeF cs = cellSize(rect);
    double m = std::min(cs.width(), cs.height());
    return m / 5;
}

QRectF
LevelPanWidget::cellOutlineRect(QRectF rect, int row, int col) const
{
    QRectF clr = cellLightRect(rect, row, col);
    double adj = thinLineWidth(rect)/2 + 0.5;
    return clr.adjusted(-adj, -adj, adj, adj);
}

QColor
LevelPanWidget::cellToColour(int cell) const
{
    if (cell < 1) return Qt::black;
    if (cell < 2) return QColor(80, 0, 0);
    if (cell < 3) return QColor(160, 0, 0);
    if (cell < 4) return QColor(255, 0, 0);
    return QColor(255, 255, 0);
}

void
LevelPanWidget::renderTo(QPaintDevice *dev, QRectF rect, bool asIfEditable) const
{
    QPainter paint(dev);

    paint.setRenderHint(QPainter::Antialiasing, true);

    double thin = thinLineWidth(rect);
    double radius = cornerRadius(rect);

    QColor columnBackground = QColor(180, 180, 180);

    bool monitoring = (m_monitorLeft > 0.f || m_monitorRight > 0.f);
    
    QPen pen;
    if (isEnabled()) {
        pen.setColor(Qt::black);
    } else {
        pen.setColor(Qt::darkGray);
    }
    pen.setWidthF(thin);
    pen.setCapStyle(Qt::FlatCap);
    pen.setJoinStyle(Qt::MiterJoin);

    for (int pan = -maxPan; pan <= maxPan; ++pan) {

        paint.setPen(Qt::NoPen);
        paint.setBrush(columnBackground);
        
        QRectF top = cellOutlineRect(rect, m_maxNotch/2 - 1, pan);
        QRectF bottom = cellOutlineRect(rect, 0, pan);
        paint.drawRoundedRect(QRectF(top.x(),
                                     top.y(),
                                     top.width(),
                                     bottom.y() + bottom.height() - top.y()),
                              radius, radius);

        if (!asIfEditable && m_includeMute && m_notch == 0) {
            // We will instead be drawing a single big X for mute,
            // after this loop
            continue;
        }

        if (!monitoring && m_pan != pan) {
            continue;
        }

        int monitorNotch = 0;
        if (monitoring) {
            float rprop = float(pan - (-maxPan)) / float(maxPan * 2);
            float lprop = float(maxPan - pan) / float(maxPan * 2);
            float monitorLevel =
                lprop * m_monitorLeft * m_monitorLeft +
                rprop * m_monitorRight * m_monitorRight;
            monitorNotch = audioLevelToNotch(monitorLevel);
        }

        int firstCell = 0;
        int lastCell = m_maxNotch / 2 - 1;
        
        for (int cell = firstCell; cell <= lastCell; ++cell) {

            QRectF clr = cellLightRect(rect, cell, pan);

            if (m_includeMute && m_pan == pan && m_notch == 0) {
                // X for mute in the bottom cell
                paint.setPen(pen);
                paint.drawLine(clr.topLeft(), clr.bottomRight());
                paint.drawLine(clr.bottomLeft(), clr.topRight());
                break;
            }

            const int none = 0, half = 1, full = 2;

            int fill = none;

            int outline = none;
            if (m_pan == pan && m_notch > cell * 2 + 1) {
                outline = full;
            } else if (m_pan == pan && m_notch == cell * 2 + 1) {
                outline = half;
            }

            if (monitoring) {
                if (monitorNotch > cell * 2 + 1) {
                    fill = full;
                } else if (monitorNotch == cell * 2 + 1) {
                    fill = half;
                }
            } else {
                if (isEnabled()) {
                    fill = outline;
                }
            }                

            // If one of {fill, outline} is "full" and the other is
            // "half", then we draw the "half" one first (because we
            // need to erase half of it)

            if (fill == half || outline == half) {
                if (fill == half) {
                    paint.setBrush(cellToColour(cell));
                } else {
                    paint.setBrush(Qt::NoBrush);
                }
                if (outline == half) {
                    paint.setPen(pen);
                } else {
                    paint.setPen(Qt::NoPen);
                }

                paint.drawRoundedRect(clr, radius, radius);

                paint.setBrush(columnBackground);
                
                if (cell == lastCell) {
                    QPen bgpen(pen);
                    bgpen.setColor(columnBackground);
                    paint.setPen(bgpen);
                    paint.drawRoundedRect(QRectF(clr.x(),
                                                 clr.y(),
                                                 clr.width(),
                                                 clr.height()/4),
                                          radius, radius);
                    paint.drawRect(QRectF(clr.x(),
                                          clr.y() + clr.height()/4,
                                          clr.width(),
                                          clr.height()/4));
                } else {
                    paint.setPen(Qt::NoPen);
                    QRectF cor = cellOutlineRect(rect, cell, pan);
                    paint.drawRect(QRectF(cor.x(),
                                          cor.y() - 0.5,
                                          cor.width(),
                                          cor.height()/2));
                }
            }

            if (outline == full || fill == full) {

                if (fill == full) {
                    paint.setBrush(cellToColour(cell));
                } else {
                    paint.setBrush(Qt::NoBrush);
                }
                if (outline == full) {
                    paint.setPen(pen);
                } else {
                    paint.setPen(Qt::NoPen);
                }
                
                paint.drawRoundedRect(clr, radius, radius);
            }
        }
    }

    if (!asIfEditable && m_includeMute && m_notch == 0) {
        // The X for mute takes up the whole display when we're not
        // being rendered in editable style
        pen.setColor(Qt::black);
        pen.setWidthF(thin * 2);
        pen.setCapStyle(Qt::RoundCap);
        paint.setPen(pen);
        paint.drawLine(cellCentre(rect, 0, -maxPan),
                       cellCentre(rect, m_maxNotch/2 - 1, maxPan));
        paint.drawLine(cellCentre(rect, m_maxNotch/2 - 1, -maxPan),
                       cellCentre(rect, 0, maxPan));
    }
}

void
LevelPanWidget::paintEvent(QPaintEvent *)
{
    renderTo(this, rect(), m_editable);
}

void
LevelPanWidget::enterEvent(QEvent *e)
{
    QWidget::enterEvent(e);
    emit mouseEntered();
}

void
LevelPanWidget::leaveEvent(QEvent *e)
{
    QWidget::enterEvent(e);
    emit mouseLeft();
}

