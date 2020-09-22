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

#include "RecordDirectory.h"
#include "TempDirectory.h"

#include <QDir>
#include <QDateTime>

#include "Debug.h"

QString
RecordDirectory::getRecordContainerDirectory()
{
    QDir parent(TempDirectory::getInstance()->getContainingPath());
    QString subdirname("recorded");

    if (!parent.mkpath(subdirname)) {
        SVCERR << "ERROR: RecordDirectory::getRecordContainerDirectory: Failed to create recorded dir in \"" << parent.canonicalPath() << "\"" << endl;
        return "";
    } else {
        return parent.filePath(subdirname);
    }
}

QString
RecordDirectory::getRecordDirectory()
{
    QDir parent(getRecordContainerDirectory());
    QDateTime now = QDateTime::currentDateTime();
    QString subdirname = QString("%1").arg(now.toString("yyyyMMdd"));

    if (!parent.mkpath(subdirname)) {
        SVCERR << "ERROR: RecordDirectory::getRecordDirectory: Failed to create recorded dir in \"" << parent.canonicalPath() << "\"" << endl;
        return "";
    } else {
        return parent.filePath(subdirname);
    }
}

QString
RecordDirectory::getConvertedAudioDirectory()
{
    QDir parent(getRecordContainerDirectory());
    QDateTime now = QDateTime::currentDateTime();
    QString subdirname = "converted";

    if (!parent.mkpath(subdirname)) {
        SVCERR << "ERROR: RecordDirectory::getConvertedAudioDirectory: Failed to create recorded dir in \"" << parent.canonicalPath() << "\"" << endl;
        return "";
    } else {
        return parent.filePath(subdirname);
    }
}


