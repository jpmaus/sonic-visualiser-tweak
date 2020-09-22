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

#ifndef TEST_LOG_RANGE_H
#define TEST_LOG_RANGE_H

#include "../LogRange.h"

#include <QObject>
#include <QtTest>

#include <iostream>
#include <cmath>

using namespace std;

class TestLogRange : public QObject
{
    Q_OBJECT

private slots:

    void mapPositiveAboveDefaultThreshold()
    {
        QCOMPARE(LogRange::map(10.0), 1.0);
        QCOMPARE(LogRange::map(100.0), 2.0);
        QCOMPARE(LogRange::map(0.1), -1.0);
        QCOMPARE(LogRange::map(1.0), 0.0);
        QCOMPARE(LogRange::map(0.0000001), -7.0);
        QCOMPARE(LogRange::map(20.0), log10(20.0));
    }
    
    void mapPositiveAboveSetThreshold()
    {
        QCOMPARE(LogRange::map(10.0, -10.0), 1.0);
        QCOMPARE(LogRange::map(100.0, 1.0), 2.0);
        QCOMPARE(LogRange::map(0.1, -5.0), -1.0);
        QCOMPARE(LogRange::map(1.0, -0.01), 0.0);
        QCOMPARE(LogRange::map(0.0000001, -20.0), -7.0);
        QCOMPARE(LogRange::map(20.0, 0.0), log10(20.0));
    }

    void mapZeroDefaultThreshold()
    {
        QCOMPARE(LogRange::map(0.0), -10.0);
    }

    void mapZeroSetThreshold()
    {
        QCOMPARE(LogRange::map(0.0, 12.0), 12.0);
        QCOMPARE(LogRange::map(0.0, -12.0), -12.0);
        QCOMPARE(LogRange::map(0.0, 0.0), 0.0);
    }
    
    void mapPositiveBelowDefaultThreshold()
    {
        // The threshold is used only for zero values, not for very
        // small ones -- it's arguably a stand-in or replacement value
        // rather than a threshold. So this should behave the same as
        // for values above the threshold.
        QCOMPARE(LogRange::map(1e-10), -10.0);
        QCOMPARE(LogRange::map(1e-20), -20.0);
        QCOMPARE(LogRange::map(1e-100), -100.0);
    }

    void mapPositiveBelowSetThreshold()
    {
        // As above
        QCOMPARE(LogRange::map(10.0, 4.0), 1.0);
        QCOMPARE(LogRange::map(1e-10, 4.0), -10.0);
        QCOMPARE(LogRange::map(1e-20, -15.0), -20.0);
        QCOMPARE(LogRange::map(1e-100, -100.0), -100.0);
    }

    void mapNegative()
    {
        // Should always return map of absolute value. These are
        // picked from vaarious of the above tests.
        
        QCOMPARE(LogRange::map(-10.0), 1.0);
        QCOMPARE(LogRange::map(-100.0), 2.0);
        QCOMPARE(LogRange::map(-0.1), -1.0);
        QCOMPARE(LogRange::map(-1.0), 0.0);
        QCOMPARE(LogRange::map(-0.0000001), -7.0);
        QCOMPARE(LogRange::map(-20.0), log10(20.0));
        QCOMPARE(LogRange::map(-10.0, 4.0), 1.0);
        QCOMPARE(LogRange::map(-1e-10, 4.0), -10.0);
        QCOMPARE(LogRange::map(-1e-20, -15.0), -20.0);
        QCOMPARE(LogRange::map(-1e-100, -100.0), -100.0);
        QCOMPARE(LogRange::map(-0.0, 12.0), 12.0);
        QCOMPARE(LogRange::map(-0.0, -12.0), -12.0);
        QCOMPARE(LogRange::map(-0.0, 0.0), 0.0);
    }

    void unmap()
    {
        // Simply pow(10, x)

        QCOMPARE(LogRange::unmap(0.0), 1.0);
        QCOMPARE(LogRange::unmap(1.0), 10.0);
        QCOMPARE(LogRange::unmap(-1.0), 0.1);
        QCOMPARE(LogRange::unmap(100.0), 1e+100);
        QCOMPARE(LogRange::unmap(-100.0), 1e-100);
    }

    void mapRangeAllPositiveDefaultThreshold()
    {
        double min, max;

        min = 1.0; max = 10.0;
        LogRange::mapRange(min, max);
        QCOMPARE(min, 0.0); QCOMPARE(max, 1.0);

        min = 10.0; max = 1.0;
        LogRange::mapRange(min, max);
        QCOMPARE(min, 0.0); QCOMPARE(max, 1.0);

        // if equal, the function uses an arbitrary 1.0 range before mapping
        min = 10.0; max = 10.0;
        LogRange::mapRange(min, max);
        QCOMPARE(min, 1.0); QCOMPARE(max, log10(11.0));
    }

    void mapRangeAllPositiveSetThreshold()
    {
        double min, max;

        min = 1.0; max = 10.0;
        LogRange::mapRange(min, max, -4.0);
        QCOMPARE(min, 0.0); QCOMPARE(max, 1.0);

        min = 10.0; max = 1.0;
        LogRange::mapRange(min, max, -4.0);
        QCOMPARE(min, 0.0); QCOMPARE(max, 1.0);

        // if equal, the function uses an arbitrary 1.0 range before mapping
        min = 10.0; max = 10.0;
        LogRange::mapRange(min, max, -4.0);
        QCOMPARE(min, 1.0); QCOMPARE(max, log10(11.0));
    }

    void mapRangeAllNegativeDefaultThreshold()
    {
        double min, max;

        min = -1.0; max = -10.0;
        LogRange::mapRange(min, max);
        QCOMPARE(min, 0.0); QCOMPARE(max, 1.0);

        min = -10.0; max = -1.0;
        LogRange::mapRange(min, max);
        QCOMPARE(min, 0.0); QCOMPARE(max, 1.0);

        // if equal, the function uses an arbitrary 1.0 range before mapping
        min = -10.0; max = -10.0;
        LogRange::mapRange(min, max);
        QCOMPARE(min, log10(9.0)); QCOMPARE(max, 1.0);
    }
    
    void mapRangeAllNegativeSetThreshold()
    {
        double min, max;

        min = -1.0; max = -10.0;
        LogRange::mapRange(min, max, -4.0);
        QCOMPARE(min, 0.0); QCOMPARE(max, 1.0);

        min = -10.0; max = -1.0;
        LogRange::mapRange(min, max, -4.0);
        QCOMPARE(min, 0.0); QCOMPARE(max, 1.0);

        // if equal, the function uses an arbitrary 1.0 range before mapping
        min = -10.0; max = -10.0;
        LogRange::mapRange(min, max, -4.0);
        QCOMPARE(min, log10(9.0)); QCOMPARE(max, 1.0);
    }
    
    void mapRangeAllNonNegativeDefaultThreshold()
    {
        double min, max;

        min = 0.0; max = 10.0;
        LogRange::mapRange(min, max);
        QCOMPARE(min, -10.0); QCOMPARE(max, 1.0);

        min = 10.0; max = 0.0;
        LogRange::mapRange(min, max);
        QCOMPARE(min, -10.0); QCOMPARE(max, 1.0);

        // if equal, the function uses an arbitrary 1.0 range before mapping
        min = 0.0; max = 0.0;
        LogRange::mapRange(min, max);
        QCOMPARE(min, -10.0); QCOMPARE(max, 0.0);
    }
    
    void mapRangeAllNonNegativeSetThreshold()
    {
        double min, max;

        min = 0.0; max = 10.0;
        LogRange::mapRange(min, max, -4.0);
        QCOMPARE(min, -4.0); QCOMPARE(max, 1.0);

        min = 10.0; max = 0.0;
        LogRange::mapRange(min, max, -4.0);
        QCOMPARE(min, -4.0); QCOMPARE(max, 1.0);

        // if equal, the function uses an arbitrary 1.0 range before mapping
        min = 0.0; max = 0.0;
        LogRange::mapRange(min, max, -4.0);
        QCOMPARE(min, -4.0); QCOMPARE(max, 0.0);
    }
    
    void mapRangeAllNonPositiveDefaultThreshold()
    {
        double min, max;

        min = 0.0; max = -10.0;
        LogRange::mapRange(min, max);
        QCOMPARE(min, -10.0); QCOMPARE(max, 1.0);

        min = -10.0; max = 0.0;
        LogRange::mapRange(min, max);
        QCOMPARE(min, -10.0); QCOMPARE(max, 1.0);
    }
    
    void mapRangeAllNonPositiveSetThreshold()
    {
        double min, max;

        min = 0.0; max = -10.0;
        LogRange::mapRange(min, max, -4.0);
        QCOMPARE(min, -4.0); QCOMPARE(max, 1.0);

        min = -10.0; max = 0.0;
        LogRange::mapRange(min, max, -4.0);
        QCOMPARE(min, -4.0); QCOMPARE(max, 1.0);
    }
    
    void mapRangeSpanningZeroDefaultThreshold()
    {
        double min, max;

        min = -1.0; max = 10.0;
        LogRange::mapRange(min, max);
        QCOMPARE(min, -10.0); QCOMPARE(max, 1.0);

        min = -100.0; max = 1.0;
        LogRange::mapRange(min, max);
        QCOMPARE(min, -10.0); QCOMPARE(max, 2.0);

        min = -10.0; max = 1e-200;
        LogRange::mapRange(min, max);
        QCOMPARE(min, -10.0); QCOMPARE(max, 1.0);

        min = 1e-200; max = -10.0;
        LogRange::mapRange(min, max);
        QCOMPARE(min, -10.0); QCOMPARE(max, 1.0);

        min = -1e-200; max = 100.0;
        LogRange::mapRange(min, max);
        QCOMPARE(min, -10.0); QCOMPARE(max, 2.0);

        min = 10.0; max = -1e-200;
        LogRange::mapRange(min, max);
        QCOMPARE(min, -10.0); QCOMPARE(max, 1.0);

        // if none of the input range is above the threshold in
        // magnitude, but it still spans zero, we use the input max as
        // threshold and then add 1 for range
        min = -1e-200; max = 1e-300;
        LogRange::mapRange(min, max);
        QCOMPARE(min, -201.0); QCOMPARE(max, -200.0);

        min = 1e-200; max = -1e-300;
        LogRange::mapRange(min, max);
        QCOMPARE(min, -201.0); QCOMPARE(max, -200.0);
    }
    
    void mapRangeSpanningZeroSetThreshold()
    {
        double min, max;

        min = -1.0; max = 10.0;
        LogRange::mapRange(min, max, -4.0);
        QCOMPARE(min, -4.0); QCOMPARE(max, 1.0);

        min = -100.0; max = 1.0;
        LogRange::mapRange(min, max, -4.0);
        QCOMPARE(min, -4.0); QCOMPARE(max, 2.0);

        min = -10.0; max = 1e-200;
        LogRange::mapRange(min, max, -4.0);
        QCOMPARE(min, -4.0); QCOMPARE(max, 1.0);

        min = 1e-200; max = -10.0;
        LogRange::mapRange(min, max, -4.0);
        QCOMPARE(min, -4.0); QCOMPARE(max, 1.0);

        min = -1e-200; max = 100.0;
        LogRange::mapRange(min, max, -4.0);
        QCOMPARE(min, -4.0); QCOMPARE(max, 2.0);

        min = 10.0; max = -1e-200;
        LogRange::mapRange(min, max, -4.0);
        QCOMPARE(min, -4.0); QCOMPARE(max, 1.0);

        // if none of the input range is above the threshold in
        // magnitude, but it still spans zero, we use the input max as
        // threshold and then add 1 for range
        min = -1e-200; max = 1e-300;
        LogRange::mapRange(min, max, -4.0);
        QCOMPARE(min, -201.0); QCOMPARE(max, -200.0);

        min = 1e-200; max = -1e-300;
        LogRange::mapRange(min, max, -4.0);
        QCOMPARE(min, -201.0); QCOMPARE(max, -200.0);
    }
    
};

#endif
