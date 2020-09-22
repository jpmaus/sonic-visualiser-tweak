/* -*- c-basic-offset: 4 indent-tabs-mode: nil -*-  vi:set ts=8 sts=4 sw=4: */

/*
    Sonic Visualiser
    An audio file viewer and annotation editor.
    Centre for Digital Music, Queen Mary, University of London.
    This file copyright 2013 Chris Cannam.
    
    This program is free software; you can redistribute it and/or
    modify it under the terms of the GNU General Public License as
    published by the Free Software Foundation; either version 2 of the
    License, or (at your option) any later version.  See the file
    COPYING included with this distribution for more information.
*/

#ifndef TEST_MIDI_FILE_READER_H
#define TEST_MIDI_FILE_READER_H

#include "../MIDIFileReader.h"

#include <cmath>

#include <QObject>
#include <QtTest>
#include <QDir>

#include "base/Debug.h"

#include <iostream>

using namespace std;

class MIDIFileReaderTest : public QObject
{
    Q_OBJECT

private:
    QString testDirBase;
    QString midiDir;

    const char *strOf(QString s) {
        return strdup(s.toLocal8Bit().data());
    }

public:
    MIDIFileReaderTest(QString base) {
        if (base == "") {
            base = "svcore/data/fileio/test";
        }
        testDirBase = base;
        midiDir = base + "/midi";
    }

private slots:
    void init()
    {
        if (!QDir(midiDir).exists()) {
            SVCERR << "ERROR: MIDI file directory \"" << midiDir << "\" does not exist" << endl;
            QVERIFY2(QDir(midiDir).exists(), "MIDI file directory not found");
        }
    }

    void read_data()
    {
        QTest::addColumn<QString>("filename");
        QStringList files = QDir(midiDir).entryList(QDir::Files);
        foreach (QString filename, files) {
            QTest::newRow(strOf(filename)) << filename;
        }
    }
    
    void read()
    {
        QFETCH(QString, filename);
        QString path = midiDir + "/" + filename;
        MIDIFileReader reader(path, nullptr, 44100);
        Model *m = reader.load();
        if (!m) {
            SVCERR << "MIDI load failed for path: \"" << path << "\"" << endl;
        }
        QVERIFY(m != nullptr);
        //!!! Ah, now here we could do something a bit more informative
    }

};

#endif

