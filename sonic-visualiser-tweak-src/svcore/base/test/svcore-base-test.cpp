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

#include "TestLogRange.h"
#include "TestRangeMapper.h"
#include "TestPitch.h"
#include "TestScaleTickIntervals.h"
#include "TestStringBits.h"
#include "TestOurRealTime.h"
#include "TestVampRealTime.h"
#include "TestColumnOp.h"
#include "TestMovingMedian.h"
#include "TestById.h"
#include "TestEventSeries.h"
#include "StressEventSeries.h"

#include "system/Init.h"

#include <QtTest>

#include <iostream>

int main(int argc, char *argv[])
{
    int good = 0, bad = 0;

    // This is necessary to ensure correct behaviour of snprintf with
    // older MinGW implementations
    svSystemSpecificInitialisation();

    QCoreApplication app(argc, argv);
    app.setOrganizationName("sonic-visualiser");
    app.setApplicationName("test-svcore-base");

    {
        TestRangeMapper t;
        if (QTest::qExec(&t, argc, argv) == 0) ++good;
        else ++bad;
    }
    {
        TestPitch t;
        if (QTest::qExec(&t, argc, argv) == 0) ++good;
        else ++bad;
    }
    {
        TestOurRealTime t;
        if (QTest::qExec(&t, argc, argv) == 0) ++good;
        else ++bad;
    }
    {
        TestVampRealTime t;
        if (QTest::qExec(&t, argc, argv) == 0) ++good;
        else ++bad;
    }
    {
        TestStringBits t;
        if (QTest::qExec(&t, argc, argv) == 0) ++good;
        else ++bad;
    }
    {
        TestColumnOp t;
        if (QTest::qExec(&t, argc, argv) == 0) ++good;
        else ++bad;
    }
    {
        TestLogRange t;
        if (QTest::qExec(&t, argc, argv) == 0) ++good;
        else ++bad;
    }
    {
        TestScaleTickIntervals t;
        if (QTest::qExec(&t, argc, argv) == 0) ++good;
        else ++bad;
    }
    {
        TestMovingMedian t;
        if (QTest::qExec(&t, argc, argv) == 0) ++good;
        else ++bad;
    }
    {
        TestEventSeries t;
        if (QTest::qExec(&t, argc, argv) == 0) ++good;
        else ++bad;
    }
    {
        TestById t;
        if (QTest::qExec(&t, argc, argv) == 0) ++good;
        else ++bad;
    }

#ifdef NOT_DEFINED
    {
        StressEventSeries t;
        if (QTest::qExec(&t, argc, argv) == 0) ++good;
        else ++bad;
    }
#endif
    
    if (bad > 0) {
        SVCERR << "\n********* " << bad << " test suite(s) failed!\n" << endl;
        return 1;
    } else {
        SVCERR << "All tests passed" << endl;
        return 0;
    }
}
