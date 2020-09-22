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

#ifndef TEST_SPARSE_MODELS_H
#define TEST_SPARSE_MODELS_H

#include "../SparseOneDimensionalModel.h"
#include "../NoteModel.h"
#include "../TextModel.h"
#include "../Path.h"
#include "../ImageModel.h"

#include <QObject>
#include <QtTest>

#include <iostream>

using namespace std;

// NB model & dataset IDs in the export tests are incremental,
// depending on how many have been exported in previous tests - so
// when adding or removing tests we may occasionally need to update
// the IDs in other ones

class TestSparseModels : public QObject
{
    Q_OBJECT

private slots:
    void s1d_empty() {
        SparseOneDimensionalModel m(100, 10, false);
        QCOMPARE(m.isEmpty(), true);
        QCOMPARE(m.getEventCount(), 0);
        QCOMPARE(m.getAllEvents().size(), size_t(0));
        QCOMPARE(m.getStartFrame(), sv_frame_t(0));
        QCOMPARE(m.getEndFrame(), sv_frame_t(0));
        QCOMPARE(m.getSampleRate(), 100.0);
        QCOMPARE(m.getResolution(), 10);
        QCOMPARE(m.isSparse(), true);

        Event p(10);
        m.add(p);
        m.remove(p);
        QCOMPARE(m.isEmpty(), true);
        QCOMPARE(m.getEventCount(), 0);
        QCOMPARE(m.getAllEvents().size(), size_t(0));
        QCOMPARE(m.getStartFrame(), sv_frame_t(0));
        QCOMPARE(m.getEndFrame(), sv_frame_t(0));
    }

    void s1d_extents() {
        SparseOneDimensionalModel m(100, 10, false);
        Event p1(20);
        m.add(p1);
        QCOMPARE(m.isEmpty(), false);
        QCOMPARE(m.getEventCount(), 1);
        Event p2(50);
        m.add(p2);
        QCOMPARE(m.isEmpty(), false);
        QCOMPARE(m.getEventCount(), 2);
        QCOMPARE(m.getAllEvents().size(), size_t(2));
        QCOMPARE(*m.getAllEvents().begin(), p1);
        QCOMPARE(*m.getAllEvents().rbegin(), p2);
        QCOMPARE(m.getStartFrame(), sv_frame_t(20));
        QCOMPARE(m.getEndFrame(), sv_frame_t(60));
        QCOMPARE(m.containsEvent(p1), true);
        m.remove(p1);
        QCOMPARE(m.getEventCount(), 1);
        QCOMPARE(m.getAllEvents().size(), size_t(1));
        QCOMPARE(*m.getAllEvents().begin(), p2);
        QCOMPARE(m.getStartFrame(), sv_frame_t(50));
        QCOMPARE(m.getEndFrame(), sv_frame_t(60));
        QCOMPARE(m.containsEvent(p1), false);
    }
             
    void s1d_sample() {
        SparseOneDimensionalModel m(100, 10, false);
        Event p1(20), p2(20), p3(50);
        m.add(p1);
        m.add(p2);
        m.add(p3);
        QCOMPARE(m.getAllEvents().size(), size_t(3));
        QCOMPARE(*m.getAllEvents().begin(), p1);
        QCOMPARE(*m.getAllEvents().rbegin(), p3);

        // The EventSeries that is used internally is tested more
        // thoroughly in its own test suite. This is just a check
        auto pp = m.getEventsWithin(20, 10);
        QCOMPARE(pp.size(), size_t(2));
        QCOMPARE(*pp.begin(), p1);
        QCOMPARE(*pp.rbegin(), p2);
        
        pp = m.getEventsWithin(40, 10);
        QCOMPARE(pp.size(), size_t(0));

        pp = m.getEventsStartingAt(50);
        QCOMPARE(pp.size(), size_t(1));
        QCOMPARE(*pp.begin(), p3);
    }

    void s1d_xml() {
        SparseOneDimensionalModel m(100, 10, false);
        m.setObjectName("This \"&\" that");
        Event p1(20);
        Event p2(20, "Label &'\">");
        Event p3(50, 12.4f, 16, ""); // value + duration should not be saved
        m.add(p1);
        m.add(p2);
        m.add(p3);
        QString xml;
        QTextStream str(&xml, QIODevice::WriteOnly);
        m.toXml(str);
        str.flush();
        QString expected =
            "<model id='1' name='This &quot;&amp;&quot; that' sampleRate='100' start='20' end='60' type='sparse' dimensions='1' resolution='10' notifyOnAdd='true' dataset='0' />\n"
            "<dataset id='0' dimensions='1'>\n"
            "  <point frame='20' label='' />\n"
            "  <point frame='20' label='Label &amp;&apos;&quot;&gt;' />\n"
            "  <point frame='50' label='' />\n"
            "</dataset>\n";
        expected.replace("\'", "\"");
        if (xml != expected) {
            cerr << "Obtained xml:\n" << xml
                 << "\nExpected:\n" << expected << std::endl;
        }
        QCOMPARE(xml, expected);
    }

    void note_extents() {
        NoteModel m(100, 10, false);
        Event p1(20, 123.4f, 40, 0.8f, "note 1");
        m.add(p1);
        QCOMPARE(m.isEmpty(), false);
        QCOMPARE(m.getEventCount(), 1);
        Event p2(50, 124.3f, 30, 0.9f, "note 2");
        m.add(p2);
        QCOMPARE(m.isEmpty(), false);
        QCOMPARE(m.getEventCount(), 2);
        QCOMPARE(m.getAllEvents().size(), size_t(2));
        QCOMPARE(*m.getAllEvents().begin(), p1);
        QCOMPARE(*m.getAllEvents().rbegin(), p2);
        QCOMPARE(m.getStartFrame(), sv_frame_t(20));
        QCOMPARE(m.getEndFrame(), sv_frame_t(80));
        QCOMPARE(m.containsEvent(p1), true);
        QCOMPARE(m.getValueMinimum(), 123.4f);
        QCOMPARE(m.getValueMaximum(), 124.3f);
        m.remove(p1);
        QCOMPARE(m.getEventCount(), 1);
        QCOMPARE(m.getAllEvents().size(), size_t(1));
        QCOMPARE(*m.getAllEvents().begin(), p2);
        QCOMPARE(m.getStartFrame(), sv_frame_t(50));
        QCOMPARE(m.getEndFrame(), sv_frame_t(80));
        QCOMPARE(m.containsEvent(p1), false);
    }
             
    void note_sample() {
        NoteModel m(100, 10, false);
        Event p1(20, 123.4f, 10, 0.8f, "note 1");
        Event p2(20, 124.3f, 20, 0.9f, "note 2");
        Event p3(50, 126.3f, 30, 0.9f, "note 3");
        m.add(p1);
        m.add(p2);
        m.add(p3);

        QCOMPARE(m.getAllEvents().size(), size_t(3));
        QCOMPARE(*m.getAllEvents().begin(), p1);
        QCOMPARE(*m.getAllEvents().rbegin(), p3);

        auto pp = m.getEventsSpanning(20, 10);
        QCOMPARE(pp.size(), size_t(2));
        QCOMPARE(*pp.begin(), p1);
        QCOMPARE(*pp.rbegin(), p2);

        pp = m.getEventsSpanning(30, 20);
        QCOMPARE(pp.size(), size_t(1));
        QCOMPARE(*pp.begin(), p2);

        pp = m.getEventsSpanning(40, 10);
        QCOMPARE(pp.size(), size_t(0));

        pp = m.getEventsCovering(50);
        QCOMPARE(pp.size(), size_t(1));
        QCOMPARE(*pp.begin(), p3);
    }

    void note_xml() {
        NoteModel m(100, 10, false);
        Event p1(20, 123.4f, 20, 0.8f, "note 1");
        Event p2(20, 124.3f, 10, 0.9f, "note 2");
        Event p3(50, 126.3f, 30, 0.9f, "note 3");
        m.setScaleUnits("Hz");
        m.add(p1);
        m.add(p2);
        m.add(p3);
        QString xml;
        QTextStream str(&xml, QIODevice::WriteOnly);
        m.toXml(str);
        str.flush();
        
        QString expected =
            "<model id='3' name='' sampleRate='100' start='20' end='80' type='sparse' dimensions='3' resolution='10' notifyOnAdd='true' dataset='2' subtype='note' valueQuantization='0' minimum='123.4' maximum='126.3' units='Hz' />\n"
            "<dataset id='2' dimensions='3'>\n"
            "  <point frame='20' value='124.3' duration='10' level='0.9' label='note 2' />\n"
            "  <point frame='20' value='123.4' duration='20' level='0.8' label='note 1' />\n"
            "  <point frame='50' value='126.3' duration='30' level='0.9' label='note 3' />\n"
            "</dataset>\n";
        expected.replace("\'", "\"");
        if (xml != expected) {
            cerr << "Obtained xml:\n" << xml
                 << "\nExpected:\n" << expected << std::endl;
        }
        QCOMPARE(xml, expected);
    }

    void text_xml() {
        TextModel m(100, 10, false);
        Event p1(20, 1.0f, "text 1");
        Event p2(20, 0.0f, "text 2");
        Event p3(50, 0.3f, "text 3");
        m.add(p1);
        m.add(p2.withLevel(0.8f));
        m.add(p3);
        QString xml;
        QTextStream str(&xml, QIODevice::WriteOnly);
        m.toXml(str);
        str.flush();

        QString expected =
            "<model id='5' name='' sampleRate='100' start='20' end='60' type='sparse' dimensions='2' resolution='10' notifyOnAdd='true' dataset='4' subtype='text' />\n"
            "<dataset id='4' dimensions='2'>\n"
            "  <point frame='20' height='0' label='text 2' />\n"
            "  <point frame='20' height='1' label='text 1' />\n"
            "  <point frame='50' height='0.3' label='text 3' />\n"
            "</dataset>\n";
        expected.replace("\'", "\"");
        if (xml != expected) {
            cerr << "Obtained xml:\n" << xml
                 << "\nExpected:\n" << expected << std::endl;
        }
        QCOMPARE(xml, expected);
    }
    
    void path_xml() {
        Path m(100, 10);
        PathPoint p1(20, 30);
        PathPoint p2(40, 60);
        PathPoint p3(50, 49);
        m.add(p1);
        m.add(p2);
        m.add(p3);
        QString xml;
        QTextStream str(&xml, QIODevice::WriteOnly);
        m.toXml(str);
        str.flush();

        QString expected =
            "<model id='6' name='' sampleRate='100' start='20' end='60' type='sparse' dimensions='2' resolution='10' notifyOnAdd='true' dataset='6' subtype='path' />\n"
            "<dataset id='6' dimensions='2'>\n"
            "  <point frame='20' mapframe='30' />\n"
            "  <point frame='40' mapframe='60' />\n"
            "  <point frame='50' mapframe='49' />\n"
            "</dataset>\n";
        expected.replace("\'", "\"");
        if (xml != expected) {
            cerr << "Obtained xml:\n" << xml
                 << "\nExpected:\n" << expected << std::endl;
        }
        QCOMPARE(xml, expected);
    }

    void image_xml() {
        ImageModel m(100, 10, false);
        Event p1(20, 30, 40, "a label"); // value + duration should not be saved
        m.add(p1.withURI("/path/to/thing.png").withLevel(0.8f));
        QString xml;
        QTextStream str(&xml, QIODevice::WriteOnly);
        m.toXml(str);
        str.flush();

        QString expected =
            "<model id='8' name='' sampleRate='100' start='20' end='30' type='sparse' dimensions='1' resolution='10' notifyOnAdd='true' dataset='7' subtype='image' />\n"
            "<dataset id='7' dimensions='1'>\n"
            "  <point frame='20' label='a label' image='/path/to/thing.png' />\n"
            "</dataset>\n";
        expected.replace("\'", "\"");
        if (xml != expected) {
            cerr << "Obtained xml:\n" << xml
                 << "\nExpected:\n" << expected << std::endl;
        }
        QCOMPARE(xml, expected);
    }
};

#endif
