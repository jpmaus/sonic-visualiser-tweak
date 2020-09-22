/* -*- c-basic-offset: 4 indent-tabs-mode: nil -*-  vi:set ts=8 sts=4 sw=4: */

/*
    Sonic Visualiser
    An audio file viewer and annotation editor.
    Centre for Digital Music, Queen Mary, University of London.
    This file copyright 2006 Chris Cannam.
    
    This program is free software; you can redistribute it and/or
    modify it under the terms of the GNU General Public License as
    published by the Free Software Foundation; either version 2 of the
    License, or (at your option) any later version.  See the file
    COPYING included with this distribution for more information.
*/

#ifndef SV_CSV_FILE_WRITER_H
#define SV_CSV_FILE_WRITER_H

#include <QObject>
#include <QString>

#include "base/DataExportOptions.h"

class Model;
class MultiSelection;
class ProgressReporter;

class CSVFileWriter : public QObject
{
    Q_OBJECT

public:
    CSVFileWriter(QString path,
                  Model *model,
                  QString delimiter = ",",
                  DataExportOptions options = DataExportDefaults);

    CSVFileWriter(QString path,
                  Model *model,
                  ProgressReporter *reporter,
                  QString delimiter = ",",
                  DataExportOptions options = DataExportDefaults) 
    : CSVFileWriter(path, model, delimiter, options)
    {
        m_reporter = reporter;
    }
    virtual ~CSVFileWriter();

    virtual bool isOK() const;
    virtual QString getError() const;

    virtual void write();
    virtual void writeSelection(MultiSelection selection);

protected:
    QString m_path;
    Model *m_model;
    QString m_error;
    QString m_delimiter;
    DataExportOptions m_options;
    ProgressReporter *m_reporter = nullptr;
};

#endif

    
