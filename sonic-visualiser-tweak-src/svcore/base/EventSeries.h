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

#ifndef SV_EVENT_SERIES_H
#define SV_EVENT_SERIES_H

#include "Event.h"
#include "XmlExportable.h"

#include <set>
#include <functional>

#include <QMutex>

//#define DEBUG_EVENT_SERIES 1

/**
 * Container storing a series of events, with or without durations,
 * and supporting the ability to query which events are active at a
 * given frame or within a span of frames.
 *
 * To that end, in addition to the series of events, it stores a
 * series of "seams", which are frame positions at which the set of
 * simultaneous events changes (i.e. an event of non-zero duration
 * starts or ends) associated with a set of the events that are active
 * at or from that position. These are updated when an event is added
 * or removed.
 *
 * This class is highly optimised for inserting events in increasing
 * order of start frame. Inserting (or deleting) events in the middle
 * does work, and should be acceptable in interactive use, but it is
 * very slow in bulk.
 *
 * EventSeries is thread-safe.
 */
class EventSeries : public XmlExportable
{
public:
    EventSeries() : m_finalDurationlessEventFrame(0) { }
    ~EventSeries() =default;

    EventSeries(const EventSeries &);

    EventSeries &operator=(const EventSeries &);
    EventSeries &operator=(EventSeries &&);
    
    bool operator==(const EventSeries &other) const;

    static EventSeries fromEvents(const EventVector &ee);
    
    void clear();
    void add(const Event &e);
    void remove(const Event &e);
    bool contains(const Event &e) const;
    bool isEmpty() const;
    int count() const;

    /**
     * Return the frame of the first event in the series. If there are
     * no events, return 0.
     */
    sv_frame_t getStartFrame() const;

    /**
     * Return the frame plus duration of the event in the series that
     * ends last. If there are no events, return 0.
     */
    sv_frame_t getEndFrame() const;
    
    /**
     * Retrieve all events any part of which falls within the range in
     * frames defined by the given frame f and duration d.
     *
     * - An event without duration is spanned by the range if its own
     * frame is greater than or equal to f and less than f + d.
     * 
     * - An event with duration is spanned by the range if its start
     * frame is less than f + d and its start frame plus its duration
     * is greater than f.
     * 
     * Note: Passing a duration of zero is seldom useful here; you
     * probably want getEventsCovering instead. getEventsSpanning(f,
     * 0) is not equivalent to getEventsCovering(f). The latter
     * includes durationless events at f and events starting at f,
     * both of which are excluded from the former.
     */
    EventVector getEventsSpanning(sv_frame_t frame,
                                  sv_frame_t duration) const;

    /**
     * Retrieve all events that cover the given frame. An event without
     * duration covers a frame if its own frame is equal to it. An event
     * with duration covers a frame if its start frame is less than or
     * equal to it and its end frame (start + duration) is greater
     * than it.
     */
    EventVector getEventsCovering(sv_frame_t frame) const;

    /**
     * Retrieve all events falling wholly within the range in frames
     * defined by the given frame f and duration d.
     *
     * - An event without duration is within the range if its own
     * frame is greater than or equal to f and less than f + d.
     * 
     * - An event with duration is within the range if its start frame
     * is greater than or equal to f and its start frame plus its
     * duration is less than or equal to f + d.
     *
     * If overspill is greater than zero, also include that number of
     * additional events (where they exist) both before and after the
     * edges of the range.
     */
    EventVector getEventsWithin(sv_frame_t frame,
                                sv_frame_t duration,
                                int overspill = 0) const;

    /**
     * Retrieve all events starting within the range in frames defined
     * by the given frame f and duration d. An event (regardless of
     * whether it has duration or not) starts within the range if its
     * start frame is greater than or equal to f and less than f + d.
     */
    EventVector getEventsStartingWithin(sv_frame_t frame,
                                        sv_frame_t duration) const;

    /**
     * Retrieve all events starting at exactly the given frame.
     */
    EventVector getEventsStartingAt(sv_frame_t frame) const {
        return getEventsStartingWithin(frame, 1);
    }

    /**
     * Retrieve all events, in their natural order.
     */
    EventVector getAllEvents() const;
    
    /**
     * If e is in the series and is not the first event in it, set
     * preceding to the event immediate preceding it according to the
     * standard event ordering and return true. Otherwise leave
     * preceding unchanged and return false.
     *
     * If there are multiple events identical to e in the series,
     * assume that the event passed in is the first one (i.e. never
     * set preceding equal to e).
     *
     * It is acceptable for preceding to alias e when this is called.
     */
    bool getEventPreceding(const Event &e, Event &preceding) const;

    /**
     * If e is in the series and is not the final event in it, set
     * following to the event immediate following it according to the
     * standard event ordering and return true. Otherwise leave
     * following unchanged and return false.
     *
     * If there are multiple events identical to e in the series,
     * assume that the event passed in is the last one (i.e. never set
     * following equal to e).
     *
     * It is acceptable for following to alias e when this is called.
     */
    bool getEventFollowing(const Event &e, Event &following) const;

    enum Direction {
        Forward,
        Backward
    };

    /**
     * Return the first event for which the given predicate returns
     * true, searching events with start frames increasingly far from
     * the given frame in the given direction. If the direction is
     * Forward then the search includes events starting at the given
     * frame, otherwise it does not.
     */
    bool getNearestEventMatching(sv_frame_t startSearchAt,
                                 std::function<bool(const Event &)> predicate,
                                 Direction direction,
                                 Event &found) const;
    
    /**
     * Return the event at the given numerical index in the series,
     * where 0 = the first event and count()-1 = the last.
     */
    Event getEventByIndex(int index) const;

    /**
     * Return the index of the first event in the series that does not
     * compare inferior to the given event. If there is no such event,
     * return count().
     */
    int getIndexForEvent(const Event &e) const;

    /**
     * Emit to XML as a dataset element.
     */
    void toXml(QTextStream &out,
               QString indent,
               QString extraAttributes) const override;

    /**
     * Emit to XML as a dataset element.
     */
    void toXml(QTextStream &out,
               QString indent,
               QString extraAttributes,
               Event::ExportNameOptions) const;

    /**
     * Emit events starting within the given range to a delimited
     * (e.g. comma-separated) data format.
     */
    QString toDelimitedDataString(QString delimiter,
                                  DataExportOptions options,
                                  sv_frame_t startFrame,
                                  sv_frame_t duration,
                                  sv_samplerate_t sampleRate,
                                  sv_frame_t resolution,
                                  Event fillEvent) const;
    
private:
    mutable QMutex m_mutex;

    EventSeries(const EventSeries &other, const QMutexLocker &);
    
    /**
     * This vector contains all events in the series, in the normal
     * sort order. For backward compatibility we must support series
     * containing multiple instances of identical events, so
     * consecutive events in this vector will not always be distinct.
     * The vector is used in preference to a multiset or map<Event,
     * int> in order to allow indexing by "row number" as well as by
     * properties such as frame.
     * 
     * Because events are immutable, we do not have to worry about the
     * order changing once an event is inserted - we only add or
     * delete them.
     */
    typedef std::vector<Event> Events;
    Events m_events;
    
    /**
     * The FrameEventMap maps from frame number to a set of events. In
     * the seam map this is used to represent the events that are
     * active at that frame, either because they begin at that frame
     * or because they are continuing from an earlier frame. There is
     * an entry here for each frame at which an event starts or ends,
     * with the event appearing in all entries from its start time
     * onward and disappearing again at its end frame.
     *
     * Only events with duration appear in this map; point events
     * appear only in m_events. Note that unlike m_events, we only
     * store one instance of each event here, even if we hold many -
     * we refer back to m_events when we need to know how many
     * identical copies of a given event we have.
     */
    typedef std::map<sv_frame_t, std::vector<Event>> FrameEventMap;
    FrameEventMap m_seams;

    /**
     * The frame of the last durationless event we have in the series.
     * This is to support a fast-ish getEndFrame(): we can easily keep
     * this up-to-date when events are added or removed, and we can
     * easily find the end frame of the last with-duration event from
     * the seam map, but it's not so easy to continuously update an
     * overall end frame or to find the last frame of all events
     * without this.
     */
    sv_frame_t m_finalDurationlessEventFrame;
    
    /** 
     * Create a seam at the given frame, copying from the prior seam
     * if there is one. If a seam already exists at the given frame,
     * leave it untouched.
     *
     * Call with m_mutex locked.
     */
    void createSeam(sv_frame_t frame) {
        auto itr = m_seams.lower_bound(frame);
        if (itr == m_seams.end() || itr->first > frame) {
            if (itr != m_seams.begin()) {
                --itr;
            }
        }
        if (itr == m_seams.end()) {
            m_seams[frame] = {};
        } else if (itr->first < frame) {
            m_seams[frame] = itr->second;
        } else if (itr->first > frame) { // itr must be begin()
            m_seams[frame] = {};
        }
    }

    /** 
     * Return true if the two seam map entries contain the same set of
     * events.
     *
     * Precondition: no duplicates, i.e. no event appears more than
     * once in s1 or more than once in s2.
     *
     * Call with m_mutex locked.
     */
    bool seamsEqual(const std::vector<Event> &s1,
                    const std::vector<Event> &s2) const {
        
        if (s1.size() != s2.size()) {
            return false;
        }

#ifdef DEBUG_EVENT_SERIES
        for (int i = 0; in_range_for(s1, i); ++i) {
            for (int j = i + 1; in_range_for(s1, j); ++j) {
                if (s1[i] == s1[j] || s2[i] == s2[j]) {
                    throw std::runtime_error
                        ("debug error: duplicate event in s1 or s2");
                }
            }
        }
#endif

        std::set<Event> ee;
        for (const auto &e: s1) {
            ee.insert(e);
        }
        for (const auto &e: s2) {
            if (ee.find(e) == ee.end()) {
                return false;
            }
        }
        return true;
    }

#ifdef DEBUG_EVENT_SERIES
    void dumpEvents() const {
        std::cerr << "EVENTS (" << m_events.size() << ") [" << std::endl;
        for (const auto &i: m_events) {
            std::cerr << "  " << i.toXmlString();
        }
        std::cerr << "]" << std::endl;
    }
    
    void dumpSeams() const {
        std::cerr << "SEAMS (" << m_seams.size() << ") [" << std::endl;
        for (const auto &s: m_seams) {
            std::cerr << "  " << s.first << " -> {" << std::endl;
            for (const auto &p: s.second) {
                std::cerr << p.toXmlString("    ");
            }
            std::cerr << "  }" << std::endl;
        }
        std::cerr << "]" << std::endl;
    }
#endif
};

#endif
