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

#ifndef SV_TEXT_MODEL_H
#define SV_TEXT_MODEL_H

#include "EventCommands.h"
#include "TabularModel.h"
#include "Model.h"
#include "DeferredNotifier.h"

#include "base/EventSeries.h"
#include "base/XmlExportable.h"
#include "base/RealTime.h"

#include "system/System.h"

#include <QStringList>

/**
 * A model representing casual textual annotations. A piece of text
 * has a given time and y-value in the [0,1) range (indicative of
 * height on the window).
 */
class TextModel : public Model,
                  public TabularModel,
                  public EventEditable
{
    Q_OBJECT
    
public:
    TextModel(sv_samplerate_t sampleRate,
              int resolution,
              bool notifyOnAdd = true) :
        m_sampleRate(sampleRate),
        m_resolution(resolution),
        m_notifier(this,
                   getId(),
                   notifyOnAdd ?
                   DeferredNotifier::NOTIFY_ALWAYS :
                   DeferredNotifier::NOTIFY_DEFERRED),
        m_completion(100) {
    }

    QString getTypeName() const override { return tr("Text"); }
    bool isSparse() const override { return true; }
    bool isOK() const override { return true; }

    sv_frame_t getStartFrame() const override {
        return m_events.getStartFrame();
    }
    sv_frame_t getTrueEndFrame() const override {
        if (m_events.isEmpty()) return 0;
        sv_frame_t e = m_events.getEndFrame() + 1;
        if (e % m_resolution == 0) return e;
        else return (e / m_resolution + 1) * m_resolution;
    }
    
    sv_samplerate_t getSampleRate() const override { return m_sampleRate; }
    int getResolution() const { return m_resolution; }
    
    int getCompletion() const override { return m_completion; }

    void setCompletion(int completion, bool update = true) {
        
        {   QMutexLocker locker(&m_mutex);
            if (m_completion == completion) return;
            m_completion = completion;
        }

        if (update) {
            m_notifier.makeDeferredNotifications();
        }
        
        emit completionChanged(getId());

        if (completion == 100) {
            // henceforth:
            m_notifier.switchMode(DeferredNotifier::NOTIFY_ALWAYS);
            emit modelChanged(getId());
        }
    }
    
    /**
     * Query methods.
     */

    int getEventCount() const {
        return m_events.count();
    }
    bool isEmpty() const {
        return m_events.isEmpty();
    }
    bool containsEvent(const Event &e) const {
        return m_events.contains(e);
    }
    EventVector getAllEvents() const {
        return m_events.getAllEvents();
    }
    EventVector getEventsSpanning(sv_frame_t f, sv_frame_t duration) const {
        return m_events.getEventsSpanning(f, duration);
    }
    EventVector getEventsCovering(sv_frame_t f) const {
        return m_events.getEventsCovering(f);
    }
    EventVector getEventsWithin(sv_frame_t f, sv_frame_t duration,
                                int overspill = 0) const {
        return m_events.getEventsWithin(f, duration, overspill);
    }
    EventVector getEventsStartingWithin(sv_frame_t f, sv_frame_t duration) const {
        return m_events.getEventsStartingWithin(f, duration);
    }
    EventVector getEventsStartingAt(sv_frame_t f) const {
        return m_events.getEventsStartingAt(f);
    }
    bool getNearestEventMatching(sv_frame_t startSearchAt,
                                 std::function<bool(Event)> predicate,
                                 EventSeries::Direction direction,
                                 Event &found) const {
        return m_events.getNearestEventMatching
            (startSearchAt, predicate, direction, found);
    }

    /**
     * Editing methods.
     */
    void add(Event e) override {

        {   QMutexLocker locker(&m_mutex);
            m_events.add(e.withoutDuration().withoutLevel());
        }
        
        m_notifier.update(e.getFrame(), m_resolution);
    }
    
    void remove(Event e) override {
        {   QMutexLocker locker(&m_mutex);
            m_events.remove(e);
        }
        emit modelChangedWithin(getId(),
                                e.getFrame(), e.getFrame() + m_resolution);
    }

    /**
     * TabularModel methods.  
     */
    
    int getRowCount() const override {
        return m_events.count();
    }

    int getColumnCount() const override {
        return 4;
    }

    bool isColumnTimeValue(int column) const override {
        return (column < 2);
    }

    sv_frame_t getFrameForRow(int row) const override {
        if (row < 0 || row >= m_events.count()) {
            return 0;
        }
        Event e = m_events.getEventByIndex(row);
        return e.getFrame();
    }

    int getRowForFrame(sv_frame_t frame) const override {
        return m_events.getIndexForEvent(Event(frame));
    }
    
    QString getHeading(int column) const override {
        switch (column) {
        case 0: return tr("Time");
        case 1: return tr("Frame");
        case 2: return tr("Height");
        case 3: return tr("Label");
        default: return tr("Unknown");
        }
    }

    SortType getSortType(int column) const override {
        if (column == 3) return SortAlphabetical;
        return SortNumeric;
    }

    QVariant getData(int row, int column, int role) const override {
        
        if (row < 0 || row >= m_events.count()) {
            return QVariant();
        }

        Event e = m_events.getEventByIndex(row);

        switch (column) {
        case 0: return adaptFrameForRole(e.getFrame(), getSampleRate(), role);
        case 1: return int(e.getFrame());
        case 2: return e.getValue();
        case 3: return e.getLabel();
        default: return QVariant();
        }
    }

    Command *getSetDataCommand(int row, int column, const QVariant &value, int role) override {
        
        if (row < 0 || row >= m_events.count()) return nullptr;
        if (role != Qt::EditRole) return nullptr;

        Event e0 = m_events.getEventByIndex(row);
        Event e1;

        switch (column) {
        case 0: e1 = e0.withFrame(sv_frame_t(round(value.toDouble() *
                                                   getSampleRate()))); break;
        case 1: e1 = e0.withFrame(value.toInt()); break;
        case 2: e1 = e0.withValue(float(value.toDouble())); break;
        case 3: e1 = e0.withLabel(value.toString()); break;
        }

        auto command = new ChangeEventsCommand(getId().untyped, tr("Edit Data"));
        command->remove(e0);
        command->add(e1);
        return command->finish();
    }

    bool isEditable() const override { return true; }

    Command *getInsertRowCommand(int row) override {
        if (row < 0 || row >= m_events.count()) return nullptr;
        auto command = new ChangeEventsCommand(getId().untyped,
                                               tr("Add Label"));
        Event e = m_events.getEventByIndex(row);
        command->add(e);
        return command->finish();
    }

    Command *getRemoveRowCommand(int row) override {
        if (row < 0 || row >= m_events.count()) return nullptr;
        auto command = new ChangeEventsCommand(getId().untyped,
                                               tr("Delete Label"));
        Event e = m_events.getEventByIndex(row);
        command->remove(e);
        return command->finish();
    }
    
    /**
     * XmlExportable methods.
     */
    void toXml(QTextStream &out,
               QString indent = "",
               QString extraAttributes = "") const override {

        Model::toXml
            (out,
             indent,
             QString("type=\"sparse\" dimensions=\"2\" resolution=\"%1\" "
                     "notifyOnAdd=\"%2\" dataset=\"%3\" subtype=\"text\" %4")
             .arg(m_resolution)
             .arg("true") // always true after model reaches 100% -
                          // subsequent events are always notified
             .arg(m_events.getExportId())
             .arg(extraAttributes));

        Event::ExportNameOptions options;
        options.valueAttributeName = "height";
        
        m_events.toXml(out, indent, QString("dimensions=\"2\""), options);
    }

    QString toDelimitedDataString(QString delimiter,
                                  DataExportOptions options,
                                  sv_frame_t startFrame,
                                  sv_frame_t duration) const override {
        return m_events.toDelimitedDataString(delimiter,
                                              options,
                                              startFrame,
                                              duration,
                                              m_sampleRate,
                                              m_resolution,
                                              Event().withValue(0.f));
    }
  
protected:
    sv_samplerate_t m_sampleRate;
    int m_resolution;

    DeferredNotifier m_notifier;
    int m_completion;

    EventSeries m_events;

    mutable QMutex m_mutex;  

};


#endif


    
