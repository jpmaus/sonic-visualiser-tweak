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

#ifndef TEST_VAMP_REALTIME_H

#include <QObject>
#include <QtTest>
#include <QDir>

#include <iostream>

#include <vamp-hostsdk/RealTime.h>

using namespace std;

#define ONE_MILLION 1000000
#define ONE_BILLION 1000000000

class TestVampRealTime : public QObject
{
    Q_OBJECT

    void compareTexts(string s, const char *e) {
        QString actual(s.c_str());
        QString expected(e);
        QCOMPARE(actual, expected);
    }

    typedef Vamp::RealTime RealTime;
    typedef long frame_type;
                           
private slots:

    void zero()
    {
        QCOMPARE(RealTime(0, 0), RealTime::zeroTime);
        QCOMPARE(RealTime(0, 0).sec, 0);
        QCOMPARE(RealTime(0, 0).nsec, 0);
        QCOMPARE(RealTime(0, 0).msec(), 0);
        QCOMPARE(RealTime(0, 0).usec(), 0);
    }

    void ctor()
    {
        QCOMPARE(RealTime(0, 0), RealTime(0, 0));

        // wraparounds
        QCOMPARE(RealTime(0, ONE_BILLION/2), RealTime(1, -ONE_BILLION/2));
        QCOMPARE(RealTime(0, -ONE_BILLION/2), RealTime(-1, ONE_BILLION/2));

        QCOMPARE(RealTime(1, ONE_BILLION), RealTime(2, 0));
        QCOMPARE(RealTime(1, -ONE_BILLION), RealTime(0, 0));
        QCOMPARE(RealTime(-1, ONE_BILLION), RealTime(0, 0));
        QCOMPARE(RealTime(-1, -ONE_BILLION), RealTime(-2, 0));

        QCOMPARE(RealTime(2, -ONE_BILLION*2), RealTime(0, 0));
        QCOMPARE(RealTime(2, -ONE_BILLION/2), RealTime(1, ONE_BILLION/2));

        QCOMPARE(RealTime(-2, ONE_BILLION*2), RealTime(0, 0));
        QCOMPARE(RealTime(-2, ONE_BILLION/2), RealTime(-1, -ONE_BILLION/2));
        
        QCOMPARE(RealTime(0, 1).sec, 0);
        QCOMPARE(RealTime(0, 1).nsec, 1);
        QCOMPARE(RealTime(0, -1).sec, 0);
        QCOMPARE(RealTime(0, -1).nsec, -1);
        QCOMPARE(RealTime(1, -1).sec, 0);
        QCOMPARE(RealTime(1, -1).nsec, ONE_BILLION-1);
        QCOMPARE(RealTime(-1, 1).sec, 0);
        QCOMPARE(RealTime(-1, 1).nsec, -ONE_BILLION+1);
        QCOMPARE(RealTime(-1, -1).sec, -1);
        QCOMPARE(RealTime(-1, -1).nsec, -1);
        
        QCOMPARE(RealTime(2, -ONE_BILLION*2).sec, 0);
        QCOMPARE(RealTime(2, -ONE_BILLION*2).nsec, 0);
        QCOMPARE(RealTime(2, -ONE_BILLION/2).sec, 1);
        QCOMPARE(RealTime(2, -ONE_BILLION/2).nsec, ONE_BILLION/2);

        QCOMPARE(RealTime(-2, ONE_BILLION*2).sec, 0);
        QCOMPARE(RealTime(-2, ONE_BILLION*2).nsec, 0);
        QCOMPARE(RealTime(-2, ONE_BILLION/2).sec, -1);
        QCOMPARE(RealTime(-2, ONE_BILLION/2).nsec, -ONE_BILLION/2);
    }
    
    void fromSeconds()
    {
        QCOMPARE(RealTime::fromSeconds(0), RealTime(0, 0));

        QCOMPARE(RealTime::fromSeconds(0.5).sec, 0);
        QCOMPARE(RealTime::fromSeconds(0.5).nsec, ONE_BILLION/2);
        QCOMPARE(RealTime::fromSeconds(0.5).usec(), ONE_MILLION/2);
        QCOMPARE(RealTime::fromSeconds(0.5).msec(), 500);
        
        QCOMPARE(RealTime::fromSeconds(0.5), RealTime(0, ONE_BILLION/2));
        QCOMPARE(RealTime::fromSeconds(1), RealTime(1, 0));
        QCOMPARE(RealTime::fromSeconds(1.5), RealTime(1, ONE_BILLION/2));

        QCOMPARE(RealTime::fromSeconds(-0.5).sec, 0);
        QCOMPARE(RealTime::fromSeconds(-0.5).nsec, -ONE_BILLION/2);
        QCOMPARE(RealTime::fromSeconds(-0.5).usec(), -ONE_MILLION/2);
        QCOMPARE(RealTime::fromSeconds(-0.5).msec(), -500);
        
        QCOMPARE(RealTime::fromSeconds(-1.5).sec, -1);
        QCOMPARE(RealTime::fromSeconds(-1.5).nsec, -ONE_BILLION/2);
        QCOMPARE(RealTime::fromSeconds(-1.5).usec(), -ONE_MILLION/2);
        QCOMPARE(RealTime::fromSeconds(-1.5).msec(), -500);
        
        QCOMPARE(RealTime::fromSeconds(-0.5), RealTime(0, -ONE_BILLION/2));
        QCOMPARE(RealTime::fromSeconds(-1), RealTime(-1, 0));
        QCOMPARE(RealTime::fromSeconds(-1.5), RealTime(-1, -ONE_BILLION/2));
    }

    void fromMilliseconds()
    {
        QCOMPARE(RealTime::fromMilliseconds(0), RealTime(0, 0));
        QCOMPARE(RealTime::fromMilliseconds(500), RealTime(0, ONE_BILLION/2));
        QCOMPARE(RealTime::fromMilliseconds(1000), RealTime(1, 0));
        QCOMPARE(RealTime::fromMilliseconds(1500), RealTime(1, ONE_BILLION/2));

        QCOMPARE(RealTime::fromMilliseconds(-0), RealTime(0, 0));
        QCOMPARE(RealTime::fromMilliseconds(-500), RealTime(0, -ONE_BILLION/2));
        QCOMPARE(RealTime::fromMilliseconds(-1000), RealTime(-1, 0));
        QCOMPARE(RealTime::fromMilliseconds(-1500), RealTime(-1, -ONE_BILLION/2));
    }

#ifndef Q_OS_WIN
    void fromTimeval()
    {
        struct timeval tv;

        tv.tv_sec = 0; tv.tv_usec = 0;
        QCOMPARE(RealTime::fromTimeval(tv), RealTime(0, 0));
        tv.tv_sec = 0; tv.tv_usec = ONE_MILLION/2;
        QCOMPARE(RealTime::fromTimeval(tv), RealTime(0, ONE_BILLION/2));
        tv.tv_sec = 1; tv.tv_usec = 0;
        QCOMPARE(RealTime::fromTimeval(tv), RealTime(1, 0));
        tv.tv_sec = 1; tv.tv_usec = ONE_MILLION/2;
        QCOMPARE(RealTime::fromTimeval(tv), RealTime(1, ONE_BILLION/2));

        tv.tv_sec = 0; tv.tv_usec = -ONE_MILLION/2;
        QCOMPARE(RealTime::fromTimeval(tv), RealTime(0, -ONE_BILLION/2));
        tv.tv_sec = -1; tv.tv_usec = 0;
        QCOMPARE(RealTime::fromTimeval(tv), RealTime(-1, 0));
        tv.tv_sec = -1; tv.tv_usec = -ONE_MILLION/2;
        QCOMPARE(RealTime::fromTimeval(tv), RealTime(-1, -ONE_BILLION/2));
    }
#endif

    void assign()
    {
        RealTime r;
        r = RealTime(0, 0);
        QCOMPARE(r, RealTime::zeroTime);
        r = RealTime(0, ONE_BILLION/2);
        QCOMPARE(r.sec, 0);
        QCOMPARE(r.nsec, ONE_BILLION/2);
        r = RealTime(-1, -ONE_BILLION/2);
        QCOMPARE(r.sec, -1);
        QCOMPARE(r.nsec, -ONE_BILLION/2);
    }

    void plus()
    {
        QCOMPARE(RealTime(0, 0) + RealTime(0, 0), RealTime(0, 0));

        QCOMPARE(RealTime(0, 0) + RealTime(0, ONE_BILLION/2), RealTime(0, ONE_BILLION/2));
        QCOMPARE(RealTime(0, ONE_BILLION/2) + RealTime(0, ONE_BILLION/2), RealTime(1, 0));
        QCOMPARE(RealTime(1, 0) + RealTime(0, ONE_BILLION/2), RealTime(1, ONE_BILLION/2));

        QCOMPARE(RealTime(0, 0) + RealTime(0, -ONE_BILLION/2), RealTime(0, -ONE_BILLION/2));
        QCOMPARE(RealTime(0, -ONE_BILLION/2) + RealTime(0, -ONE_BILLION/2), RealTime(-1, 0));
        QCOMPARE(RealTime(-1, 0) + RealTime(0, -ONE_BILLION/2), RealTime(-1, -ONE_BILLION/2));

        QCOMPARE(RealTime(1, 0) + RealTime(0, -ONE_BILLION/2), RealTime(0, ONE_BILLION/2));
        QCOMPARE(RealTime(1, 0) + RealTime(0, -ONE_BILLION/2) + RealTime(0, -ONE_BILLION/2), RealTime(0, 0));
        QCOMPARE(RealTime(1, 0) + RealTime(0, -ONE_BILLION/2) + RealTime(0, -ONE_BILLION/2) + RealTime(0, -ONE_BILLION/2), RealTime(0, -ONE_BILLION/2));

        QCOMPARE(RealTime(0, ONE_BILLION/2) + RealTime(-1, 0), RealTime(0, -ONE_BILLION/2));
        QCOMPARE(RealTime(0, -ONE_BILLION/2) + RealTime(1, 0), RealTime(0, ONE_BILLION/2));
    }
    
    void minus()
    {
        QCOMPARE(RealTime(0, 0) - RealTime(0, 0), RealTime(0, 0));

        QCOMPARE(RealTime(0, 0) - RealTime(0, ONE_BILLION/2), RealTime(0, -ONE_BILLION/2));
        QCOMPARE(RealTime(0, ONE_BILLION/2) - RealTime(0, ONE_BILLION/2), RealTime(0, 0));
        QCOMPARE(RealTime(1, 0) - RealTime(0, ONE_BILLION/2), RealTime(0, ONE_BILLION/2));

        QCOMPARE(RealTime(0, 0) - RealTime(0, -ONE_BILLION/2), RealTime(0, ONE_BILLION/2));
        QCOMPARE(RealTime(0, -ONE_BILLION/2) - RealTime(0, -ONE_BILLION/2), RealTime(0, 0));
        QCOMPARE(RealTime(-1, 0) - RealTime(0, -ONE_BILLION/2), RealTime(0, -ONE_BILLION/2));

        QCOMPARE(RealTime(1, 0) - RealTime(0, -ONE_BILLION/2), RealTime(1, ONE_BILLION/2));
        QCOMPARE(RealTime(1, 0) - RealTime(0, -ONE_BILLION/2) - RealTime(0, -ONE_BILLION/2), RealTime(2, 0));
        QCOMPARE(RealTime(1, 0) - RealTime(0, -ONE_BILLION/2) - RealTime(0, -ONE_BILLION/2) - RealTime(0, -ONE_BILLION/2), RealTime(2, ONE_BILLION/2));

        QCOMPARE(RealTime(0, ONE_BILLION/2) - RealTime(-1, 0), RealTime(1, ONE_BILLION/2));
        QCOMPARE(RealTime(0, -ONE_BILLION/2) - RealTime(1, 0), RealTime(-1, -ONE_BILLION/2));
    }

    void negate()
    {
        QCOMPARE(-RealTime(0, 0), RealTime(0, 0));
        QCOMPARE(-RealTime(1, 0), RealTime(-1, 0));
        QCOMPARE(-RealTime(1, ONE_BILLION/2), RealTime(-1, -ONE_BILLION/2));
        QCOMPARE(-RealTime(-1, -ONE_BILLION/2), RealTime(1, ONE_BILLION/2));
    }

    void compare()
    {
        int sec, nsec;
        for (sec = -2; sec <= 2; sec += 2) {
            for (nsec = -1; nsec <= 1; nsec += 1) {
                QCOMPARE(RealTime(sec, nsec) < RealTime(sec, nsec), false);
                QCOMPARE(RealTime(sec, nsec) > RealTime(sec, nsec), false);
                QCOMPARE(RealTime(sec, nsec) == RealTime(sec, nsec), true);
                QCOMPARE(RealTime(sec, nsec) != RealTime(sec, nsec), false);
                QCOMPARE(RealTime(sec, nsec) <= RealTime(sec, nsec), true);
                QCOMPARE(RealTime(sec, nsec) >= RealTime(sec, nsec), true);
            }
        }
        RealTime prev(-3, 0);
        for (sec = -2; sec <= 2; sec += 2) {
            for (nsec = -1; nsec <= 1; nsec += 1) {

                RealTime curr(sec, nsec);

                QCOMPARE(prev < curr, true);
                QCOMPARE(prev > curr, false);
                QCOMPARE(prev == curr, false);
                QCOMPARE(prev != curr, true);
                QCOMPARE(prev <= curr, true);
                QCOMPARE(prev >= curr, false);

                QCOMPARE(curr < prev, false);
                QCOMPARE(curr > prev, true);
                QCOMPARE(curr == prev, false);
                QCOMPARE(curr != prev, true);
                QCOMPARE(curr <= prev, false);
                QCOMPARE(curr >= prev, true);

                prev = curr;
            }
        }
    }

    void frame()
    {
        int frames[] = {
            0, 1, 2047, 2048, 6656,
            32767, 32768, 44100, 44101,
            999999999, 2000000000
        };
        int n = sizeof(frames)/sizeof(frames[0]);

        int rates[] = {
            1, 2, 8000, 22050,
            44100, 44101, 192000, 2000000001
        };
        int m = sizeof(rates)/sizeof(rates[0]);

        vector<vector<RealTime>> realTimes = {
            { { 0, 0 }, { 1, 0 }, { 2047, 0 }, { 2048, 0 },
              { 6656, 0 }, { 32767, 0 }, { 32768, 0 }, { 44100, 0 },
              { 44101, 0 }, { 999999999, 0 }, { 2000000000, 0 } },
            { { 0, 0 }, { 0, 500000000 }, { 1023, 500000000 }, { 1024, 0 },
              { 3328, 0 }, { 16383, 500000000 }, { 16384, 0 }, { 22050, 0 },
              { 22050, 500000000 }, { 499999999, 500000000 }, { 1000000000, 0 } },
            { { 0, 0 }, { 0, 125000 }, { 0, 255875000 }, { 0, 256000000 },
              { 0, 832000000 }, { 4, 95875000 }, { 4, 96000000 }, { 5, 512500000 },
              { 5, 512625000 }, { 124999, 999875000 }, { 250000, 0 } },
            { { 0, 0 }, { 0, 45351 }, { 0, 92834467 }, { 0, 92879819 },
              { 0, 301859410 }, { 1, 486031746 }, { 1, 486077098 }, { 2, 0 },
              { 2, 45351 }, { 45351, 473877551 }, { 90702, 947845805 } },
            { { 0, 0 }, { 0, 22676 }, { 0, 46417234 }, { 0, 46439909 },
              { 0, 150929705 }, { 0, 743015873 }, { 0, 743038549 }, { 1, 0 },
              { 1, 22676 }, { 22675, 736938776 }, { 45351, 473922902 } },
            { { 0, 0 }, { 0, 22675 }, { 0, 46416181 }, { 0, 46438856 },
              { 0, 150926283 }, { 0, 742999025 }, { 0, 743021700 }, { 0, 999977325 },
              { 1, 0 }, { 22675, 222761389 }, { 45350, 445568128 } },
            { { 0, 0 }, { 0, 5208 }, { 0, 10661458 }, { 0, 10666667 },
              { 0, 34666667 }, { 0, 170661458 }, { 0, 170666667 }, { 0, 229687500 },
              { 0, 229692708 }, { 5208, 333328125 }, { 10416, 666666667 } },
            { { 0, 0 }, { 0, 0 }, { 0, 1023 }, { 0, 1024 },
              { 0, 3328 }, { 0, 16383 }, { 0, 16384 }, { 0, 22050 },
              { 0, 22050 }, { 0, 499999999 }, { 1, 0 } }
        };
        
        for (int i = 0; i < n; ++i) {
            frame_type frame = frames[i];
            for (int j = 0; j < m; ++j) {
                int rate = rates[j];

                RealTime rt = RealTime::frame2RealTime(frame, rate);
                QCOMPARE(rt.sec, realTimes[j][i].sec);
                QCOMPARE(rt.nsec, realTimes[j][i].nsec);

                frame_type conv = RealTime::realTime2Frame(rt, rate);
                
                rt = RealTime::frame2RealTime(-frame, rate);
                frame_type negconv = RealTime::realTime2Frame(rt, rate);

                if (rate > ONE_BILLION) {
                    // We don't have enough precision in RealTime
                    // for this absurd sample rate, so a round trip
                    // conversion may round
                    QVERIFY(abs(frame - conv) < 2);
                    QVERIFY(abs(-frame - negconv) < 2);
                } else {
                    QCOMPARE(conv, frame);
                    QCOMPARE(negconv, -frame);
                }
            }
        }
    }

    // Vamp SDK version just has toText, which is like our own
    // toMSText with true for its second arg
    
    void toText()
    {
        // we want to use QStrings, because then the Qt test library
        // will print out any conflicts. The compareTexts function
        // does this for us

        int halfSec = ONE_BILLION/2; // nsec
        
        RealTime rt = RealTime(0, 0);
        compareTexts(rt.toText(false), "0");
        compareTexts(rt.toText(true), "0.000");

        rt = RealTime(1, halfSec);
        compareTexts(rt.toText(false), "1.5");
        compareTexts(rt.toText(true), "1.500");

        rt = RealTime::fromSeconds(-1.5);
        compareTexts(rt.toText(false), "-1.5");
        compareTexts(rt.toText(true), "-1.500");

        rt = RealTime::fromSeconds(60);
        compareTexts(rt.toText(false), "1:00");
        compareTexts(rt.toText(true), "1:00.000");

        rt = RealTime::fromSeconds(61.05);
        compareTexts(rt.toText(false), "1:01.05");
        compareTexts(rt.toText(true), "1:01.050");
        
        rt = RealTime::fromSeconds(601.05);
        compareTexts(rt.toText(false), "10:01.05");
        compareTexts(rt.toText(true), "10:01.050");
        
        rt = RealTime::fromSeconds(3600);
        compareTexts(rt.toText(false), "1:00:00");
        compareTexts(rt.toText(true), "1:00:00.000");

        // For practical reasons our time display always rounds down
        rt = RealTime(3599, ONE_BILLION-1);
        compareTexts(rt.toText(false), "59:59.999");
        compareTexts(rt.toText(true), "59:59.999");

        rt = RealTime::fromSeconds(3600 * 4 + 60 * 5 + 3 + 0.01);
        compareTexts(rt.toText(false), "4:05:03.01");
        compareTexts(rt.toText(true), "4:05:03.010");

        rt = RealTime::fromSeconds(-(3600 * 4 + 60 * 5 + 3 + 0.01));
        compareTexts(rt.toText(false), "-4:05:03.01");
        compareTexts(rt.toText(true), "-4:05:03.010");
    }
};

#endif

