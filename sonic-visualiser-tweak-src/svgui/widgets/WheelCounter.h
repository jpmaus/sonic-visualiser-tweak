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

#ifndef SV_WHEEL_COUNTER_H
#define SV_WHEEL_COUNTER_H

#include <QWheelEvent>

/**
 * Manage the little bit of tedious book-keeping associated with
 * translating vertical wheel events into up/down notch counts
 */
class WheelCounter
{
public:
    WheelCounter() : m_pendingWheelAngle(0) { }

    ~WheelCounter() { }

    int count(QWheelEvent *e) {
        
        e->accept();
    
        int delta = e->angleDelta().y();
        if (delta == 0) {
            return 0;
        }

        if (e->phase() == Qt::ScrollBegin ||
            std::abs(delta) >= 120 ||
            (delta > 0 && m_pendingWheelAngle < 0) ||
            (delta < 0 && m_pendingWheelAngle > 0)) {
            m_pendingWheelAngle = delta;
        } else {
            m_pendingWheelAngle += delta;
        }

        if (abs(m_pendingWheelAngle) >= 600) {
            // Sometimes on Linux we're seeing absurdly extreme angles
            // on the first wheel event -- discard those entirely
            m_pendingWheelAngle = 0;
            return 0;
        }

        int count = m_pendingWheelAngle / 120;
        m_pendingWheelAngle -= count * 120;
        return count;
    }

private:
    int m_pendingWheelAngle;
};

#endif
