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

#include "DataFileReaderFactory.h"
#include "MIDIFileReader.h"
#include "CSVFileReader.h"

#include "model/Model.h"

#include <QString>

QString
DataFileReaderFactory::getKnownExtensions()
{
    return "*.svl *.csv *.lab *.mid *.txt";
}

DataFileReader *
DataFileReaderFactory::createReader(QString path,
                                    bool csv,
                                    MIDIFileImportPreferenceAcquirer *acquirer,
                                    CSVFormat format,
                                    sv_samplerate_t mainModelSampleRate,
                                    ProgressReporter *reporter)
{
    QString err;

    DataFileReader *reader = nullptr;

    if (!csv) {
        reader = new MIDIFileReader(path,
                                    acquirer,
                                    mainModelSampleRate,
                                    reporter);
        if (reader->isOK()) return reader;
        if (reader->getError() != "") err = reader->getError();
        delete reader;
    }

    if (csv) {
        reader = new CSVFileReader(path,
                                   format,
                                   mainModelSampleRate,
                                   reporter);
        if (reader->isOK()) return reader;
        if (reader->getError() != "") err = reader->getError();
        delete reader;
    }

    return nullptr;
}

DataFileReader *
DataFileReaderFactory::createReader(QString path,
                                    MIDIFileImportPreferenceAcquirer *acquirer,
                                    sv_samplerate_t mainModelSampleRate,
                                    ProgressReporter *reporter)
{
    DataFileReader *reader = createReader
        (path, false, acquirer, CSVFormat(), mainModelSampleRate, reporter);
    if (reader) return reader;

    reader = createReader
        (path, true, acquirer, CSVFormat(path), mainModelSampleRate, reporter);
    if (reader) return reader;

    return nullptr;
}

Model *
DataFileReaderFactory::load(QString path,
                            MIDIFileImportPreferenceAcquirer *acquirer,
                            sv_samplerate_t mainModelSampleRate,
                            ProgressReporter *reporter)
{
    DataFileReader *reader = createReader(path,
                                          acquirer,
                                          mainModelSampleRate,
                                          reporter);
    if (!reader) return nullptr;

    try {
        Model *model = reader->load();
        delete reader;
        return model;
    } catch (Exception) {
        delete reader;
        throw;
    }
}

Model *
DataFileReaderFactory::loadNonCSV(QString path,
                                  MIDIFileImportPreferenceAcquirer *acquirer,
                                  sv_samplerate_t mainModelSampleRate,
                                  ProgressReporter *reporter)
{
    DataFileReader *reader = createReader(path, false,
                                          acquirer,
                                          CSVFormat(),
                                          mainModelSampleRate,
                                          reporter);
    if (!reader) return nullptr;

    try {
        Model *model = reader->load();
        delete reader;
        return model;
    } catch (Exception) {
        delete reader;
        throw;
    }
}

Model *
DataFileReaderFactory::loadCSV(QString path, CSVFormat format,
                               sv_samplerate_t mainModelSampleRate,
                               ProgressReporter *reporter)
{
    DataFileReader *reader = createReader(path, true, nullptr, format,
                                          mainModelSampleRate,
                                          reporter);
    if (!reader) return nullptr;

    try {
        Model *model = reader->load();
        delete reader;
        return model;
    } catch (Exception) {
        delete reader;
        throw;
    }
}

