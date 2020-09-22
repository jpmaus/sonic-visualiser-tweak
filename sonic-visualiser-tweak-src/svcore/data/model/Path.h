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

#ifndef SV_PATH_H
#define SV_PATH_H

#include "base/XmlExportable.h"
#include "base/RealTime.h"
#include "base/BaseTypes.h"

#include <QStringList>
#include <set>

struct PathPoint
{
    PathPoint(sv_frame_t _frame) :
        frame(_frame), mapframe(_frame) { }
    PathPoint(sv_frame_t _frame, sv_frame_t _mapframe) :
        frame(_frame), mapframe(_mapframe) { }

    sv_frame_t frame;
    sv_frame_t mapframe;

    void toXml(QTextStream &stream, QString indent = "",
               QString extraAttributes = "") const {
        stream << QString("%1<point frame=\"%2\" mapframe=\"%3\" %4/>\n")
            .arg(indent).arg(frame).arg(mapframe).arg(extraAttributes);
    }
        
    QString toDelimitedDataString(QString delimiter, DataExportOptions,
                                  sv_samplerate_t sampleRate) const {
        QStringList list;
        list << RealTime::frame2RealTime(frame, sampleRate).toString().c_str();
        list << QString("%1").arg(mapframe);
        return list.join(delimiter);
    }

    bool operator<(const PathPoint &p2) const {
        if (frame != p2.frame) return frame < p2.frame;
        return mapframe < p2.mapframe;
    }
};

class Path : public XmlExportable
{
public:
    Path(sv_samplerate_t sampleRate, int resolution) :
        m_sampleRate(sampleRate),
        m_resolution(resolution) {
    }
    Path(const Path &) =default;
    Path &operator=(const Path &) =default;

    typedef std::set<PathPoint> Points;

    sv_samplerate_t getSampleRate() const { return m_sampleRate; }
    int getResolution() const { return m_resolution; }

    int getPointCount() const {
        return int(m_points.size());
    }

    const Points &getPoints() const {
        return m_points;
    }

    void add(PathPoint p) {
        m_points.insert(p);
    }
    
    void remove(PathPoint p) {
        m_points.erase(p);
    }

    void clear() {
        m_points.clear();
    }

    /**
     * XmlExportable methods.
     */
    void toXml(QTextStream &out,
               QString indent = "",
               QString extraAttributes = "") const override {

        // For historical reasons we serialise a Path as a model,
        // although the class itself no longer is.

        // We also write start and end frames - which our API no
        // longer exposes - just for backward compatibility

        sv_frame_t start = 0;
        sv_frame_t end = 0;
        if (!m_points.empty()) {
            start = m_points.begin()->frame;
            end = m_points.rbegin()->frame + m_resolution;
        }
        
        // Our dataset doesn't have its own export ID, we just use
        // ours. Actually any model could do that, since datasets
        // aren't in the same id-space as models (or paths) when
        // re-read
        
        out << indent;
        out << QString("<model id=\"%1\" name=\"\" sampleRate=\"%2\" "
                       "start=\"%3\" end=\"%4\" type=\"sparse\" "
                       "dimensions=\"2\" resolution=\"%5\" "
                       "notifyOnAdd=\"true\" dataset=\"%6\" "
                       "subtype=\"path\" %7/>\n")
            .arg(getExportId())
            .arg(m_sampleRate)
            .arg(start)
            .arg(end)
            .arg(m_resolution)
            .arg(getExportId())
            .arg(extraAttributes);

        out << indent << QString("<dataset id=\"%1\" dimensions=\"2\">\n")
            .arg(getExportId());
        
        for (PathPoint p: m_points) {
            p.toXml(out, indent + "  ", "");
        }

        out << indent << "</dataset>\n";
    }

    QString toDelimitedDataString(QString delimiter,
                                  DataExportOptions,
                                  sv_frame_t startFrame,
                                  sv_frame_t duration) const {

        QString s;
        for (PathPoint p: m_points) {
            if (p.frame < startFrame) continue;
            if (p.frame >= startFrame + duration) break;
            s += QString("%1%2%3\n")
                .arg(p.frame)
                .arg(delimiter)
                .arg(p.mapframe);
        }

        return s;
    }
    
protected:
    sv_samplerate_t m_sampleRate;
    int m_resolution;
    Points m_points;
};


#endif
