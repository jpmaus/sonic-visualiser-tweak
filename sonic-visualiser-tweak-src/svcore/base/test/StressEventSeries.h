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

#ifndef STRESS_EVENT_SERIES_H
#define STRESS_EVENT_SERIES_H

#include "../EventSeries.h"

#include <QObject>
#include <QtTest>

#include <iostream>

using namespace std;

class StressEventSeries : public QObject
{
    Q_OBJECT

private:
    void report(int n, QString sort, clock_t start, clock_t end) {
        QString message = QString("Time for %1 %2 events = ").arg(n).arg(sort);
        cerr << "                 " << message;
        for (int i = 0; i < 34 - message.size(); ++i) cerr << " ";
        cerr << double(end - start) * 1000.0 / double(CLOCKS_PER_SEC)
             << "ms" << std::endl;
    }        
    
    void short_n(int n) {
        clock_t start = clock();
        std::set<Event> ee;
        EventSeries s;
        for (int i = 0; i < n; ++i) {
            float value = float(rand()) / float(RAND_MAX);
            Event e(rand(), value, 1000, QString("event %1").arg(i));
            ee.insert(e);
        }
        for (const Event &e: ee) {
            s.add(e);
        }
        QCOMPARE(s.count(), n);
        clock_t end = clock();
        report(n, "short", start, end);
    }

    void longish_n(int n) {
        clock_t start = clock();
        std::set<Event> ee;
        EventSeries s;
        for (int i = 0; i < n; ++i) {
            float value = float(rand()) / float(RAND_MAX);
            Event e(rand(), value, rand() / 1000, QString("event %1").arg(i));
            ee.insert(e);
        }
        for (const Event &e: ee) {
            s.add(e);
        }
        QCOMPARE(s.count(), n);
        clock_t end = clock();
        report(n, "longish", start, end);
    }

private slots:
    void short_3() { short_n(1000); }
    void short_4() { short_n(10000); }
    void short_5() { short_n(100000); }
    void short_6() { short_n(1000000); }
    void longish_3() { longish_n(1000); }
    void longish_4() { longish_n(10000); }
    void longish_5() { longish_n(100000); }
};

#endif
