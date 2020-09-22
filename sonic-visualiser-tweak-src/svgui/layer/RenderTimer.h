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

#ifndef RENDER_TIMER_H
#define RENDER_TIMER_H

#include <chrono>

class RenderTimer
{
public:
    enum Type {
        /// A normal rendering operation with normal responsiveness demands
        FastRender,

        /// An operation that the user might accept being slower
        SlowRender,

        /// An operation that should always complete, i.e. as if there
        /// were no RenderTimer in use, but without having to change
        /// client code structurally
        NoTimeout
    };
    
    /**
     * Create a new RenderTimer and start timing. Make one of these
     * before rendering, and then call outOfTime() regularly during
     * rendering. If outOfTime() returns true, abandon rendering!  and
     * schedule the rest for after some user responsiveness has
     * happened.
     */
    RenderTimer(Type t) :
        m_start(std::chrono::steady_clock::now()),
        m_haveLimits(true),
        m_minFraction(0.1),
        m_softLimit(0.1),
        m_hardLimit(0.2),
        m_softLimitOverridden(false) {

        if (t == NoTimeout) {
            m_haveLimits = false;
        } else if (t == SlowRender) {
            m_softLimit = 0.2;
            m_hardLimit = 0.4;
        }
    }


    /**
     * Return true if we have run out of time and should suspend
     * rendering and handle user events instead. Call this regularly
     * during rendering work: fractionComplete should be an estimate
     * of how much of the work has been done as of this call, as a
     * number between 0.0 (none of it) and 1.0 (all of it).
     */
    bool outOfTime(double fractionComplete) {

        if (!m_haveLimits || fractionComplete < m_minFraction) {
            return false;
        }
        
        auto t = std::chrono::steady_clock::now();
        double elapsed = std::chrono::duration<double>(t - m_start).count();
        
        if (elapsed > m_hardLimit) {
            return true;
        } else if (!m_softLimitOverridden && elapsed > m_softLimit) {
            if (fractionComplete > 0.6) {
                // If we're significantly more than half way by the
                // time we reach the soft limit, ignore it (though
                // always respect the hard limit, above). Otherwise
                // respect the soft limit and report out of time now.
                m_softLimitOverridden = true;
            } else {
                return true;
            }
        }

        return false;
    }

    double secondsPerItem(int itemsRendered) const {

        if (itemsRendered == 0) return 0.0;

        auto t = std::chrono::steady_clock::now();
        double elapsed = std::chrono::duration<double>(t - m_start).count();

        return elapsed / itemsRendered;
    }

private:
    std::chrono::time_point<std::chrono::steady_clock> m_start;
    bool m_haveLimits;
    double m_minFraction; // proportion, 0.0 -> 1.0
    double m_softLimit; // seconds
    double m_hardLimit; // seconds
    bool m_softLimitOverridden;
};

#endif
