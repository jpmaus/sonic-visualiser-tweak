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

#include "CSVFeatureWriter.h"

#include <iostream>

#include <QRegExp>
#include <QTextStream>
#include <QTextCodec>

using namespace std;
using namespace Vamp;

CSVFeatureWriter::CSVFeatureWriter() :
    FileFeatureWriter(SupportOneFilePerTrackTransform |
                      SupportOneFileTotal |
                      SupportStdOut,
                      "csv"),
    m_separator(","),
    m_sampleTiming(false),
    m_endTimes(false),
    m_forceEnd(false),
    m_omitFilename(false),
    m_digits(6)
{
}

CSVFeatureWriter::~CSVFeatureWriter()
{
}

string
CSVFeatureWriter::getDescription() const
{
    return "Write features in comma-separated (CSV) format. If transforms are being written to a single file or to stdout, the first column in the output will contain the input audio filename, or an empty string if the feature hails from the same audio file as its predecessor. If transforms are being written to multiple files, the audio filename column will be omitted. Subsequent columns will contain the feature timestamp, then any or all of duration, values, and label.";
}

CSVFeatureWriter::ParameterList
CSVFeatureWriter::getSupportedParameters() const
{
    ParameterList pl = FileFeatureWriter::getSupportedParameters();
    Parameter p;
    
    p.name = "separator";
    p.description = "Column separator for output.  Default is \",\" (comma).";
    p.hasArg = true;
    pl.push_back(p);
    
    p.name = "omit-filename";
    p.description = "Omit the filename column. May result in confusion if sending more than one audio file's features to the same CSV output.";
    p.hasArg = false;
    pl.push_back(p);
    
    p.name = "sample-timing";
    p.description = "Show timings as sample frame counts instead of in seconds.";
    p.hasArg = false;
    pl.push_back(p);
    
    p.name = "end-times";
    p.description = "Show start and end time instead of start and duration, for features with duration.";
    p.hasArg = false;
    pl.push_back(p);

    p.name = "fill-ends";
    p.description = "Include durations (or end times) even for features without duration, by using the gap to the next feature instead.";
    p.hasArg = false;
    pl.push_back(p);

    p.name = "digits";
    p.description = "Specify the number of significant digits to use when printing transform outputs. Outputs are represented internally using single-precision floating-point, so digits beyond the 8th or 9th place are usually meaningless. The default is 6.";
    p.hasArg = true;
    pl.push_back(p);

    return pl;
}

void
CSVFeatureWriter::setParameters(map<string, string> &params)
{
    FileFeatureWriter::setParameters(params);

    SVDEBUG << "CSVFeatureWriter::setParameters" << endl;
    for (map<string, string>::iterator i = params.begin();
         i != params.end(); ++i) {
        SVDEBUG << i->first << " -> " << i->second << endl;
        if (i->first == "separator") {
            m_separator = i->second.c_str();
            SVDEBUG << "m_separator = " << m_separator << endl;
            if (m_separator == "\\t") {
                m_separator = QChar::Tabulation;
            }
        } else if (i->first == "sample-timing") {
            m_sampleTiming = true;
        } else if (i->first == "end-times") {
            m_endTimes = true;
        } else if (i->first == "fill-ends") {
            m_forceEnd = true;
        } else if (i->first == "omit-filename") {
            m_omitFilename = true;
        } else if (i->first == "digits") {
            int digits = atoi(i->second.c_str());
            if (digits <= 0 || digits > 100) {
                SVCERR << "CSVFeatureWriter: ERROR: Invalid or out-of-range value for number of significant digits: " << i->second << endl;
                SVCERR << "CSVFeatureWriter: NOTE: Continuing with default settings" << endl;
            } else {
                m_digits = digits;
            }
        }
    }
}

void
CSVFeatureWriter::write(QString trackId,
                        const Transform &transform,
                        const Plugin::OutputDescriptor& ,
                        const Plugin::FeatureList& features,
                        std::string summaryType)
{
    TransformId transformId = transform.getIdentifier();

    // Select appropriate output file for our track/transform
    // combination

    QTextStream *sptr = getOutputStream(trackId,
                                        transformId,
                                        QTextCodec::codecForName("UTF-8"));
    if (!sptr) {
        throw FailedToOpenOutputStream(trackId, transformId);
    }

    QTextStream &stream = *sptr;

    int n = (int)features.size();

    if (n == 0) return;

    DataId tt(trackId, transform);

    if (m_pending.find(tt) != m_pending.end()) {
        writeFeature(tt,
                     stream,
                     m_pending[tt],
                     &features[0],
                     m_pendingSummaryTypes[tt]);
        m_pending.erase(tt);
        m_pendingSummaryTypes.erase(tt);
    }

    if (m_forceEnd) {
        // can't write final feature until we know its end time
        --n;
        m_pending[tt] = features[n];
        m_pendingSummaryTypes[tt] = summaryType;
    }

    for (int i = 0; i < n; ++i) {
        writeFeature(tt, 
                     stream,
                     features[i], 
                     m_forceEnd ? &features[i+1] : nullptr,
                     summaryType);
    }
}

void
CSVFeatureWriter::finish()
{
    for (PendingFeatures::const_iterator i = m_pending.begin();
         i != m_pending.end(); ++i) {
        DataId tt = i->first;
        Plugin::Feature f = i->second;
        QTextStream *sptr = getOutputStream(tt.first,
                                            tt.second.getIdentifier(),
                                            QTextCodec::codecForName("UTF-8"));
        if (!sptr) {
            throw FailedToOpenOutputStream(tt.first, tt.second.getIdentifier());
        }
        QTextStream &stream = *sptr;
        // final feature has its own time as end time (we can't
        // reliably determine the end of audio file, and because of
        // the nature of block processing, the feature could even
        // start beyond that anyway)
        writeFeature(tt, stream, f, &f, m_pendingSummaryTypes[tt]);
    }

    m_pending.clear();
}

void
CSVFeatureWriter::writeFeature(DataId tt,
                               QTextStream &stream,
                               const Plugin::Feature &f,
                               const Plugin::Feature *optionalNextFeature,
                               std::string summaryType)
{
    QString trackId = tt.first;
    Transform transform = tt.second;

    if (!m_omitFilename) {
        if (m_stdout || m_singleFileName != "") {
            if (trackId != m_prevPrintedTrackId) {
                stream << "\"" << trackId << "\"" << m_separator;
                m_prevPrintedTrackId = trackId;
            } else {
                stream << m_separator;
            }
        }
    }

    ::RealTime duration;
    bool haveDuration = true;
    
    if (f.hasDuration) {
        duration = f.duration;
    } else if (optionalNextFeature) {
        duration = optionalNextFeature->timestamp - f.timestamp;
    } else {
        haveDuration = false;
    }

    if (m_sampleTiming) {

        sv_samplerate_t rate = transform.getSampleRate();

        stream << ::RealTime::realTime2Frame(f.timestamp, rate);

        if (haveDuration) {
            stream << m_separator;
            if (m_endTimes) {
                stream << ::RealTime::realTime2Frame
                    (::RealTime(f.timestamp) + duration, rate);
            } else {
                stream << ::RealTime::realTime2Frame(duration, rate);
            }
        }

    } else {

        QString timestamp = f.timestamp.toString().c_str();
        timestamp.replace(QRegExp("^ +"), "");
        stream << timestamp;

        if (haveDuration) {
            if (m_endTimes) {
                QString endtime =
                    (::RealTime(f.timestamp) + duration).toString().c_str();
                endtime.replace(QRegExp("^ +"), "");
                stream << m_separator << endtime;
            } else {
                QString d = ::RealTime(duration).toString().c_str();
                d.replace(QRegExp("^ +"), "");
                stream << m_separator << d;
            }
        }            
    }

    if (summaryType != "") {
        stream << m_separator << summaryType.c_str();
    }
    
    for (unsigned int j = 0; j < f.values.size(); ++j) {

        QString number = QString("%1").arg(f.values[j], 0, 'g', m_digits);

        // Qt pre-5.6 zero pads single-digit exponents to two digits;
        // Qt 5.7+ doesn't by default. But we want both to produce the
        // same output. Getting the new behaviour from standard APIs
        // in Qt 5.6 isn't possible I think; getting the old behaviour
        // from Qt 5.7 is possible but fiddly, involving setting up an
        // appropriate locale and using the %L specifier. We could
        // doubtless do it with sprintf but Qt is a known quantity at
        // this point. Let's just convert the old format to the new.
        number.replace("e-0", "e-");
        
        stream << m_separator << number;
    }
    
    if (f.label != "") {
        stream << m_separator << "\"" << f.label.c_str() << "\"";
    }
    
    stream << "\n";
}


