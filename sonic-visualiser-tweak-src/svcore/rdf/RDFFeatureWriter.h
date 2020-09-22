/* -*- c-basic-offset: 4 indent-tabs-mode: nil -*-  vi:set ts=8 sts=4 sw=4: */

/*
    Sonic Visualiser
    An audio file viewer and annotation editor.

    Sonic Annotator
    A utility for batch feature extraction from audio files.

    Mark Levy, Chris Sutton and Chris Cannam, Queen Mary, University of London.
    Copyright 2007-2008 QMUL.

    This program is free software; you can redistribute it and/or
    modify it under the terms of the GNU General Public License as
    published by the Free Software Foundation; either version 2 of the
    License, or (at your option) any later version.  See the file
    COPYING included with this distribution for more information.
*/

#ifndef SV_RDF_FEATURE_WRITER_H
#define SV_RDF_FEATURE_WRITER_H

#include <string>
#include <map>
#include <set>

#include <QString>

#include "transform/FileFeatureWriter.h"

#include "PluginRDFDescription.h"

using std::string;
using std::map;
using std::set;
using std::pair;

class QTextStream;
class QFile;

class RDFFeatureWriter : public FileFeatureWriter
{
public:
    RDFFeatureWriter();
    virtual ~RDFFeatureWriter();

    string getDescription() const override;

    ParameterList getSupportedParameters() const override;
    void setParameters(map<string, string> &params) override;

    void setTrackMetadata(QString trackid, TrackMetadata metadata) override;

    void write(QString trackid,
                       const Transform &transform,
                       const Vamp::Plugin::OutputDescriptor &output,
                       const Vamp::Plugin::FeatureList &features,
                       std::string summaryType = "") override;

    virtual void setFixedEventTypeURI(QString uri); // something of a hack

    void finish() override;

    QString getWriterTag() const override { return "rdf"; }

private:
    typedef map<QString, PluginRDFDescription> RDFDescriptionMap; // by plugin id
    RDFDescriptionMap m_rdfDescriptions;

    typedef map<QString, TrackMetadata> TrackMetadataMap;
    TrackMetadataMap m_metadata;

    bool haveTitleArtistMetadata(QString trackId) const {
        // Formerly in various places we used to test whether a track
        // appeared in the metadata map at all, in order to determine
        // whether it had any associated metadata. That won't work any
        // more because metadata now includes duration, which can
        // appear even if no title/artist are given and which is not
        // something whose presence indicates the involvement of a
        // "publication Track". So check for artist/title explicitly.
        auto mitr = m_metadata.find(trackId);
        if (mitr == m_metadata.end()) return false;
        return (mitr->second.title != "" || mitr->second.maker != "");
    }

    QString m_fixedEventTypeURI;

    void reviewFileForAppending(QString filename) override;

    void writePrefixes(QTextStream *);
    void writeSignalDescription(QTextStream *, QString);
    void writeLocalFeatureTypes(QTextStream *,
                                const Transform &,
                                const Vamp::Plugin::OutputDescriptor &,
                                PluginRDFDescription &,
                                std::string summaryType);

    void writeSparseRDF(QTextStream *stream,
                        const Transform &transform,
                        const Vamp::Plugin::OutputDescriptor &output,
                        const Vamp::Plugin::FeatureList &features,
                        PluginRDFDescription &desc,
                        QString timelineURI);

    void writeTrackLevelRDF(QTextStream *stream,
                            const Transform &transform,
                            const Vamp::Plugin::OutputDescriptor &output,
                            const Vamp::Plugin::FeatureList &features,
                            PluginRDFDescription &desc,
                            QString signalURI);

    void writeDenseRDF(QTextStream *stream,
                       const Transform &transform,
                       const Vamp::Plugin::OutputDescriptor &output,
                       const Vamp::Plugin::FeatureList &features,
                       PluginRDFDescription &desc,
                       QString signalURI,
                       QString timelineURI);

    set<QString> m_startedTrackIds;

    map<QTextStream *, set<Transform> > m_startedStreamTransforms;

    map<QString, QString> m_trackTrackURIs;
    map<QString, QString> m_trackTimelineURIs;
    map<QString, QString> m_trackSignalURIs;

    map<Transform, QString> m_transformURIs;
    map<Transform, QString> m_syntheticEventTypeURIs;
    map<Transform, QString> m_syntheticSignalTypeURIs;

    typedef pair<QString, Transform> StringTransformPair;
    typedef pair<QTextStream *, QString> StreamBuffer;
    map<StringTransformPair, StreamBuffer> m_openDenseFeatures; // signal URI + transform -> stream + text
    QString m_userAudioFileUri;
    QString m_userTrackUri;
    QString m_userMakerUri;

    bool m_plain;

    bool m_network;
    bool m_networkRetrieved;

    long m_count;
};

#endif
