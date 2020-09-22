/* -*- c-basic-offset: 4 indent-tabs-mode: nil -*-  vi:set ts=8 sts=4 sw=4: */

/*
    Sonic Visualiser
    An audio file viewer and annotation editor.
    Centre for Digital Music, Queen Mary, University of London.
    This file copyright 2006-2007 Chris Cannam and QMUL.
    
    This program is free software; you can redistribute it and/or
    modify it under the terms of the GNU General Public License as
    published by the Free Software Foundation; either version 2 of the
    License, or (at your option) any later version.  See the file
    COPYING included with this distribution for more information.
*/

#ifndef SV_LABELLER_H
#define SV_LABELLER_H

#include "base/Selection.h"
#include "base/Event.h"

#include "EventCommands.h"

#include <QObject>

#include <map>
#include <iostream>

class Labeller : public QObject
{
    Q_OBJECT

public:
    enum ValueType {
        ValueNone,
        ValueFromSimpleCounter,
        ValueFromCyclicalCounter,
        ValueFromTwoLevelCounter,
        ValueFromFrameNumber,
        ValueFromRealTime,
        ValueFromDurationFromPrevious,
        ValueFromDurationToNext,
        ValueFromTempoFromPrevious,
        ValueFromTempoToNext,
        ValueFromExistingNeighbour,
        ValueFromLabel
    };

    // uses:
    //
    // 1. when adding points to a time-value model, generate values
    // for those points based on their times or labels or a counter
    //
    // 2. when adding a single point to a time-instant model, generate
    // a label for it based on its time and that of the previous point
    // or a counter
    //
    // 3. when adding a single point to a time-instant model, generate
    // a label for the previous point based on its time and that of
    // the point just added (as tempo is based on time to the next
    // point, not the previous one)
    //
    // 4. re-label a set of points that have already been added to a
    // model
    //
    // 5. generate new labelled points in the gaps between other
    // points (subdivide), or remove them (winnow)

    Labeller(ValueType type = ValueNone) :
        m_type(type),
        m_counter(1),
        m_counter2(1),
        m_cycle(4),
        m_dp(10),
        m_rate(0) { }

    Labeller(const Labeller &l) :
        QObject(),
        m_type(l.m_type),
        m_counter(l.m_counter),
        m_counter2(l.m_counter2),
        m_cycle(l.m_cycle),
        m_dp(l.m_dp),
        m_rate(l.m_rate) { }

    virtual ~Labeller() { }

    typedef std::map<ValueType, QString> TypeNameMap;
    TypeNameMap getTypeNames() const {
        TypeNameMap m;
        m[ValueNone]
            = tr("No numbering");
        m[ValueFromSimpleCounter]
            = tr("Simple counter");
        m[ValueFromCyclicalCounter]
            = tr("Cyclical counter");
        m[ValueFromTwoLevelCounter]
            = tr("Cyclical two-level counter (bar/beat)");
        m[ValueFromFrameNumber]
            = tr("Audio sample frame number");
        m[ValueFromRealTime]
            = tr("Time in seconds");
        m[ValueFromDurationToNext]
            = tr("Duration to the following item");
        m[ValueFromTempoToNext]
            = tr("Tempo (bpm) based on duration to following item");
        m[ValueFromDurationFromPrevious]
            = tr("Duration since the previous item");
        m[ValueFromTempoFromPrevious]
            = tr("Tempo (bpm) based on duration since previous item");
        m[ValueFromExistingNeighbour]
            = tr("Same as the nearest previous item");
        m[ValueFromLabel]
            = tr("Value extracted from the item's label (where possible)");
        return m;
    }

    ValueType getType() const { return m_type; }
    void setType(ValueType type) { m_type = type; }

    int getCounterValue() const { return m_counter; }
    void setCounterValue(int v) { m_counter = v; }

    int getSecondLevelCounterValue() const { return m_counter2; }
    void setSecondLevelCounterValue(int v) { m_counter2 = v; }

    int getCounterCycleSize() const { return m_cycle; }
    void setCounterCycleSize(int s) {
        m_cycle = s;
        m_dp = 1;
        while (s > 0) {
            s /= 10;
            m_dp *= 10;
        }
        if (m_counter > m_cycle) m_counter = 1;
    }

    void setSampleRate(sv_samplerate_t rate) { m_rate = rate; }

    void resetCounters() {
        m_counter = 1;
        m_counter2 = 1;
        m_cycle = 4;
    }

    void incrementCounter() {
        m_counter++;
        if (m_type == ValueFromCyclicalCounter ||
            m_type == ValueFromTwoLevelCounter) {
            if (m_counter > m_cycle) {
                m_counter = 1;
                m_counter2++;
            }
        }
    }

    enum Application {
        AppliesToThisEvent,
        AppliesToPreviousEvent
    };
    typedef std::pair<Application, Event> Relabelling;
    typedef std::pair<Application, Event> Revaluing;

    /** 
     * Return a labelled event based on the given event, previous
     * event if supplied, and internal labeller state. The returned
     * event replaces either the event provided or the previous event,
     * depending on the Application value in the returned pair.
     */
    Relabelling
    label(Event e, const Event *prev = nullptr) {

        QString label = e.getLabel();

        if (m_type == ValueNone) {
            label = "";
        } else if (m_type == ValueFromTwoLevelCounter) {
            label = tr("%1.%2").arg(m_counter2).arg(m_counter);
            incrementCounter();
        } else if (m_type == ValueFromFrameNumber) {
            // avoid going through floating-point value
            label = tr("%1").arg(e.getFrame());
        } else {
            float value = getValueFor(e, prev);
            label = QString("%1").arg(value);
        }

        if (actingOnPrevEvent() && prev) {
            return { AppliesToPreviousEvent, prev->withLabel(label) };
        } else {
            return { AppliesToThisEvent, e.withLabel(label) };
        }
    }

    /** 
     * Return an event with a value following the labelling scheme,
     * based on the given event, previous event if supplied, and
     * internal labeller state. The returned event replaces either the
     * event provided or the previous event, depending on the
     * Application value in the returned pair.
     */
    Revaluing
    revalue(Event e, const Event *prev = nullptr) {

        float value = e.getValue();
        
        if (m_type == ValueFromExistingNeighbour) {
            if (!prev) {
                std::cerr << "ERROR: Labeller::setValue: Previous point required but not provided" << std::endl;
            } else {
                return { AppliesToThisEvent, e.withValue(prev->getValue()) };
            }
        } else {
            value = getValueFor(e, prev);
        }

        if (actingOnPrevEvent() && prev) {
            return { AppliesToPreviousEvent, prev->withValue(value) };
        } else {
            return { AppliesToThisEvent, e.withValue(value) };
        }
    }
    
    /**
     * Relabel all events in the given event vector that lie within
     * the given multi-selection, according to the labelling
     * properties of this labeller.  Return a command that has been
     * executed but not yet added to the history.  The id must be that
     * of a type that can be retrieved from the AnyById store and
     * dynamic_cast to EventEditable.
     */
    Command *labelAll(int editableId,
                      MultiSelection *ms,
                      const EventVector &allEvents) {

        auto command = new ChangeEventsCommand
            (editableId, tr("Label Points"));

        Event prev;
        bool havePrev = false;

        for (auto p: allEvents) {

            if (ms) {
                Selection s(ms->getContainingSelection(p.getFrame(), false));
                if (!s.contains(p.getFrame())) {
                    prev = p;
                    havePrev = true;
                    continue;
                }
            }

            auto labelling = label(p, havePrev ? &prev : nullptr);

            if (labelling.first == AppliesToThisEvent) {
                command->remove(p);
            } else {
                command->remove(prev);
            }

            command->add(labelling.second);

            prev = p;
            havePrev = true;
        }

        return command->finish();
    }

    /**
     * For each event in the given event vector (except the last), if
     * that event lies within the given multi-selection, add n-1 new
     * events at equally spaced intervals between it and the following
     * event.  Return a command that has been executed but not yet
     * added to the history.  The id must be that of a type that can
     * be retrieved from the AnyById store and dynamic_cast to
     * EventEditable.
     */
    Command *subdivide(int editableId,
                       MultiSelection *ms,
                       const EventVector &allEvents,
                       int n) {

        auto command = new ChangeEventsCommand
            (editableId, tr("Subdivide Points"));

        for (auto i = allEvents.begin(); i != allEvents.end(); ++i) {

            auto j = i;
            // require a "next point" even if it's not in selection
            if (++j == allEvents.end()) {
                break;
            }

            if (ms) {
                Selection s(ms->getContainingSelection(i->getFrame(), false));
                if (!s.contains(i->getFrame())) {
                    continue;
                }
            }

            Event p(*i);
            Event nextP(*j);

            // n is the number of subdivisions, so we add n-1 new
            // points equally spaced between p and nextP

            for (int m = 1; m < n; ++m) {
                sv_frame_t f = p.getFrame() +
                    (m * (nextP.getFrame() - p.getFrame())) / n;
                Event newPoint = p
                    .withFrame(f)
                    .withLabel(tr("%1.%2").arg(p.getLabel()).arg(m+1));
                command->add(newPoint);
            }
        }

        return command->finish();
    }

    /**
     * The opposite of subdivide. Given an event vector, a
     * multi-selection, and a number n, remove all but every nth event
     * from the vector within the extents of the multi-selection.
     * Return a command that has been executed but not yet added to
     * the history. The id must be that of a type that can be
     * retrieved from the AnyById store and dynamic_cast to
     * EventEditable.
     */
    Command *winnow(int editableId,
                    MultiSelection *ms,
                    const EventVector &allEvents,
                    int n) {

        auto command = new ChangeEventsCommand
            (editableId, tr("Winnow Points"));

        int counter = 0;
        
        for (auto p: allEvents) {

            if (ms) {
                Selection s(ms->getContainingSelection(p.getFrame(), false));
                if (!s.contains(p.getFrame())) {
                    counter = 0;
                    continue;
                }
            }

            ++counter;

            if (counter == n+1) counter = 1;
            if (counter == 1) {
                // this is an Nth instant, don't remove it
                continue;
            }
            
            command->remove(p);
        }

        return command->finish();
    }

    bool requiresPrevPoint() const {
        return (m_type == ValueFromDurationFromPrevious ||
                m_type == ValueFromDurationToNext ||
                m_type == ValueFromTempoFromPrevious ||
                m_type == ValueFromDurationToNext);
    }

    bool actingOnPrevEvent() const {
        return (m_type == ValueFromDurationToNext ||
                m_type == ValueFromTempoToNext);
    }

protected:
    float getValueFor(Event p, const Event *prev) {
        
        float value = 0.f;

        switch (m_type) {

        case ValueNone:
            value = 0;
            break;

        case ValueFromSimpleCounter:
        case ValueFromCyclicalCounter:
            value = float(m_counter);
            incrementCounter();
            break;

        case ValueFromTwoLevelCounter:
            value = float(m_counter2 + double(m_counter) / double(m_dp));
            incrementCounter();
            break;

        case ValueFromFrameNumber:
            value = float(p.getFrame());
            break;
            
        case ValueFromRealTime: 
            if (m_rate == 0.0) {
                std::cerr << "ERROR: Labeller::getValueFor: Real-time conversion required, but no sample rate set" << std::endl;
            } else {
                value = float(double(p.getFrame()) / m_rate);
            }
            break;

        case ValueFromDurationToNext:
        case ValueFromTempoToNext:
        case ValueFromDurationFromPrevious:
        case ValueFromTempoFromPrevious:
            if (m_rate == 0.0) {
                std::cerr << "ERROR: Labeller::getValueFor: Real-time conversion required, but no sample rate set" << std::endl;
            } else if (!prev) {
                std::cerr << "ERROR: Labeller::getValueFor: Time difference required, but only one point provided" << std::endl;
            } else {
                sv_frame_t f0 = prev->getFrame(), f1 = p.getFrame();
                if (m_type == ValueFromDurationToNext ||
                    m_type == ValueFromDurationFromPrevious) {
                    value = float(double(f1 - f0) / m_rate);
                } else {
                    if (f1 > f0) {
                        value = float((60.0 * m_rate) / double(f1 - f0));
                    }
                }
            }
            break;

        case ValueFromExistingNeighbour:
            // need to deal with this in the calling function, as this
            // function must handle points that don't have values to
            // read from
            break;

        case ValueFromLabel:
            if (p.getLabel() != "") {
                // more forgiving than QString::toFloat()
                value = float(atof(p.getLabel().toLocal8Bit()));
            } else {
                value = 0.f;
            }
            break;
        }

        return value;
    }

    ValueType m_type;
    int m_counter;
    int m_counter2;
    int m_cycle;
    int m_dp;
    sv_samplerate_t m_rate;
};

#endif
