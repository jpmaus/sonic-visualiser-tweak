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

#ifndef SV_BOGUS_AUDIO_FILE_READER_TEST_H
#define SV_BOGUS_AUDIO_FILE_READER_TEST_H

#include "../AudioFileReaderFactory.h"

#include "base/TempDirectory.h"

#include <QObject>
#include <QtTest>
#include <QDir>

// Tests for malformed audio files - primarily to ensure we don't crash

class BogusAudioFileReaderTest : public QObject
{
    Q_OBJECT

private slots:
    void bogus_data()
    {
        QTest::addColumn<QString>("format");
        QTest::addColumn<bool>("empty");
        QStringList patterns = AudioFileReaderFactory::getKnownExtensions()
            .split(" ", QString::SkipEmptyParts);

        for (auto p: patterns) {

            QStringList bits = p.split(".");
            QString extension = bits[bits.size()-1];

            QString testName = QString("%1, empty").arg(extension);
            QTest::newRow(strdup(testName.toLocal8Bit().data()))
                << extension << true;

            testName = QString("%1, nonsense").arg(extension);
            QTest::newRow(strdup(testName.toLocal8Bit().data()))
                << extension << false;
        }
    }

    void bogus()
    {
        QFETCH(QString, format);
        QFETCH(bool, empty);

        if (format == "au") { // au is headerless, so any file is legal
#if ( QT_VERSION >= 0x050000 )
            QSKIP("Skipping headerless file");
#else
            QSKIP("Skipping headerless file", SkipSingle);
#endif
        }
            
        QString tmpdir = TempDirectory::getInstance()->getPath();

        QString path = QString("%1/%2.%3")
            .arg(tmpdir)
            .arg(empty ? "empty" : "nonsense")
            .arg(format);
        QFile f(path);
        if (!f.open(QIODevice::WriteOnly)) {
            std::cerr << "Failed to create temporary file "
                      << path << std::endl;
            throw std::runtime_error("Failed to create temporary file");
        }
        if (!empty) {
            for (int i = 0; i < 1000; ++i) {
                f.write("weeble");
            }
        }
        f.close();

        AudioFileReader *reader =
            AudioFileReaderFactory::createReader(path, {});
        QCOMPARE((void *)reader, (void *)0);
    }

};

#endif
