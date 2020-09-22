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

#ifndef SV_EVENT_H
#define SV_EVENT_H

#include "BaseTypes.h"
#include "NoteData.h"
#include "XmlExportable.h"
#include "DataExportOptions.h"

#include <vector>
#include <stdexcept>

#include <QString>

#if (QT_VERSION < QT_VERSION_CHECK(5, 3, 0))
#ifdef __GNUC__
#pragma GCC diagnostic ignored "-Wunused-function"
#endif
static uint qHash(float key, uint seed = 0) {
    uint h = seed;
    const uchar *p = reinterpret_cast<const uchar *>(&key);
    for (size_t i = 0; i < sizeof(key); ++i) {
        h = 31 * h + p[i];
    }
    return h;
}
#endif

/**
 * An immutable(-ish) type used for point and event representation in
 * sparse models, as well as for interchange within the clipboard. An
 * event always has a frame and (possibly empty) label, and optionally
 * has numerical value, level, duration in frames, and a mapped
 * reference frame. Event has an operator< defining a total ordering,
 * by frame first and then by the other properties.
 * 
 * Event is based on the Clipboard::Point type up to SV v3.2.1 and is
 * intended also to replace the custom point types previously found in
 * sparse models.
 */
class Event
{
public:
    Event() :
        m_haveValue(false), m_haveLevel(false),
        m_haveDuration(false), m_haveReferenceFrame(false),
        m_value(0.f), m_level(0.f), m_frame(0),
        m_duration(0), m_referenceFrame(0), m_label() { }
    
    Event(sv_frame_t frame) :
        m_haveValue(false), m_haveLevel(false),
        m_haveDuration(false), m_haveReferenceFrame(false),
        m_value(0.f), m_level(0.f), m_frame(frame),
        m_duration(0), m_referenceFrame(0), m_label() { }
        
    Event(sv_frame_t frame, QString label) :
        m_haveValue(false), m_haveLevel(false), 
        m_haveDuration(false), m_haveReferenceFrame(false),
        m_value(0.f), m_level(0.f), m_frame(frame),
        m_duration(0), m_referenceFrame(0), m_label(label) { }
        
    Event(sv_frame_t frame, float value, QString label) :
        m_haveValue(true), m_haveLevel(false), 
        m_haveDuration(false), m_haveReferenceFrame(false),
        m_value(value), m_level(0.f), m_frame(frame),
        m_duration(0), m_referenceFrame(0), m_label(label) { }
        
    Event(sv_frame_t frame, float value, sv_frame_t duration, QString label) :
        m_haveValue(true), m_haveLevel(false), 
        m_haveDuration(true), m_haveReferenceFrame(false),
        m_value(value), m_level(0.f), m_frame(frame),
        m_duration(duration), m_referenceFrame(0), m_label(label) {
        if (m_duration < 0) throw std::logic_error("duration must be >= 0");
    }
        
    Event(sv_frame_t frame, float value, sv_frame_t duration,
          float level, QString label) :
        m_haveValue(true), m_haveLevel(true), 
        m_haveDuration(true), m_haveReferenceFrame(false),
        m_value(value), m_level(level), m_frame(frame),
        m_duration(duration), m_referenceFrame(0), m_label(label) {
        if (m_duration < 0) throw std::logic_error("duration must be >= 0");
    }

    Event(const Event &event) =default;

    // We would ideally like Event to be immutable - but we have to
    // have these because otherwise we can't put Events in vectors
    // etc. Let's call it conceptually immutable
    Event &operator=(const Event &event) =default;
    Event &operator=(Event &&event) =default;
    
    sv_frame_t getFrame() const { return m_frame; }

    Event withFrame(sv_frame_t frame) const {
        Event p(*this);
        p.m_frame = frame;
        return p;
    }
    
    bool hasValue() const { return m_haveValue; }
    float getValue() const { return m_haveValue ? m_value : 0.f; }
    
    Event withValue(float value) const {
        Event p(*this);
        p.m_haveValue = true;
        p.m_value = value;
        return p;
    }
    Event withoutValue() const {
        Event p(*this);
        p.m_haveValue = false;
        p.m_value = 0.f;
        return p;
    }
    
    bool hasDuration() const { return m_haveDuration; }
    sv_frame_t getDuration() const { return m_haveDuration ? m_duration : 0; }

    Event withDuration(sv_frame_t duration) const {
        Event p(*this);
        p.m_duration = duration;
        p.m_haveDuration = true;
        if (duration < 0) throw std::logic_error("duration must be >= 0");
        return p;
    }
    Event withoutDuration() const {
        Event p(*this);
        p.m_haveDuration = false;
        p.m_duration = 0;
        return p;
    }

    bool hasLabel() const { return m_label != QString(); }
    QString getLabel() const { return m_label; }

    Event withLabel(QString label) const {
        Event p(*this);
        p.m_label = label;
        return p;
    }

    bool hasUri() const { return m_uri != QString(); }
    QString getURI() const { return m_uri; }

    Event withURI(QString uri) const {
        Event p(*this);
        p.m_uri = uri;
        return p;
    }
    
    bool hasLevel() const { return m_haveLevel; }
    float getLevel() const { return m_haveLevel ? m_level : 0.f; }

    Event withLevel(float level) const {
        Event p(*this);
        p.m_haveLevel = true;
        p.m_level = level;
        return p;
    }
    Event withoutLevel() const {
        Event p(*this);
        p.m_haveLevel = false;
        p.m_level = 0.f;
        return p;
    }
    
    bool hasReferenceFrame() const { return m_haveReferenceFrame; }
    sv_frame_t getReferenceFrame() const {
        return m_haveReferenceFrame ? m_referenceFrame : m_frame;
    }
        
    bool referenceFrameDiffers() const { // from event frame
        return m_haveReferenceFrame && (m_referenceFrame != m_frame);
    }
    
    Event withReferenceFrame(sv_frame_t frame) const {
        Event p(*this);
        p.m_haveReferenceFrame = true;
        p.m_referenceFrame = frame;
        return p;
    }
    Event withoutReferenceFrame() const {
        Event p(*this);
        p.m_haveReferenceFrame = false;
        p.m_referenceFrame = 0;
        return p;
    }

    bool operator==(const Event &p) const {

        if (m_frame != p.m_frame) return false;

        if (m_haveDuration != p.m_haveDuration) return false;
        if (m_haveDuration && (m_duration != p.m_duration)) return false;

        if (m_haveValue != p.m_haveValue) return false;
        if (m_haveValue && (m_value != p.m_value)) return false;

        if (m_haveLevel != p.m_haveLevel) return false;
        if (m_haveLevel && (m_level != p.m_level)) return false;

        if (m_haveReferenceFrame != p.m_haveReferenceFrame) return false;
        if (m_haveReferenceFrame &&
            (m_referenceFrame != p.m_referenceFrame)) return false;
        
        if (m_label != p.m_label) return false;
        if (m_uri != p.m_uri) return false;
        
        return true;
    }

    bool operator!=(const Event &p) const {
        return !operator==(p);
    }

    bool operator<(const Event &p) const {

        if (m_frame != p.m_frame) {
            return m_frame < p.m_frame;
        }

        // events without a property sort before events with that property

        if (m_haveDuration != p.m_haveDuration) {
            return !m_haveDuration;
        }
        if (m_haveDuration && (m_duration != p.m_duration)) {
            return m_duration < p.m_duration;
        }

        if (m_haveValue != p.m_haveValue) {
            return !m_haveValue;
        }
        if (m_haveValue && (m_value != p.m_value)) {
            return m_value < p.m_value;
        }
        
        if (m_haveLevel != p.m_haveLevel) {
            return !m_haveLevel;
        }
        if (m_haveLevel && (m_level != p.m_level)) {
            return m_level < p.m_level;
        }

        if (m_haveReferenceFrame != p.m_haveReferenceFrame) {
            return !m_haveReferenceFrame;
        }
        if (m_haveReferenceFrame && (m_referenceFrame != p.m_referenceFrame)) {
            return m_referenceFrame < p.m_referenceFrame;
        }
        
        if (m_label != p.m_label) {
            return m_label < p.m_label;
        }
        return m_uri < p.m_uri;
    }

    struct ExportNameOptions {

        ExportNameOptions() :
            valueAttributeName("value"),
            levelAttributeName("level"),
            uriAttributeName("uri") { }

        QString valueAttributeName;
        QString levelAttributeName;
        QString uriAttributeName;
    };
    
    void toXml(QTextStream &stream,
               QString indent = "",
               QString extraAttributes = "",
               ExportNameOptions opts = ExportNameOptions()) const {

        // For I/O purposes these are points, not events
        stream << indent << QString("<point frame=\"%1\" ").arg(m_frame);
        if (m_haveValue) {
            stream << QString("%1=\"%2\" ")
                .arg(opts.valueAttributeName).arg(m_value);
        }
        if (m_haveDuration) {
            stream << QString("duration=\"%1\" ").arg(m_duration);
        }
        if (m_haveLevel) {
            stream << QString("%1=\"%2\" ")
                .arg(opts.levelAttributeName)
                .arg(m_level);
        }
        if (m_haveReferenceFrame) {
            stream << QString("referenceFrame=\"%1\" ")
                .arg(m_referenceFrame);
        }

        stream << QString("label=\"%1\" ")
            .arg(XmlExportable::encodeEntities(m_label));
        
        if (m_uri != QString()) {
            stream << QString("%1=\"%2\" ")
                .arg(opts.uriAttributeName)
                .arg(XmlExportable::encodeEntities(m_uri));
        }
        stream << extraAttributes << "/>\n";
    }

    QString toXmlString(QString indent = "",
                        QString extraAttributes = "") const {
        QString s;
        QTextStream out(&s);
        toXml(out, indent, extraAttributes);
        out.flush();
        return s;
    }

    NoteData toNoteData(sv_samplerate_t sampleRate,
                        bool valueIsMidiPitch) const {

        sv_frame_t duration;
        if (m_haveDuration && m_duration > 0) {
            duration = m_duration;
        } else {
            duration = sv_frame_t(sampleRate / 6); // arbitrary short duration
        }

        int midiPitch;
        float frequency = 0.f;
        if (m_haveValue) {
            if (valueIsMidiPitch) {
                midiPitch = int(roundf(m_value));
            } else {
                frequency = m_value;
                midiPitch = Pitch::getPitchForFrequency(frequency);
            }
        } else {
            midiPitch = 64;
            valueIsMidiPitch = true;
        }

        int velocity = 100;
        if (m_haveLevel) {
            if (m_level > 0.f && m_level <= 1.f) {
                velocity = int(roundf(m_level * 127.f));
            }
        }

        NoteData n(m_frame, duration, midiPitch, velocity);
        n.isMidiPitchQuantized = valueIsMidiPitch;
        if (!valueIsMidiPitch) {
            n.frequency = frequency;
        }

        return n;
    }

    QString toDelimitedDataString(QString delimiter,
                                  DataExportOptions opts,
                                  sv_samplerate_t sampleRate) const {
        QStringList list;

        list << RealTime::frame2RealTime(m_frame, sampleRate)
            .toString().c_str();
        
        if (m_haveValue) {
            list << QString("%1").arg(m_value);
        }
        
        if (m_haveDuration) {
            list << RealTime::frame2RealTime(m_duration, sampleRate)
                .toString().c_str();
        }
        
        if (m_haveLevel) {
            if (!(opts & DataExportOmitLevels)) {
                list << QString("%1").arg(m_level);
            }
        }

        // Put URI before label, to preserve the ordering previously
        // used in the custom Image model exporter. We shouldn't
        // change the column ordering unless (until?) we provide a
        // facility for the user to customise it
        if (m_uri != "") list << m_uri;
        if (m_label != "") list << m_label;
        
        return list.join(delimiter);
    }
    
    uint hash(uint seed = 0) const {
        uint h = qHash(m_label, seed);
        if (m_haveValue) h ^= qHash(m_value);
        if (m_haveLevel) h ^= qHash(m_level);
        h ^= qHash(m_frame);
        if (m_haveDuration) h ^= qHash(m_duration);
        if (m_haveReferenceFrame) h ^= qHash(m_referenceFrame);
        h ^= qHash(m_uri);
        return h;
    }
    
private:
    // The order of fields here is chosen to minimise overall size of struct.
    // We potentially store very many of these objects.
    // If you change something, check what difference it makes to packing.
    bool m_haveValue : 1;
    bool m_haveLevel : 1;
    bool m_haveDuration : 1;
    bool m_haveReferenceFrame : 1;
    float m_value;
    float m_level;
    sv_frame_t m_frame;
    sv_frame_t m_duration;
    sv_frame_t m_referenceFrame;
    QString m_label;
    QString m_uri;
};

inline uint qHash(const Event &e, uint seed = 0) {
    return e.hash(seed);
}

typedef std::vector<Event> EventVector;

#endif
