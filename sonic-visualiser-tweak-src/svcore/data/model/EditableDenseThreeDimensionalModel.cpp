/* -*- c-basic-offset: 4 indent-tabs-mode: nil -*-  vi:set ts=8 sts=4 sw=4: */

/*
    Sonic Visualiser
    An audio file viewer and annotation editor.
    Centre for Digital Music, Queen Mary, University of London.
    This file copyright 2006 Chris Cannam and QMUL.
    
    This program is free software; you can redistribute it and/or
    modify it under the terms of the GNU General Public License as
    published by the Free Software Foundation; either version 2 of the
    License, or (at your option) any later version.  See the file
    COPYING included with this distribution for more information.
*/

#include "EditableDenseThreeDimensionalModel.h"

#include "base/LogRange.h"

#include <QTextStream>
#include <QStringList>
#include <QMutexLocker>

#include <iostream>

#include <cmath>
#include <cassert>

using std::vector;

#include "system/System.h"

EditableDenseThreeDimensionalModel::EditableDenseThreeDimensionalModel(sv_samplerate_t sampleRate,
                                                                       int resolution,
                                                                       int yBinCount,
                                                                       bool notifyOnAdd) :
    m_startFrame(0),
    m_sampleRate(sampleRate),
    m_resolution(resolution),
    m_yBinCount(yBinCount),
    m_minimum(0.0),
    m_maximum(0.0),
    m_haveExtents(false),
    m_notifyOnAdd(notifyOnAdd),
    m_sinceLastNotifyMin(-1),
    m_sinceLastNotifyMax(-1),
    m_completion(100)
{
}    

bool
EditableDenseThreeDimensionalModel::isOK() const
{
    return true;
}

bool
EditableDenseThreeDimensionalModel::isReady(int *completion) const
{
    if (completion) *completion = getCompletion();
    return true;
}

sv_samplerate_t
EditableDenseThreeDimensionalModel::getSampleRate() const
{
    return m_sampleRate;
}

sv_frame_t
EditableDenseThreeDimensionalModel::getStartFrame() const
{
    return m_startFrame;
}

void
EditableDenseThreeDimensionalModel::setStartFrame(sv_frame_t f)
{
    m_startFrame = f; 
}

sv_frame_t
EditableDenseThreeDimensionalModel::getTrueEndFrame() const
{
    return m_resolution * m_data.size() + (m_resolution - 1);
}

int
EditableDenseThreeDimensionalModel::getResolution() const
{
    return m_resolution;
}

void
EditableDenseThreeDimensionalModel::setResolution(int sz)
{
    m_resolution = sz;
}

int
EditableDenseThreeDimensionalModel::getWidth() const
{
    return int(m_data.size());
}

int
EditableDenseThreeDimensionalModel::getHeight() const
{
    return m_yBinCount;
}

void
EditableDenseThreeDimensionalModel::setHeight(int sz)
{
    m_yBinCount = sz;
}

float
EditableDenseThreeDimensionalModel::getMinimumLevel() const
{
    return m_minimum;
}

void
EditableDenseThreeDimensionalModel::setMinimumLevel(float level)
{
    m_minimum = level;
}

float
EditableDenseThreeDimensionalModel::getMaximumLevel() const
{
    return m_maximum;
}

void
EditableDenseThreeDimensionalModel::setMaximumLevel(float level)
{
    m_maximum = level;
}

EditableDenseThreeDimensionalModel::Column
EditableDenseThreeDimensionalModel::getColumn(int index) const
{
    QMutexLocker locker(&m_mutex);
    if (!in_range_for(m_data, index)) {
        return {};
    }
    Column c = m_data.at(index);
    if (int(c.size()) == m_yBinCount) {
        return c;
    } else {
        Column cc(c);
        cc.resize(m_yBinCount, 0.0);
        return cc;
    }
}

float
EditableDenseThreeDimensionalModel::getValueAt(int index, int n) const
{
    QMutexLocker locker(&m_mutex);
    if (!in_range_for(m_data, index)) {
        return m_minimum;
    }
    const Column &c = m_data.at(index);
    if (!in_range_for(c, n)) {
        return m_minimum;
    }
    return c.at(n);
}

void
EditableDenseThreeDimensionalModel::setColumn(int index,
                                              const Column &values)
{
    bool allChange = false;
    sv_frame_t windowStart = index;
    windowStart *= m_resolution;

    {
        QMutexLocker locker(&m_mutex);

        while (index >= int(m_data.size())) {
            m_data.push_back(Column());
        }

        for (int i = 0; in_range_for(values, i); ++i) {
            float value = values[i];
            if (ISNAN(value) || ISINF(value)) {
                continue;
            }
            if (!m_haveExtents || value < m_minimum) {
                m_minimum = value;
                allChange = true;
            }
            if (!m_haveExtents || value > m_maximum) {
                m_maximum = value;
                allChange = true;
            }
            m_haveExtents = true;
        }

        m_data[index] = values;

        if (allChange) {
            m_sinceLastNotifyMin = -1;
            m_sinceLastNotifyMax = -1;
        } else {
            if (m_sinceLastNotifyMin == -1 ||
                windowStart < m_sinceLastNotifyMin) {
                m_sinceLastNotifyMin = windowStart;
            }
            if (m_sinceLastNotifyMax == -1 ||
                windowStart > m_sinceLastNotifyMax) {
                m_sinceLastNotifyMax = windowStart;
            }
        }
    }

    if (m_notifyOnAdd) {
        if (allChange) {
            emit modelChanged(getId());
        } else {
            emit modelChangedWithin(getId(),
                                    windowStart, windowStart + m_resolution);
        }
    } else {
        if (allChange) {
            emit modelChanged(getId());
        }
    }
}

QString
EditableDenseThreeDimensionalModel::getBinName(int n) const
{
    if (n >= 0 && (int)m_binNames.size() > n) return m_binNames[n];
    else return "";
}

void
EditableDenseThreeDimensionalModel::setBinName(int n, QString name)
{
    while ((int)m_binNames.size() <= n) m_binNames.push_back("");
    m_binNames[n] = name;
    emit modelChanged(getId());
}

void
EditableDenseThreeDimensionalModel::setBinNames(std::vector<QString> names)
{
    m_binNames = names;
    emit modelChanged(getId());
}

bool
EditableDenseThreeDimensionalModel::hasBinValues() const
{
    return !m_binValues.empty();
}

float
EditableDenseThreeDimensionalModel::getBinValue(int n) const
{
    if (n < (int)m_binValues.size()) return m_binValues[n];
    else return 0.f;
}

void
EditableDenseThreeDimensionalModel::setBinValues(std::vector<float> values)
{
    m_binValues = values;
}

QString
EditableDenseThreeDimensionalModel::getBinValueUnit() const
{
    return m_binValueUnit;
}

void
EditableDenseThreeDimensionalModel::setBinValueUnit(QString unit)
{
    m_binValueUnit = unit;
}

bool
EditableDenseThreeDimensionalModel::shouldUseLogValueScale() const
{
    QMutexLocker locker(&m_mutex);

    vector<double> sample;
    vector<int> n;
    
    for (int i = 0; i < 10; ++i) {
        int index = i * 10;
        if (in_range_for(m_data, index)) {
            const Column &c = m_data.at(index);
            while (c.size() > sample.size()) {
                sample.push_back(0.0);
                n.push_back(0);
            }
            for (int j = 0; in_range_for(c, j); ++j) {
                sample[j] += c.at(j);
                ++n[j];
            }
        }
    }

    if (sample.empty()) return false;
    for (decltype(sample)::size_type j = 0; j < sample.size(); ++j) {
        if (n[j]) sample[j] /= n[j];
    }
    
    return LogRange::shouldUseLogScale(sample);
}

void
EditableDenseThreeDimensionalModel::setCompletion(int completion, bool update)
{
    if (m_completion != completion) {
        m_completion = completion;

        if (completion == 100) {

            m_notifyOnAdd = true; // henceforth
            emit modelChanged(getId());

        } else if (!m_notifyOnAdd) {

            if (update &&
                m_sinceLastNotifyMin >= 0 &&
                m_sinceLastNotifyMax >= 0) {
                emit modelChangedWithin(getId(),
                                        m_sinceLastNotifyMin,
                                        m_sinceLastNotifyMax + m_resolution);
                m_sinceLastNotifyMin = m_sinceLastNotifyMax = -1;
            } else {
                emit completionChanged(getId());
            }
        } else {
            emit completionChanged(getId());
        }            
    }
}

int
EditableDenseThreeDimensionalModel::getCompletion() const
{
    return m_completion;
}

QString
EditableDenseThreeDimensionalModel::toDelimitedDataString(QString delimiter,
                                                          DataExportOptions,
                                                          sv_frame_t startFrame,
                                                          sv_frame_t duration) const
{
    QMutexLocker locker(&m_mutex);
    QString s;
    for (int i = 0; in_range_for(m_data, i); ++i) {
        sv_frame_t fr = m_startFrame + i * m_resolution;
        if (fr >= startFrame && fr < startFrame + duration) {
            QStringList list;
            for (int j = 0; in_range_for(m_data.at(i), j); ++j) {
                list << QString("%1").arg(m_data.at(i).at(j));
            }
            s += list.join(delimiter) + "\n";
        }
    }
    return s;
}

void
EditableDenseThreeDimensionalModel::toXml(QTextStream &out,
                                          QString indent,
                                          QString extraAttributes) const
{
    QMutexLocker locker(&m_mutex);

    // For historical reasons we read and write "resolution" as "windowSize".

    // Our dataset doesn't have its own export ID, we just use
    // ours. Actually any model could do that, since datasets aren't
    // in the same id-space as models when re-read

    SVDEBUG << "EditableDenseThreeDimensionalModel::toXml" << endl;

    Model::toXml
        (out, indent,
         QString("type=\"dense\" dimensions=\"3\" windowSize=\"%1\" yBinCount=\"%2\" minimum=\"%3\" maximum=\"%4\" dataset=\"%5\" startFrame=\"%6\" %7")
         .arg(m_resolution)
         .arg(m_yBinCount)
         .arg(m_minimum)
         .arg(m_maximum)
         .arg(getExportId())
         .arg(m_startFrame)
         .arg(extraAttributes));

    out << indent;
    out << QString("<dataset id=\"%1\" dimensions=\"3\" separator=\" \">\n")
        .arg(getExportId());

    for (int i = 0; in_range_for(m_binNames, i); ++i) {
        if (m_binNames[i] != "") {
            out << indent + "  ";
            out << QString("<bin number=\"%1\" name=\"%2\"/>\n")
                .arg(i).arg(m_binNames[i]);
        }
    }

    for (int i = 0; in_range_for(m_data, i); ++i) {
        Column c = getColumn(i);
        out << indent + "  ";
        out << QString("<row n=\"%1\">").arg(i);
        for (int j = 0; in_range_for(c, j); ++j) {
            if (j > 0) out << " ";
            out << c.at(j);
        }
        out << QString("</row>\n");
        out.flush();
    }

    out << indent + "</dataset>\n";
}


