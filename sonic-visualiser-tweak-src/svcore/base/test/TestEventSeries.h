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

#ifndef TEST_EVENT_SERIES_H
#define TEST_EVENT_SERIES_H

#include "../EventSeries.h"

#include <QObject>
#include <QtTest>

#include <iostream>

using namespace std;

class TestEventSeries : public QObject
{
    Q_OBJECT

private slots:
    void empty() {

        EventSeries s;
        QCOMPARE(s.isEmpty(), true);
        QCOMPARE(s.count(), 0);

        Event p(10, QString());
        QCOMPARE(s.contains(p), false);
        QCOMPARE(s.getEventsCovering(400), EventVector());
    }

    void singleEvent() {

        EventSeries s;
        Event p(10, QString());
        s.add(p);
        QCOMPARE(s.isEmpty(), false);
        QCOMPARE(s.count(), 1);
        QCOMPARE(s.contains(p), true);

        s.remove(p);
        QCOMPARE(s.isEmpty(), true);
        QCOMPARE(s.count(), 0);
        QCOMPARE(s.contains(p), false);
    }

    void duplicateEvents() {

        EventSeries s;
        Event p(10, QString());
        s.add(p);
        s.add(p);
        QCOMPARE(s.isEmpty(), false);
        QCOMPARE(s.count(), 2);
        QCOMPARE(s.contains(p), true);

        s.remove(p);
        QCOMPARE(s.isEmpty(), false);
        QCOMPARE(s.count(), 1);
        QCOMPARE(s.contains(p), true);

        s.remove(p);
        QCOMPARE(s.isEmpty(), true);
        QCOMPARE(s.count(), 0);
        QCOMPARE(s.contains(p), false);
    }

    void singleEventCover() {

        EventSeries s;
        Event p(10, QString());
        s.add(p);
        EventVector cover;
        cover.push_back(p);
        QCOMPARE(s.getEventsCovering(10), cover);
        QCOMPARE(s.getEventsCovering(11), EventVector());
        QCOMPARE(s.getEventsCovering(9), EventVector());
    }

    void singleEventSpan() {

        EventSeries s;
        Event p(10, QString());
        s.add(p);
        EventVector span;
        span.push_back(p);
        QCOMPARE(s.getEventsSpanning(10, 2), span);
        QCOMPARE(s.getEventsSpanning(9, 2), span);
        QCOMPARE(s.getEventsSpanning(8, 2), EventVector());
        QCOMPARE(s.getEventsSpanning(7, 2), EventVector());
        QCOMPARE(s.getEventsSpanning(11, 2), EventVector());
    }
    
    void identicalEventsCover() {

        EventSeries s;
        Event p(10, QString());
        s.add(p);
        s.add(p);

        EventVector cover;
        cover.push_back(p);
        cover.push_back(p);
        QCOMPARE(s.getEventsCovering(10), cover);
        QCOMPARE(s.getEventsCovering(11), EventVector());
        QCOMPARE(s.getEventsCovering(9), EventVector());

        s.remove(p);
        cover.clear();
        cover.push_back(p);
        QCOMPARE(s.getEventsCovering(10), cover);
        QCOMPARE(s.getEventsCovering(11), EventVector());
        QCOMPARE(s.getEventsCovering(9), EventVector());
    }
    
    void identicalEventsSpan() {

        EventSeries s;
        Event p(10, QString());
        s.add(p);
        s.add(p);

        EventVector span;
        span.push_back(p);
        span.push_back(p);
        QCOMPARE(s.getEventsSpanning(10, 2), span);
        QCOMPARE(s.getEventsSpanning(9, 2), span);
        QCOMPARE(s.getEventsSpanning(8, 2), EventVector());
        QCOMPARE(s.getEventsSpanning(11, 2), EventVector());
    }

    void similarEventsCover() {

        EventSeries s;
        Event a(10, QString("a"));
        Event b(10, QString("b"));
        s.add(a);
        s.add(b);
        EventVector cover;
        cover.push_back(a);
        cover.push_back(b);
        QCOMPARE(s.getEventsCovering(10), cover);
        QCOMPARE(s.getEventsCovering(11), EventVector());
        QCOMPARE(s.getEventsCovering(9), EventVector());
    }

    void similarEventsSpan() {

        EventSeries s;
        Event a(10, QString("a"));
        Event b(10, QString("b"));
        s.add(a);
        s.add(b);
        EventVector span;
        span.push_back(a);
        span.push_back(b);
        QCOMPARE(s.getEventsSpanning(10, 2), span);
        QCOMPARE(s.getEventsSpanning(9, 2), span);
        QCOMPARE(s.getEventsSpanning(11, 2), EventVector());
        QCOMPARE(s.getEventsSpanning(8, 2), EventVector());
    }

    void singleEventWithDurationCover() {

        EventSeries s;
        Event p(10, 1.0, 20, QString());
        s.add(p);
        EventVector cover;
        cover.push_back(p);
        QCOMPARE(s.getEventsCovering(10), cover);
        QCOMPARE(s.getEventsCovering(11), cover);
        QCOMPARE(s.getEventsCovering(29), cover);
        QCOMPARE(s.getEventsCovering(30), EventVector());
        QCOMPARE(s.getEventsCovering(9), EventVector());
    }

    void singleEventWithDurationSpan() {

        EventSeries s;
        Event p(10, 1.0, 20, QString());
        s.add(p);
        EventVector span;
        span.push_back(p);
        QCOMPARE(s.getEventsSpanning(9, 2), span);
        QCOMPARE(s.getEventsSpanning(8, 2), EventVector());
        QCOMPARE(s.getEventsSpanning(19, 4), span);
        QCOMPARE(s.getEventsSpanning(29, 2), span);
        QCOMPARE(s.getEventsSpanning(30, 2), EventVector());
    }

    void identicalEventsWithDurationCover() {

        EventSeries s;
        Event p(10, 1.0, 20, QString());
        s.add(p);
        s.add(p);
        EventVector cover;
        cover.push_back(p);
        cover.push_back(p);
        QCOMPARE(s.getEventsCovering(10), cover);
        QCOMPARE(s.getEventsCovering(11), cover);
        QCOMPARE(s.getEventsCovering(29), cover);
        QCOMPARE(s.getEventsCovering(30), EventVector());
        QCOMPARE(s.getEventsCovering(9), EventVector());

        s.remove(p);
        cover.clear();
        cover.push_back(p);
        QCOMPARE(s.getEventsCovering(10), cover);
        QCOMPARE(s.getEventsCovering(11), cover);
        QCOMPARE(s.getEventsCovering(29), cover);
        QCOMPARE(s.getEventsCovering(30), EventVector());
        QCOMPARE(s.getEventsCovering(9), EventVector());
    }

    void identicalEventsWithDurationSpan() {

        EventSeries s;
        Event p(10, 1.0, 20, QString());
        s.add(p);
        s.add(p);
        EventVector span;
        span.push_back(p);
        span.push_back(p);
        QCOMPARE(s.getEventsSpanning(9, 2), span);
        QCOMPARE(s.getEventsSpanning(10, 2), span);
        QCOMPARE(s.getEventsSpanning(11, 2), span);
        QCOMPARE(s.getEventsSpanning(29, 2), span);
        QCOMPARE(s.getEventsSpanning(30, 2), EventVector());
        QCOMPARE(s.getEventsSpanning(8, 2), EventVector());
    }

    void multipleEventsCover() {

        EventSeries s;
        Event a(10, QString("a"));
        Event b(11, QString("b"));
        Event c(40, QString("c"));
        s.add(c);
        s.add(a);
        s.add(b);
        s.remove(a);
        s.add(a);
        s.add(c);
        s.remove(c);
        QCOMPARE(s.count(), 3);
        EventVector cover;
        cover.push_back(a);
        QCOMPARE(s.getEventsCovering(10), cover);
        cover.clear();
        cover.push_back(c);
        QCOMPARE(s.getEventsCovering(40), cover);
        QCOMPARE(s.getEventsCovering(9), EventVector());
    }

    void multipleEventsSpan() {

        EventSeries s;
        Event a(10, QString("a"));
        Event b(11, QString("b"));
        Event c(40, QString("c"));
        s.add(c);
        s.add(a);
        s.add(b);
        EventVector span;
        span.push_back(a);
        span.push_back(b);
        QCOMPARE(s.getEventsSpanning(10, 2), span);
        span.clear();
        span.push_back(c);
        QCOMPARE(s.getEventsSpanning(39, 3), span);
        QCOMPARE(s.getEventsSpanning(9, 1), EventVector());
        QCOMPARE(s.getEventsSpanning(10, 0), EventVector());
    }

    void multipleEventsEndFrame() {

        EventSeries s;
        Event a(10, QString("a"));
        Event b(11, QString("b"));
        Event c(40, QString("c"));
        s.add(c);
        s.add(a);
        s.add(b);
        s.add(b);
        QCOMPARE(s.getEndFrame(), sv_frame_t(40));
        s.remove(c);
        QCOMPARE(s.getEndFrame(), sv_frame_t(11));
        s.remove(b);
        QCOMPARE(s.getEndFrame(), sv_frame_t(11));
        s.remove(a);
        QCOMPARE(s.getEndFrame(), sv_frame_t(11));
        s.remove(b);
        QCOMPARE(s.getEndFrame(), sv_frame_t(0));
    }

    void disjointEventsWithDurationCover() {

        EventSeries s;
        Event a(10, 1.0f, 20, QString("a"));
        Event b(100, 1.2f, 30, QString("b"));
        s.add(a);
        s.add(b);
        QCOMPARE(s.getEventsCovering(0), EventVector());
        QCOMPARE(s.getEventsCovering(10), EventVector({ a }));
        QCOMPARE(s.getEventsCovering(15), EventVector({ a }));
        QCOMPARE(s.getEventsCovering(30), EventVector());
        QCOMPARE(s.getEventsCovering(99), EventVector());
        QCOMPARE(s.getEventsCovering(100), EventVector({ b }));
        QCOMPARE(s.getEventsCovering(120), EventVector({ b }));
        QCOMPARE(s.getEventsCovering(130), EventVector());
    }
    
    void disjointEventsWithDurationSpan() {

        EventSeries s;
        Event a(10, 1.0f, 20, QString("a"));
        Event b(100, 1.2f, 30, QString("b"));
        s.add(a);
        s.add(b);
        QCOMPARE(s.getEventsSpanning(0, 10), EventVector());
        QCOMPARE(s.getEventsSpanning(10, 10), EventVector({ a }));
        QCOMPARE(s.getEventsSpanning(15, 85), EventVector({ a }));
        QCOMPARE(s.getEventsSpanning(30, 5), EventVector());
        QCOMPARE(s.getEventsSpanning(99, 1), EventVector());
        QCOMPARE(s.getEventsSpanning(100, 1), EventVector({ b }));
        QCOMPARE(s.getEventsSpanning(120, 20), EventVector({ b }));
        QCOMPARE(s.getEventsSpanning(130, 109), EventVector());
    }
    
    void overlappingEventsWithAndWithoutDurationCover() {

        EventSeries s;
        Event p(20, QString("p"));
        Event a(10, 1.0f, 20, QString("a"));
        s.add(p);
        s.add(a);
        EventVector cover;
        cover.push_back(a);
        QCOMPARE(s.getEventsCovering(15), cover);
        QCOMPARE(s.getEventsCovering(25), cover);
        cover.clear();
        cover.push_back(p);
        cover.push_back(a);
        QCOMPARE(s.getEventsCovering(20), cover);
    }
    
    void overlappingEventsWithAndWithoutDurationSpan() {

        EventSeries s;
        Event p(20, QString("p"));
        Event a(10, 1.0f, 20, QString("a"));
        s.add(p);
        s.add(a);
        EventVector span;
        span.push_back(a);
        QCOMPARE(s.getEventsSpanning(5, 10), span);
        QCOMPARE(s.getEventsSpanning(25, 5), span);
        span.clear();
        span.push_back(p);
        span.push_back(a);
        QCOMPARE(s.getEventsSpanning(20, 1), span);
    }

    void overlappingEventsWithDurationCover() {

        EventSeries s;
        Event a(20, 1.0f, 10, QString("a"));
        Event b(10, 1.0f, 20, QString("b"));
        Event c(10, 1.0f, 40, QString("c"));
        s.add(a);
        s.add(b);
        s.add(c);
        QCOMPARE(s.getEventsCovering(10), EventVector({ b, c }));
        QCOMPARE(s.getEventsCovering(20), EventVector({ b, c, a }));
        QCOMPARE(s.getEventsCovering(25), EventVector({ b, c, a }));
        QCOMPARE(s.getEventsCovering(30), EventVector({ c }));
        QCOMPARE(s.getEventsCovering(40), EventVector({ c }));
        QCOMPARE(s.getEventsCovering(50), EventVector());
    }

    void overlappingEventsWithDurationSpan() {

        EventSeries s;
        Event a(20, 1.0f, 10, QString("a"));
        Event b(10, 1.0f, 20, QString("b"));
        Event c(10, 1.0f, 40, QString("c"));
        s.add(a);
        s.add(b);
        s.add(c);
        QCOMPARE(s.getEventsSpanning(10, 5), EventVector({ b, c }));
        QCOMPARE(s.getEventsSpanning(20, 15), EventVector({ b, c, a }));
        QCOMPARE(s.getEventsSpanning(0, 100), EventVector({ b, c, a }));
        QCOMPARE(s.getEventsSpanning(25, 4), EventVector({ b, c, a }));
        QCOMPARE(s.getEventsSpanning(30, 4), EventVector({ c }));
        QCOMPARE(s.getEventsSpanning(40, 15), EventVector({ c }));
        QCOMPARE(s.getEventsSpanning(50, 10), EventVector());
    }

    void eventPatternCover() {

        EventSeries s;
        Event a(0, 1.0f, 18, QString("a"));
        Event b(3, 2.0f, 6, QString("b"));
        Event c(5, 3.0f, 2, QString("c"));
        Event cc(5, 3.1f, 2, QString("cc"));
        Event d(6, 4.0f, 10, QString("d"));
        Event dd(6, 4.5f, 10, QString("dd"));
        Event e(14, 5.0f, 3, QString("e"));
        s.add(b);
        s.add(c);
        s.add(d);
        s.add(a);
        s.add(cc);
        s.add(dd);
        s.add(e);
        QCOMPARE(s.getEventsCovering(8), EventVector({ a, b, d, dd }));
    }

    void eventPatternSpan() {

        EventSeries s;
        Event a(0, 1.0f, 18, QString("a"));
        Event b(3, 2.0f, 6, QString("b"));
        Event c(5, 3.0f, 2, QString("c"));
        Event cc(5, 3.1f, 2, QString("cc"));
        Event d(6, 4.0f, 10, QString("d"));
        Event dd(6, 4.5f, 10, QString("dd"));
        Event e(14, 5.0f, 3, QString("e"));
        s.add(b);
        s.add(c);
        s.add(d);
        s.add(a);
        s.add(cc);
        s.add(dd);
        s.add(e);
        QCOMPARE(s.getEventsSpanning(6, 2), EventVector({ a, b, c, cc, d, dd }));
    }

    void eventPatternWithin() {

        EventSeries s;
        Event a(0, 1.0f, 18, QString("a"));
        Event b(3, 2.0f, 6, QString("b"));
        Event c(5, 3.0f, 2, QString("c"));
        Event cc(5, 3.1f, 2, QString("cc"));
        Event d(6, 4.0f, 10, QString("d"));
        Event dd(6, 4.5f, 10, QString("dd"));
        Event e(14, 5.0f, 3, QString("e"));
        s.add(b);
        s.add(c);
        s.add(d);
        s.add(a);
        s.add(cc);
        s.add(dd);
        s.add(e);
        QCOMPARE(s.getEventsWithin(2, 7), EventVector({ b, c, cc }));
    }

    void eventPatternWithinWithOverspill() {

        EventSeries s;
        Event a(0, 1.0f, 18, QString("a"));
        Event b(3, 2.0f, 6, QString("b"));
        Event c(5, 3.0f, 2, QString("c"));
        Event cc(5, 3.1f, 2, QString("cc"));
        Event d(6, 4.0f, 10, QString("d"));
        Event dd(6, 4.5f, 10, QString("dd"));
        Event e(14, 5.0f, 3, QString("e"));
        s.add(b);
        s.add(c);
        s.add(d);
        s.add(a);
        s.add(cc);
        s.add(dd);
        s.add(e);
        QCOMPARE(s.getEventsWithin(0, 0, 0), EventVector());
        QCOMPARE(s.getEventsWithin(0, 0, 1), EventVector({ a }));
        QCOMPARE(s.getEventsWithin(0, 0, 2), EventVector({ a, b }));
        QCOMPARE(s.getEventsWithin(20, 1, 0), EventVector());
        QCOMPARE(s.getEventsWithin(20, 1, 1), EventVector({ e }));
        QCOMPARE(s.getEventsWithin(20, 1, 2), EventVector({ dd, e }));
        QCOMPARE(s.getEventsWithin(2, 7, 0), EventVector({ b, c, cc }));
        QCOMPARE(s.getEventsWithin(2, 7, 1), EventVector({ a, b, c, cc, d }));
        QCOMPARE(s.getEventsWithin(2, 7, 2), EventVector({ a, b, c, cc, d, dd }));
        QCOMPARE(s.getEventsWithin(2, 7, 3), EventVector({ a, b, c, cc, d, dd, e }));
        QCOMPARE(s.getEventsWithin(2, 7, 4), EventVector({ a, b, c, cc, d, dd, e }));
    }

    void eventPatternStartingWithin() {

        EventSeries s;
        Event a(0, 1.0f, 18, QString("a"));
        Event b(3, 2.0f, 6, QString("b"));
        Event c(5, 3.0f, 2, QString("c"));
        Event cc(5, 3.1f, 2, QString("cc"));
        Event d(6, 4.0f, 10, QString("d"));
        Event dd(6, 4.5f, 10, QString("dd"));
        Event e(14, 5.0f, 3, QString("e"));
        s.add(b);
        s.add(c);
        s.add(d);
        s.add(a);
        s.add(cc);
        s.add(dd);
        s.add(e);
        QCOMPARE(s.getEventsStartingWithin(2, 7),
                 EventVector({ b, c, cc, d, dd }));
    }

    void eventPatternStartingAt() {

        EventSeries s;
        Event a(0, 1.0f, 18, QString("a"));
        Event b(3, 2.0f, 6, QString("b"));
        Event c(5, 3.0f, 2, QString("c"));
        Event cc(5, 3.1f, 2, QString("cc"));
        Event d(6, 4.0f, 10, QString("d"));
        Event dd(6, 4.5f, 10, QString("dd"));
        Event e(14, 5.0f, 3, QString("e"));
        s.add(b);
        s.add(c);
        s.add(d);
        s.add(a);
        s.add(cc);
        s.add(dd);
        s.add(e);
        QCOMPARE(s.getEventsStartingAt(2), EventVector());
        QCOMPARE(s.getEventsStartingAt(5), EventVector({ c, cc }));
    }

    void eventPatternEndFrame() {

        EventSeries s;
        Event a(0, 1.0f, 18, QString("a"));
        Event b(3, 2.0f, 6, QString("b"));
        Event c(5, 3.0f, 2, QString("c"));
        Event cc(5, 3.1f, 2, QString("cc"));
        Event d(6, 4.0f, 10, QString("d"));
        Event dd(6, 4.5f, 10, QString("dd"));
        Event e(14, 5.0f, 3, QString("e"));
        s.add(b);
        s.add(c);
        s.add(d);
        s.add(a);
        s.add(cc);
        s.add(dd);
        s.add(e);
        QCOMPARE(s.getEndFrame(), sv_frame_t(18));
    }

    void eventPatternAddRemove() {

        // This is mostly here to exercise the innards of EventSeries
        // and check it doesn't crash out with any internal
        // consistency problems
        
        EventSeries s;
        Event a(0, 1.0f, 18, QString("a"));
        Event b(3, 2.0f, 6, QString("b"));
        Event c(5, 3.0f, 2, QString("c"));
        Event cc(5, 3.1f, 2, QString("cc"));
        Event d(6, 4.0f, 10, QString("d"));
        Event dd(6, 4.5f, 10, QString("dd"));
        Event e(14, 5.0f, 3, QString("e"));
        s.add(b);
        s.add(c);
        s.add(d);
        s.add(a);
        s.add(cc);
        s.add(dd);
        s.add(e);
        QCOMPARE(s.count(), 7);
        s.remove(d);
        QCOMPARE(s.getEventsCovering(8), EventVector({ a, b, dd }));
        QCOMPARE(s.getEndFrame(), sv_frame_t(18));
        s.remove(e);
        s.remove(a);
        QCOMPARE(s.getEventsCovering(8), EventVector({ b, dd }));
        QCOMPARE(s.getEndFrame(), sv_frame_t(16));
        s.remove(cc);
        s.remove(c);
        s.remove(dd);
        QCOMPARE(s.getEventsCovering(8), EventVector({ b }));
        QCOMPARE(s.getEndFrame(), sv_frame_t(9));
        s.remove(b);
        QCOMPARE(s.getEventsCovering(8), EventVector());
        QCOMPARE(s.count(), 0);
        QCOMPARE(s.isEmpty(), true);
        QCOMPARE(s.getEndFrame(), sv_frame_t(0));
    }

    void preceding() {
        
        EventSeries s;
        Event a(0, 1.0f, 18, QString("a"));
        Event b(3, 2.0f, 6, QString("b"));
        Event c(5, 3.0f, 2, QString("c"));
        Event cc(5, 3.1f, 2, QString("cc"));
        Event d(6, 4.0f, 10, QString("d"));
        Event dd(6, 4.5f, 10, QString("dd"));
        Event e(14, 5.0f, 3, QString("e"));
        s.add(b);
        s.add(c);
        s.add(d);
        s.add(d); // again
        s.add(a);
        s.add(cc);
        s.add(dd);
        s.add(e);
        Event p;
        QCOMPARE(s.getEventPreceding(e, p), true);
        QCOMPARE(p, dd);
        QCOMPARE(s.getEventPreceding(p, p), true);
        QCOMPARE(p, d);
        QCOMPARE(s.getEventPreceding(p, p), true);
        QCOMPARE(p, cc);
        QCOMPARE(s.getEventPreceding(p, p), true);
        QCOMPARE(p, c);
        QCOMPARE(s.getEventPreceding(p, p), true);
        QCOMPARE(p, b);
        QCOMPARE(s.getEventPreceding(p, p), true);
        QCOMPARE(p, a);
        QCOMPARE(s.getEventPreceding(p, p), false);
    }
    
    void following() {
        
        EventSeries s;
        Event a(0, 1.0f, 18, QString("a"));
        Event b(3, 2.0f, 6, QString("b"));
        Event c(5, 3.0f, 2, QString("c"));
        Event cc(5, 3.1f, 2, QString("cc"));
        Event d(6, 4.0f, 10, QString("d"));
        Event dd(6, 4.5f, 10, QString("dd"));
        Event e(14, 5.0f, 3, QString("e"));
        s.add(b);
        s.add(c);
        s.add(d);
        s.add(d); // again
        s.add(a);
        s.add(cc);
        s.add(dd);
        s.add(e);
        Event p;
        QCOMPARE(s.getEventFollowing(a, p), true);
        QCOMPARE(p, b);
        QCOMPARE(s.getEventFollowing(p, p), true);
        QCOMPARE(p, c);
        QCOMPARE(s.getEventFollowing(p, p), true);
        QCOMPARE(p, cc);
        QCOMPARE(s.getEventFollowing(p, p), true);
        QCOMPARE(p, d);
        QCOMPARE(s.getEventFollowing(p, p), true);
        QCOMPARE(p, dd);
        QCOMPARE(s.getEventFollowing(p, p), true);
        QCOMPARE(p, e);
        QCOMPARE(s.getEventFollowing(p, p), false);
    }
    
    void matchingForward() {
        
        EventSeries s;
        Event p;
        QCOMPARE(s.getNearestEventMatching
                 (6, [](Event e) { return e.getDuration() < 4; },
                  EventSeries::Forward, p), false);
        Event a(0, 1.0f, 18, QString("a"));
        Event b(3, 2.0f, 6, QString("b"));
        Event c(5, 3.0f, 2, QString("c"));
        Event cc(5, 3.1f, 2, QString("cc"));
        Event d(6, 4.0f, 10, QString("d"));
        Event dd(6, 4.5f, 10, QString("dd"));
        Event e(14, 5.0f, 3, QString("e"));
        s.add(b);
        s.add(c);
        s.add(d);
        s.add(d); // again
        s.add(a);
        s.add(cc);
        s.add(dd);
        s.add(e);
        QCOMPARE(s.getNearestEventMatching
                 (0, [](Event e) { return e.getDuration() < 4; },
                  EventSeries::Forward, p), true);
        QCOMPARE(p, c);
        QCOMPARE(s.getNearestEventMatching
                 (6, [](Event e) { return e.getDuration() < 4; },
                  EventSeries::Forward, p), true);
        QCOMPARE(p, e);
        QCOMPARE(s.getNearestEventMatching
                 (6, [](Event e) { return e.getDuration() > 4; },
                  EventSeries::Forward, p), true);
        QCOMPARE(p, d);
        QCOMPARE(s.getNearestEventMatching
                 (20, [](Event e) { return e.getDuration() > 4; },
                  EventSeries::Forward, p), false);
    }
    
    void matchingBackward() {
        
        EventSeries s;
        Event p;
        QCOMPARE(s.getNearestEventMatching
                 (6, [](Event e) { return e.getDuration() < 4; },
                  EventSeries::Backward, p), false);
        Event a(0, 1.0f, 18, QString("a"));
        Event b(3, 2.0f, 6, QString("b"));
        Event c(5, 3.0f, 2, QString("c"));
        Event cc(5, 3.1f, 2, QString("cc"));
        Event d(6, 4.0f, 10, QString("d"));
        Event dd(6, 4.5f, 10, QString("dd"));
        Event e(14, 5.0f, 3, QString("e"));
        s.add(b);
        s.add(c);
        s.add(d);
        s.add(d); // again
        s.add(a);
        s.add(cc);
        s.add(dd);
        s.add(e);
        QCOMPARE(s.getNearestEventMatching
                 (0, [](Event e) { return e.getDuration() < 4; },
                  EventSeries::Backward, p), false);
        QCOMPARE(s.getNearestEventMatching
                 (6, [](Event e) { return e.getDuration() > 4; },
                  EventSeries::Backward, p), true);
        QCOMPARE(p, b);
        QCOMPARE(s.getNearestEventMatching
                 (20, [](Event e) { return e.getDuration() > 4; },
                  EventSeries::Backward, p), true);
        QCOMPARE(p, dd);
    }
};

#endif
