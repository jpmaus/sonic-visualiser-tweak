/* -*- c-basic-offset: 4 indent-tabs-mode: nil -*-  vi:set ts=8 sts=4 sw=4: */

/*
    Sonic Visualiser
    An audio file viewer and annotation editor.
    Centre for Digital Music, Queen Mary, University of London.
    This file copyright 2006 Chris Cannam.
    
    This program is free software; you can redistribute it and/or
    modify it under the terms of the GNU General Public License as
    published by the Free Software Foundation; either version 2 of the
    License, or (at your option) any later version.  See the file
    COPYING included with this distribution for more information.
*/

#ifndef SV_ZOOM_CONSTRAINT_H
#define SV_ZOOM_CONSTRAINT_H

#include <stdlib.h>

#include "ZoomLevel.h"

/**
 * ZoomConstraint is a simple interface that describes a limitation on
 * the available zoom sizes for a view, for example based on cache
 * strategy or a (processing) window-size limitation.
 *
 * The default ZoomConstraint imposes no actual constraint except for
 * a nominal maximum.
 */

class ZoomConstraint
{
public:
    virtual ~ZoomConstraint() { }

    enum RoundingDirection {
        RoundDown,
        RoundUp,
        RoundNearest
    };

    /**
     * Given an "ideal" zoom level (frames per pixel or pixels per
     * frame) for a given zoom level, return the nearest viable block
     * size for this constraint.
     *
     * For example, if a block size of 1523 frames per pixel is
     * requested but the underlying model only supports value
     * summaries at powers-of-two block sizes, return 1024 or 2048
     * depending on the rounding direction supplied.
     */
    virtual ZoomLevel getNearestZoomLevel(ZoomLevel requestedZoomLevel,
                                          RoundingDirection = RoundNearest)
        const
    {
        // canonicalise
        if (requestedZoomLevel.level == 1) {
            requestedZoomLevel.zone = ZoomLevel::FramesPerPixel;
        }
        if (getMaxZoomLevel() < requestedZoomLevel) return getMaxZoomLevel();
	else return requestedZoomLevel;
    }

    /**
     * Return the minimum zoom level within range for this constraint.
     * Individual views will probably want to limit this, for example
     * in order to ensure that at least one or two samples fit in the
     * current window size, or in order to save on interpolation cost.
     */
    virtual ZoomLevel getMinZoomLevel() const {
        return { ZoomLevel::PixelsPerFrame, 512 };
    }
    
    /**
     * Return the maximum zoom level within range for this constraint.
     * This is quite large -- individual views will probably want to
     * limit how far a user might reasonably zoom out based on other
     * factors such as the duration of the file.
     */
    virtual ZoomLevel getMaxZoomLevel() const {
        return { ZoomLevel::FramesPerPixel, 4194304 }; // 2^22, arbitrarily
    }
};

#endif

