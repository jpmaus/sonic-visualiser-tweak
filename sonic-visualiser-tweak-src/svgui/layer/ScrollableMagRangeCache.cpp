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

#include "ScrollableMagRangeCache.h"

#include "base/HitCount.h"
#include "base/Debug.h"

#include <iostream>
using namespace std;

//#define DEBUG_SCROLLABLE_MAG_RANGE_CACHE 1

void
ScrollableMagRangeCache::scrollTo(const LayerGeometryProvider *v,
                                  sv_frame_t newStartFrame)
{        
    static HitCount count("ScrollableMagRangeCache: scrolling");
    
    int dx = (v->getXForFrame(m_startFrame) -
              v->getXForFrame(newStartFrame));

#ifdef DEBUG_SCROLLABLE_MAG_RANGE_CACHE
    SVDEBUG << "ScrollableMagRangeCache::scrollTo: start frame " << m_startFrame
            << " -> " << newStartFrame << ", dx = " << dx << endl;
#endif

    if (m_startFrame == newStartFrame) {
        // haven't moved
        count.hit();
        return;
    }
    
    m_startFrame = newStartFrame;

    if (dx == 0) {
        // haven't moved visibly (even though start frame may have changed)
        count.hit();
        return;
    }
        
    int w = int(m_ranges.size());

    if (dx <= -w || dx >= w) {
        // scrolled entirely off
        invalidate();
        count.miss();
        return;
    }

    count.partial();
        
    // dx is in range, cache is scrollable

    if (dx < 0) {
        // The new start frame is to the left of the old start
        // frame. We need to add some empty ranges at the left (start)
        // end and clip the right end. Assemble -dx new values, then
        // w+dx old values starting at index 0.

        auto newRanges = vector<MagnitudeRange>(-dx);
        newRanges.insert(newRanges.end(),
                         m_ranges.begin(), m_ranges.begin() + (w + dx));
        m_ranges = newRanges;
        
    } else {
        // The new start frame is to the right of the old start
        // frame. We want to clip the left (start) end and add some
        // empty ranges at the right end. Assemble w-dx old values
        // starting at index dx, then dx new values.

        auto newRanges = vector<MagnitudeRange>(dx);
        newRanges.insert(newRanges.begin(),
                         m_ranges.begin() + dx, m_ranges.end());
        m_ranges = newRanges;
    }

#ifdef DEBUG_SCROLLABLE_MAG_RANGE_CACHE
    SVDEBUG << "maxes (" << m_ranges.size() << ") now: ";
    for (int i = 0; in_range_for(m_ranges, i); ++i) {
        SVDEBUG << m_ranges[i].getMax() << " ";
    }
    SVDEBUG << endl;
#endif
}

MagnitudeRange
ScrollableMagRangeCache::getRange(int x, int count) const
{
    MagnitudeRange r;
#ifdef DEBUG_SCROLLABLE_MAG_RANGE_CACHE
    SVDEBUG << "ScrollableMagRangeCache::getRange(" << x << ", " << count << ")" << endl;
#endif
    for (int i = 0; i < count; ++i) {
        const auto &cr = m_ranges.at(x + i);
        if (cr.isSet()) {
            r.sample(cr);
        }
#ifdef DEBUG_SCROLLABLE_MAG_RANGE_CACHE
        SVDEBUG << cr.getMin() << "->" << cr.getMax() << " ";
#endif
    }
#ifdef DEBUG_SCROLLABLE_MAG_RANGE_CACHE
    SVDEBUG << endl;
#endif
    return r;
}

void
ScrollableMagRangeCache::sampleColumn(int column, const MagnitudeRange &r)
{
    if (!in_range_for(m_ranges, column)) {
        SVCERR << "ERROR: ScrollableMagRangeCache::sampleColumn: column " << column
               << " is out of range for cache of width " << m_ranges.size()
               << " (with start frame " << m_startFrame << ")" << endl;
        throw logic_error("column out of range");
    } else {
        m_ranges[column].sample(r);
    }
}

