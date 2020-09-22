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

#include "RelativelyFineZoomConstraint.h"

#include <vector>
#include <algorithm>
#include <iostream>

using namespace std;

ZoomLevel
RelativelyFineZoomConstraint::getNearestZoomLevel(ZoomLevel requested,
                                                  RoundingDirection dir) const
{
    static vector<int> levels;

    int maxLevel = getMaxZoomLevel().level;

    if (levels.empty()) {
        int level = 1;
        while (level <= maxLevel) {
//            cerr << level << " ";
            levels.push_back(level);
            int step = level / 10;
            int pwr = 0;
            while (step > 0) {
                ++pwr;
                step /= 2;
            }
            step = (1 << pwr);
            level += step;
        }
//        cerr << endl;
    }

    RoundingDirection effective = dir;
    if (requested.zone == ZoomLevel::PixelsPerFrame) {
        if (dir == RoundUp) effective = RoundDown;
        else if (dir == RoundDown) effective = RoundUp;
    }

    // iterator pointing to first level that is >= requested
    auto i = lower_bound(levels.begin(), levels.end(), requested.level);

    ZoomLevel newLevel(requested);

    if (i == levels.end()) {
        newLevel.level = maxLevel;

    } else if (*i == requested.level) {
        newLevel.level = requested.level;

    } else if (effective == RoundUp) {
        newLevel.level = *i;

    } else if (effective == RoundDown) {
        if (i != levels.begin()) {
            --i;
        }
        newLevel.level = *i;

    } else { // RoundNearest
        if (i != levels.begin()) {
            auto j = i;
            --j;
            if (requested.level - *j < *i - requested.level) {
                newLevel.level = *j;
            } else {
                newLevel.level = *i;
            }
        }
    }

    // canonicalise
    if (newLevel.level == 1) {
        newLevel.zone = ZoomLevel::FramesPerPixel;
    }

    using namespace std::rel_ops;
    if (newLevel > getMaxZoomLevel()) {
        newLevel = getMaxZoomLevel();
    } else if (newLevel < getMinZoomLevel()) {
        newLevel = getMinZoomLevel();
    }
    
    return newLevel;
}


