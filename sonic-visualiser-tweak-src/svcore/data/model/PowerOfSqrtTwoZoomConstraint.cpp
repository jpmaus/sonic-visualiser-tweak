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

#include "PowerOfSqrtTwoZoomConstraint.h"

#include <iostream>
#include <cmath>

#include "base/Debug.h"


ZoomLevel
PowerOfSqrtTwoZoomConstraint::getNearestZoomLevel(ZoomLevel requested,
                                                  RoundingDirection dir) const
{
    int type, power;
    int blockSize;

    if (requested.zone == ZoomLevel::FramesPerPixel) {
        blockSize = getNearestBlockSize(requested.level, type, power, dir);
        return { requested.zone, blockSize };
    } else {
        RoundingDirection opposite = dir;
        if (dir == RoundUp) opposite = RoundDown;
        else if (dir == RoundDown) opposite = RoundUp;
        blockSize = getNearestBlockSize(requested.level, type, power, opposite);
        if (blockSize > getMinZoomLevel().level) {
            blockSize = getMinZoomLevel().level;
        }
        if (blockSize == 1) {
            return { ZoomLevel::FramesPerPixel, 1 };
        } else {
            return { requested.zone, blockSize };
        }
    }
}

int
PowerOfSqrtTwoZoomConstraint::getNearestBlockSize(int blockSize,
                                                  int &type, 
                                                  int &power,
                                                  RoundingDirection dir) const
{
//    SVCERR << "given " << blockSize << endl;

    int minCachePower = getMinCachePower();

    if (blockSize < (1 << minCachePower)) {
        type = -1;
        power = 0;
        float val = 1.0, prevVal = 1.0;
        while (val + 0.01 < blockSize) {
            prevVal = val;
            val *= sqrtf(2.f);
        }
        int rval = int(val + 0.01f);
//        SVCERR << "got val = " << val << ", rval = " << rval << ", prevVal = " << prevVal << endl;
        if (rval != blockSize && dir != RoundUp) {
            if (dir == RoundDown) {
                rval = int(prevVal + 0.01f);
            } else if (val - float(blockSize) < float(blockSize) - prevVal) {
                rval = int(val + 0.01f);
            } else {
                rval = int(prevVal + 0.01);
            }
        }
//        SVCERR << "returning " << rval << endl;
        return rval;
    }

    int prevBase = (1 << minCachePower);
    int prevPower = minCachePower;
    int prevType = 0;

    int result = 0;

    for (unsigned int i = 0; ; ++i) {

        power = minCachePower + i/2;
        type = i % 2;

        int base;
        if (type == 0) {
            base = (1 << power);
        } else {
            base = (((unsigned int)((1 << minCachePower) * sqrt(2.) + 0.01))
                    << (power - minCachePower));
        }

//        SVCERR << "Testing base " << base << " (i = " << i << ", power = " << power << ", type = " << type << ")" << endl;

        if (base == blockSize) {
            result = base;
//            SVCERR << "Equal, accepting" << endl;
            break;
        }

        if (base > blockSize) {
            if (dir == RoundNearest) {
                if (base - blockSize < blockSize - prevBase) {
                    dir = RoundUp;
//                    SVCERR << "Closer to " << base << " than " << prevBase
//                           << ", rounding up" << endl;
                } else {
                    dir = RoundDown;
//                    SVCERR << "Closer to " << prevBase << " than " << base
//                           << ", rounding down" << endl;
                }
            }
            if (dir == RoundUp) {
                result = base;
                break;
            } else {
                type = prevType;
                power = prevPower;
                result = prevBase;
                break;
            }
        }

        prevType = type;
        prevPower = power;
        prevBase = base;
    }

    if (result > getMaxZoomLevel().level) {
        result = getMaxZoomLevel().level;
    }

//    SVCERR << "Returning result " << result << endl;

    return result;
}   
