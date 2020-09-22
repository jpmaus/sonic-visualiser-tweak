/* -*- c-basic-offset: 4 indent-tabs-mode: nil -*-  vi:set ts=8 sts=4 sw=4: */

/*
    Sonic Visualiser
    An audio file viewer and annotation editor.
    Centre for Digital Music, Queen Mary, University of London.
    This file copyright 2008 QMUL.
   
    This program is free software; you can redistribute it and/or
    modify it under the terms of the GNU General Public License as
    published by the Free Software Foundation; either version 2 of the
    License, or (at your option) any later version.  See the file
    COPYING included with this distribution for more information.
*/

#ifndef SV_RDF_IMPORTER_H
#define SV_RDF_IMPORTER_H

#include <QObject>
#include <QString>

#include <vector>

#include "base/BaseTypes.h"
#include "data/model/Model.h"

class RDFImporterImpl;
class ProgressReporter;

class RDFImporter : public QObject
{
    Q_OBJECT

public:
    /**
     * Return the file extensions that we have data file readers for,
     * in a format suitable for use with QFileDialog.  For example,
     * "*.rdf *.n3".
     */
    static QString getKnownExtensions();

    RDFImporter(QString url, sv_samplerate_t sampleRate = 0);
    virtual ~RDFImporter();

    void setSampleRate(sv_samplerate_t sampleRate);

    bool isOK();
    QString getErrorString() const;

    /**
     * Return a list of models imported from the RDF source. The
     * models have been newly created and registered with ById; the
     * caller must arrange to release them.
     */
    std::vector<ModelId> getDataModels(ProgressReporter *reporter);

    enum RDFDocumentType {
        AudioRefAndAnnotations,
        Annotations,
        AudioRef,
        OtherRDFDocument,
        NotRDF
    };

    static RDFDocumentType identifyDocumentType(QString url);

protected:
    RDFImporterImpl *m_d;
};

#endif
