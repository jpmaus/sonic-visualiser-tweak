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

#ifndef TEST_ZOOM_CONSTRAINTS_H
#define TEST_ZOOM_CONSTRAINTS_H

#include "../PowerOfTwoZoomConstraint.h"
#include "../PowerOfSqrtTwoZoomConstraint.h"
#include "../RelativelyFineZoomConstraint.h"

#include <QObject>
#include <QtTest>
#include <QDir>

#include <iostream>

using namespace std;

class TestZoomConstraints : public QObject
{
    Q_OBJECT

    string roundingName(ZoomConstraint::RoundingDirection dir) {
        switch (dir) {
        case ZoomConstraint::RoundDown: return "RoundDown";
        case ZoomConstraint::RoundUp: return "RoundUp";
        case ZoomConstraint::RoundNearest: return "RoundNearest";
        }
        return "<?>";
    }
    
    void compare(ZoomLevel zin,
                 ZoomConstraint::RoundingDirection dir,
                 ZoomLevel zobt,
                 ZoomLevel zexp) {
        if (zexp.level == 1) {
            // A zoom level of "1 pixel per frame" is not considered
            // canonical - it should be "1 frame per pixel"
            zexp.zone = ZoomLevel::FramesPerPixel;
        }
        if (zobt == zexp) {
            return;
        } else {
            std::cerr << "For input " << zin << " and rounding direction "
                      << roundingName(dir)
                      << ", expected output " << zexp << " but obtained "
                      << zobt << std::endl;
            QCOMPARE(zobt, zexp);
        }
    }

    void checkFpp(const ZoomConstraint &c,
                  ZoomConstraint::RoundingDirection dir,
                  int n,
                  int expected) {
        ZoomLevel zin(ZoomLevel::FramesPerPixel, n);
        ZoomLevel zexp(ZoomLevel::FramesPerPixel, expected);
        ZoomLevel zobt(c.getNearestZoomLevel(zin, dir));
        compare(zin, dir, zobt, zexp);
    }

    void checkPpf(const ZoomConstraint &c,
                  ZoomConstraint::RoundingDirection dir,
                  int n,
                  int expected) {
        ZoomLevel zin(ZoomLevel::PixelsPerFrame, n);
        ZoomLevel zexp(ZoomLevel::PixelsPerFrame, expected);
        ZoomLevel zobt(c.getNearestZoomLevel(zin, dir));
        compare(zin, dir, zobt, zexp);
    }

    void checkBoth(const ZoomConstraint &c,
                   ZoomConstraint::RoundingDirection dir,
                   int n,
                   int expected) {
        checkFpp(c, dir, n, expected);
        checkPpf(c, dir, n, expected);
    }

    void checkMaxMin(const ZoomConstraint &c,
                     ZoomConstraint::RoundingDirection dir) {
        auto max = c.getMaxZoomLevel();
        compare(max, dir,
                c.getNearestZoomLevel(max, dir), max);
        compare(max.incremented(), dir,
                c.getNearestZoomLevel(max.incremented(), dir), max);
        auto min = c.getMinZoomLevel();
        compare(min, dir,
                c.getNearestZoomLevel(min, dir), min);
        compare(min.decremented(), dir,
                c.getNearestZoomLevel(min.decremented(), dir), min);
    }

    const static ZoomConstraint::RoundingDirection up = ZoomConstraint::RoundUp;
    const static ZoomConstraint::RoundingDirection down = ZoomConstraint::RoundDown;
    const static ZoomConstraint::RoundingDirection nearest = ZoomConstraint::RoundNearest;
                                                                         
private slots:
    void unconstrainedNearest() {
        ZoomConstraint c;
        checkBoth(c, nearest, 1, 1);
        checkBoth(c, nearest, 2, 2);
        checkBoth(c, nearest, 3, 3);
        checkBoth(c, nearest, 4, 4);
        checkBoth(c, nearest, 20, 20);
        checkBoth(c, nearest, 32, 32);
        auto max = c.getMaxZoomLevel();
        QCOMPARE(c.getNearestZoomLevel(max), max);
        QCOMPARE(c.getNearestZoomLevel(max.incremented()), max);
    }
    
    void unconstrainedUp() {
        ZoomConstraint c;
        checkBoth(c, up, 1, 1);
        checkBoth(c, up, 2, 2);
        checkBoth(c, up, 3, 3);
        checkBoth(c, up, 4, 4);
        checkBoth(c, up, 20, 20);
        checkBoth(c, up, 32, 32);
        auto max = c.getMaxZoomLevel();
        QCOMPARE(c.getNearestZoomLevel(max, up), max);
        QCOMPARE(c.getNearestZoomLevel(max.incremented(), up), max);
    }
    
    void unconstrainedDown() {
        ZoomConstraint c;
        checkBoth(c, down, 1, 1);
        checkBoth(c, down, 2, 2);
        checkBoth(c, down, 3, 3);
        checkBoth(c, down, 4, 4);
        checkBoth(c, down, 20, 20);
        checkBoth(c, down, 32, 32);
        auto max = c.getMaxZoomLevel();
        QCOMPARE(c.getNearestZoomLevel(max, down), max);
        QCOMPARE(c.getNearestZoomLevel(max.incremented(), down), max);
    }

    void powerOfTwoNearest() {
        PowerOfTwoZoomConstraint c;
        checkBoth(c, nearest, 1, 1);
        checkBoth(c, nearest, 2, 2);
        checkBoth(c, nearest, 3, 2);
        checkBoth(c, nearest, 4, 4);
        checkBoth(c, nearest, 20, 16);
        checkBoth(c, nearest, 23, 16);
        checkBoth(c, nearest, 24, 16);
        checkBoth(c, nearest, 25, 32);
        auto max = c.getMaxZoomLevel();
        QCOMPARE(c.getNearestZoomLevel(max), max);
        QCOMPARE(c.getNearestZoomLevel(max.incremented()), max);
    }
    
    void powerOfTwoUp() {
        PowerOfTwoZoomConstraint c;
        checkBoth(c, up, 1, 1);
        checkBoth(c, up, 2, 2);
        checkFpp(c, up, 3, 4);
        checkPpf(c, up, 3, 2);
        checkBoth(c, up, 4, 4);
        checkFpp(c, up, 20, 32);
        checkPpf(c, up, 20, 16);
        checkBoth(c, up, 32, 32);
        checkFpp(c, up, 33, 64);
        checkPpf(c, up, 33, 32);
        checkMaxMin(c, up);
    }
    
    void powerOfTwoDown() {
        PowerOfTwoZoomConstraint c;
        checkBoth(c, down, 1, 1);
        checkBoth(c, down, 2, 2);
        checkFpp(c, down, 3, 2);
        checkPpf(c, down, 3, 4);
        checkBoth(c, down, 4, 4);
        checkFpp(c, down, 20, 16);
        checkPpf(c, down, 20, 32);
        checkBoth(c, down, 32, 32);
        checkFpp(c, down, 33, 32);
        checkPpf(c, down, 33, 64);
        checkMaxMin(c, down);
    }

    void powerOfSqrtTwoNearest() {
        PowerOfSqrtTwoZoomConstraint c;
        checkBoth(c, nearest, 1, 1);
        checkBoth(c, nearest, 2, 2);
        checkBoth(c, nearest, 3, 2);
        checkBoth(c, nearest, 4, 4);
        checkBoth(c, nearest, 18, 16);
        checkBoth(c, nearest, 19, 16);
        checkBoth(c, nearest, 20, 22);
        checkBoth(c, nearest, 23, 22);
        checkBoth(c, nearest, 28, 32);
        // PowerOfSqrtTwoZoomConstraint makes an effort to ensure
        // bigger numbers get rounded to a multiple of something
        // simple (64 or 90 depending on whether they are power-of-two
        // or power-of-sqrt-two types)
        checkBoth(c, nearest, 350, 360);
        // The most extreme level available in ppf mode
        // (getMinZoomLevel()) is currently 512, so these bigger
        // numbers will only happen in fpp mode
        checkFpp(c, nearest, 800, 720);
        checkFpp(c, nearest, 1023, 1024);
        checkFpp(c, nearest, 1024, 1024);
        checkFpp(c, nearest, 1024, 1024);
        checkFpp(c, nearest, 1025, 1024);
        checkPpf(c, nearest, 800, 512);
        checkPpf(c, nearest, 1025, 512);
        checkMaxMin(c, nearest);
    }
    
    void powerOfSqrtTwoUp() {
        PowerOfSqrtTwoZoomConstraint c;
        checkBoth(c, up, 1, 1);
        checkBoth(c, up, 2, 2);
        checkFpp(c, up, 3, 4);
        checkPpf(c, up, 3, 2);
        checkBoth(c, up, 4, 4);
        checkFpp(c, up, 18, 22);
        checkPpf(c, up, 18, 16);
        checkBoth(c, up, 22, 22);
        checkFpp(c, up, 23, 32);
        checkPpf(c, up, 23, 22);
        // see comments above
        checkFpp(c, up, 800, 1024);
        checkFpp(c, up, 1023, 1024);
        checkFpp(c, up, 1024, 1024);
        checkFpp(c, up, 1025, 1440);
        checkPpf(c, up, 300, 256);
        checkPpf(c, up, 800, 512);
        checkPpf(c, up, 1600, 512);
        checkMaxMin(c, up);
    }
    
    void powerOfSqrtTwoDown() {
        PowerOfSqrtTwoZoomConstraint c;
        checkBoth(c, down, 1, 1);
        checkBoth(c, down, 2, 2);
        checkFpp(c, down, 3, 2);
        checkPpf(c, down, 3, 4);
        checkBoth(c, down, 4, 4);
        checkFpp(c, down, 18, 16);
        checkPpf(c, down, 18, 22);
        checkBoth(c, down, 22, 22);
        checkFpp(c, down, 23, 22);
        checkPpf(c, down, 23, 32);
        // see comments above
        checkFpp(c, down, 800, 720);
        checkFpp(c, down, 1023, 720);
        checkFpp(c, down, 1024, 1024);
        checkFpp(c, down, 1025, 1024);
        checkPpf(c, down, 300, 360);
        checkPpf(c, down, 800, 512);
        checkPpf(c, down, 1600, 512);
        checkMaxMin(c, down);
    }

    void relativelyFineNearest() {
        RelativelyFineZoomConstraint c;
        checkBoth(c, nearest, 1, 1);
        checkBoth(c, nearest, 2, 2);
        checkBoth(c, nearest, 3, 3);
        checkBoth(c, nearest, 4, 4);
        checkBoth(c, nearest, 20, 20);
        checkBoth(c, nearest, 33, 32);
        checkBoth(c, nearest, 59, 56);
        checkBoth(c, nearest, 69, 72);
        checkBoth(c, nearest, 121, 128);
        checkMaxMin(c, nearest);
    }
    
    void relativelyFineUp() {
        RelativelyFineZoomConstraint c;
        checkBoth(c, up, 1, 1);
        checkBoth(c, up, 2, 2);
        checkBoth(c, up, 3, 3);
        checkBoth(c, up, 4, 4);
        checkBoth(c, up, 20, 20);
        checkFpp(c, up, 33, 36);
        checkPpf(c, up, 33, 32);
        checkFpp(c, up, 59, 64);
        checkPpf(c, up, 59, 56);
        checkFpp(c, up, 69, 72);
        checkPpf(c, up, 69, 64);
        checkFpp(c, up, 121, 128);
        checkPpf(c, up, 121, 112);
        checkMaxMin(c, up);
    }
    
    void relativelyFineDown() {
        RelativelyFineZoomConstraint c;
        checkBoth(c, down, 1, 1);
        checkBoth(c, down, 2, 2);
        checkBoth(c, down, 3, 3);
        checkBoth(c, down, 4, 4);
        checkBoth(c, down, 20, 20);
        checkFpp(c, down, 33, 32);
        checkPpf(c, down, 33, 36);
        checkFpp(c, down, 59, 56);
        checkPpf(c, down, 59, 64);
        checkFpp(c, down, 69, 64);
        checkPpf(c, down, 69, 72);
        checkFpp(c, down, 121, 112);
        checkPpf(c, down, 121, 128);
        checkMaxMin(c, down);
    }
};

#endif
    
