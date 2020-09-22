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

#ifndef SV_EXTENTS_H
#define SV_EXTENTS_H

#include <vector>

/**
 * Maintain a min and max value, and update them when supplied a new
 * data point.
 */
template <typename T>
class Extents
{
public:
    Extents() : m_min(T()), m_max(T()) { }
    Extents(T min, T max) : m_min(min), m_max(max) { }
    
    bool operator==(const Extents &r) {
        return r.m_min == m_min && r.m_max == m_max;
    }
    bool operator!=(const Extents &r) {
        return !(*this == r);
    }
    
    bool isSet() const {
        return (m_min != T() || m_max != T());
    }
    void set(T min, T max) {
        m_min = min;
        m_max = max;
        if (m_max < m_min) m_max = m_min;
    }
    void reset() {
        m_min = T();
        m_max = T();
    }

    bool sample(T f) {
        bool changed = false;
        if (isSet()) {
            if (f < m_min) { m_min = f; changed = true; }
            if (f > m_max) { m_max = f; changed = true; }
        } else {
            m_max = m_min = f;
            changed = true;
        }
        return changed;
    }
    bool sample(const std::vector<T> &ff) {
        bool changed = false;
        for (auto f: ff) {
            if (sample(f)) {
                changed = true;
            }
        }
        return changed;
    }
    bool sample(const Extents &r) {
        bool changed = false;
        if (isSet()) {
            if (r.m_min < m_min) { m_min = r.m_min; changed = true; }
            if (r.m_max > m_max) { m_max = r.m_max; changed = true; }
        } else {
            m_min = r.m_min;
            m_max = r.m_max;
            changed = true;
        }
        return changed;
    }            

    T getMin() const { return m_min; }
    T getMax() const { return m_max; }

private:
    T m_min;
    T m_max;
};

#endif
