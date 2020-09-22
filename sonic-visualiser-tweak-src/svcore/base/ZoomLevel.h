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

#ifndef SV_ZOOM_LEVEL_H
#define SV_ZOOM_LEVEL_H

#include "BaseTypes.h"

#include <ostream>
#include <cmath>

/** Display zoom level. Can be an integer number of samples per pixel,
 *  or an integer number of pixels per sample.
 */
struct ZoomLevel {

    enum Zone {
        FramesPerPixel, // zoomed out (as in classic SV)
        PixelsPerFrame  // zoomed in beyond 1-1 (interpolating the waveform)
    };

    Zone zone;
    int level;

    ZoomLevel() : zone(FramesPerPixel), level(1) { }
    ZoomLevel(Zone z, int lev) : zone(z), level(lev) { }
    
    bool operator<(const ZoomLevel &other) const {
        if (zone == FramesPerPixel) {
            if (other.zone == zone) {
                return level < other.level;
            } else {
                return false;
            }
        } else {
            if (other.zone == zone) {
                return level > other.level;
            } else {
                return false;
            }
        }
    }

    bool operator==(const ZoomLevel &other) const {
        return (zone == other.zone && level == other.level);
    }

    ZoomLevel incremented() const {
        if (zone == FramesPerPixel) {
            return { zone, level + 1 };
        } else if (level == 1) {
            return { FramesPerPixel, 2 };
        } else if (level == 2) {
            return { FramesPerPixel, 1 };
        } else {
            return { zone, level - 1 };
        }
    }

    ZoomLevel decremented() const {
        if (zone == PixelsPerFrame) {
            return { zone, level + 1 };
        } else if (level == 1) {
            return { PixelsPerFrame, 2 };
        } else {
            return { zone, level - 1 };
        }
    }

    /** Inexact conversion. The result is a whole number if we are
     *  zoomed in enough (in PixelsPerFrame zone), a fraction
     *  otherwise.
     */
    double framesToPixels(double frames) const {
        if (zone == PixelsPerFrame) {
            return frames * level;
        } else {
            return frames / level;
        }
    }

    /** Inexact conversion. The result is a whole number if we are
     *  zoomed out enough (in FramesPerPixel zone), a fraction
     *  otherwise.
     */
    double pixelsToFrames(double pixels) const {
        if (zone == PixelsPerFrame) {
            return pixels / level;
        } else {
            return pixels * level;
        }
    }

    /** Return a ZoomLevel that approximates the given ratio of pixels
     *  to frames.
     */
    static ZoomLevel fromRatio(int pixels, sv_frame_t frames) {
        if (pixels < frames) {
            return { FramesPerPixel, int(round(double(frames)/pixels)) };
        } else {
            int r = int(round(pixels/double(frames)));
            if (r > 1) {
                return { PixelsPerFrame, r };
            } else {
                return { FramesPerPixel, 1 };
            }
        }
    }
};

std::ostream &operator<<(std::ostream &s, const ZoomLevel &z);

#endif
