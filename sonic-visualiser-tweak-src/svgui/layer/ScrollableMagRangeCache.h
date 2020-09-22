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

#ifndef SCROLLABLE_MAG_RANGE_CACHE_H
#define SCROLLABLE_MAG_RANGE_CACHE_H

#include "base/BaseTypes.h"
#include "base/MagnitudeRange.h"

#include "LayerGeometryProvider.h"

/**
 * A cached set of magnitude range records for a view that scrolls
 * horizontally, such as a spectrogram. The cache object holds a
 * magnitude range per column of the view, can report width (likely
 * the same as the underlying view, but it's the caller's
 * responsibility to set the size appropriately), can scroll the set
 * of ranges, and can report and update which columns have had a range
 * specified.
 *
 * The only way to *update* the valid area in a cache is to update the
 * magnitude range for a column using the sampleColumn call.
 */
class ScrollableMagRangeCache
{
public:
    ScrollableMagRangeCache() :
        m_startFrame(0)
    {}

    void invalidate() {
        m_ranges = std::vector<MagnitudeRange>(m_ranges.size());
    }
    
    int getWidth() const {
        return int(m_ranges.size());
    }

    /**
     * Set the width of the cache in columns. If the new size differs
     * from the current size, the cache is invalidated.
     */
    void resize(int newWidth) {
        if (getWidth() != newWidth) {
            m_ranges = std::vector<MagnitudeRange>(newWidth);
        }
    }
        
    ZoomLevel getZoomLevel() const {
        return m_zoomLevel;
    }

    /**
     * Set the zoom level. If the new zoom level differs from the
     * current one, the cache is invalidated. (Determining whether to
     * invalidate the cache here is the only thing the zoom level is
     * used for.)
     */
    void setZoomLevel(ZoomLevel zoom) {
        using namespace std::rel_ops;
        if (m_zoomLevel != zoom) {
            m_zoomLevel = zoom;
            invalidate();
        }
    }

    sv_frame_t getStartFrame() const {
        return m_startFrame;
    }

    /**
     * Set the start frame. If the new start frame differs from the
     * current one, the cache is invalidated. To scroll, i.e. to set
     * the start frame while retaining cache validity where possible,
     * use scrollTo() instead.
     */
    void setStartFrame(sv_frame_t frame) {
        if (m_startFrame != frame) {
            m_startFrame = frame;
            invalidate();
        }
    }

    bool isColumnSet(int column) const {
        return in_range_for(m_ranges, column) && m_ranges.at(column).isSet();
    }

    bool areColumnsSet(int x, int count) const {
        for (int i = 0; i < count; ++i) {
            if (!isColumnSet(x + i)) return false;
        }
        return true;
    }
    
    /**
     * Get the magnitude range for a single column.
     */
    MagnitudeRange getRange(int column) const {
        return m_ranges.at(column);
    }

    /**
     * Get the magnitude range for a range of columns.
     */
    MagnitudeRange getRange(int x, int count) const;
    
    /**
     * Set the new start frame for the cache, according to the
     * geometry of the supplied LayerGeometryProvider, if possible
     * also moving along any existing valid data within the cache so
     * that it continues to be valid for the new start frame.
     */
    void scrollTo(const LayerGeometryProvider *v, sv_frame_t newStartFrame);
    
    /**
     * Update a column in the cache, by column index. (Column zero is
     * the first column in the cache, it has nothing to do with any
     * underlying model that the cache may be used with.)
     */
    void sampleColumn(int column, const MagnitudeRange &r);
    
private:
    std::vector<MagnitudeRange> m_ranges;
    sv_frame_t m_startFrame;
    ZoomLevel m_zoomLevel;
};

#endif
