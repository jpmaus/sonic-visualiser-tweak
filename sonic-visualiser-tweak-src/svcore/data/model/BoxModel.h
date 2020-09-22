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

#ifndef SV_BOX_MODEL_H
#define SV_BOX_MODEL_H

#include "EventCommands.h"
#include "TabularModel.h"
#include "Model.h"
#include "DeferredNotifier.h"

#include "base/RealTime.h"
#include "base/EventSeries.h"
#include "base/UnitDatabase.h"

#include "system/System.h"

#include <QMutex>

/**
 * BoxModel -- a model for annotations having start time, duration,
 * and a value range. We use Events as usual for these, but treat the
 * "value" as the lower value and "level" as the difference between
 * lower and upper values, which is expected to be non-negative (if it
 * is negative, abs(level) will be used).
 *
 * This is expected to be used most often for time-frequency boxes.
 */
class BoxModel : public Model,
                 public TabularModel,
                 public EventEditable
{
    Q_OBJECT
    
public:
    BoxModel(sv_samplerate_t sampleRate,
             int resolution,
             bool notifyOnAdd = true) :
        m_sampleRate(sampleRate),
        m_resolution(resolution),
        m_valueMinimum(0.f),
        m_valueMaximum(0.f),
        m_haveExtents(false),
        m_notifier(this,
                   getId(),
                   notifyOnAdd ?
                   DeferredNotifier::NOTIFY_ALWAYS :
                   DeferredNotifier::NOTIFY_DEFERRED),
        m_completion(100) {
    }

    BoxModel(sv_samplerate_t sampleRate, int resolution,
             float valueMinimum, float valueMaximum,
             bool notifyOnAdd = true) :
        m_sampleRate(sampleRate),
        m_resolution(resolution),
        m_valueMinimum(valueMinimum),
        m_valueMaximum(valueMaximum),
        m_haveExtents(true),
        m_notifier(this,
                   getId(),
                   notifyOnAdd ?
                   DeferredNotifier::NOTIFY_ALWAYS :
                   DeferredNotifier::NOTIFY_DEFERRED),
        m_completion(100) {
    }

    virtual ~BoxModel() {
    }

    QString getTypeName() const override { return tr("Box"); }
    bool isSparse() const override { return true; }
    bool isOK() const override { return true; }

    sv_frame_t getStartFrame() const override {
        return m_events.getStartFrame();
    }
    sv_frame_t getTrueEndFrame() const override {
        if (m_events.isEmpty()) return 0;
        sv_frame_t e = m_events.getEndFrame();
        if (e % m_resolution == 0) return e;
        else return (e / m_resolution + 1) * m_resolution;
    }

    sv_samplerate_t getSampleRate() const override { return m_sampleRate; }
    int getResolution() const { return m_resolution; }

    QString getScaleUnits() const { return m_units; }
    void setScaleUnits(QString units) {
        m_units = units;
        UnitDatabase::getInstance()->registerUnit(units);
    }

    float getValueMinimum() const { return m_valueMinimum; }
    float getValueMaximum() const { return m_valueMaximum; }
    
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
    EventVector getEventsWithin(sv_frame_t f, sv_frame_t duration) const {
        return m_events.getEventsWithin(f, duration);
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

        bool allChange = false;

        {
            QMutexLocker locker(&m_mutex);
            m_events.add(e);

            float f0 = e.getValue();
            float f1 = f0 + fabsf(e.getLevel());
            
            if (!m_haveExtents || f0 < m_valueMinimum) {
                m_valueMinimum = f0; allChange = true;
            }
            if (!m_haveExtents || f1 > m_valueMaximum) {
                m_valueMaximum = f1; allChange = true;
            }
            m_haveExtents = true;
        }
        
        m_notifier.update(e.getFrame(), e.getDuration() + m_resolution);

        if (allChange) {
            emit modelChanged(getId());
        }
    }
    
    void remove(Event e) override {
        {
            QMutexLocker locker(&m_mutex);
            m_events.remove(e);
        }
        emit modelChangedWithin(getId(),
                                e.getFrame(),
                                e.getFrame() + e.getDuration() + m_resolution);
    }
    
    /**
     * TabularModel methods.  
     */

    int getRowCount() const override {
        return m_events.count();
    }

    int getColumnCount() const override {
        return 6;
    }

    bool isColumnTimeValue(int column) const override {
        // NB duration is not a "time value" -- that's for columns
        // whose sort ordering is exactly that of the frame time
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
        case 2: return tr("Duration");
        case 3: return tr("Min Freq");
        case 4: return tr("Max Freq");
        case 5: return tr("Label");
        default: return tr("Unknown");
        }
    }

    SortType getSortType(int column) const override {
        if (column == 5) return SortAlphabetical;
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
        case 2: return int(e.getDuration());
        case 3: return adaptValueForRole(e.getValue(), getScaleUnits(), role);
        case 4: return adaptValueForRole(e.getValue() + fabsf(e.getLevel()),
                                         getScaleUnits(), role);
        case 5: return e.getLabel();
        default: return QVariant();
        }
    }

    bool isEditable() const override { return true; }

    Command *getSetDataCommand(int row, int column, const QVariant &value,
                               int role) override {
        
        if (row < 0 || row >= m_events.count()) return nullptr;
        if (role != Qt::EditRole) return nullptr;

        Event e0 = m_events.getEventByIndex(row);
        Event e1;

        switch (column) {
        case 0: e1 = e0.withFrame(sv_frame_t(round(value.toDouble() *
                                                   getSampleRate()))); break;
        case 1: e1 = e0.withFrame(value.toInt()); break;
        case 2: e1 = e0.withDuration(value.toInt()); break;
        case 3: e1 = e0.withValue(float(value.toDouble())); break;
        case 4: e1 = e0.withLevel(fabsf(float(value.toDouble()) -
                                        e0.getValue())); break;
        case 5: e1 = e0.withLabel(value.toString()); break;
        }

        auto command = new ChangeEventsCommand(getId().untyped, tr("Edit Data"));
        command->remove(e0);
        command->add(e1);
        return command->finish();
    }

    Command *getInsertRowCommand(int row) override {
        if (row < 0 || row >= m_events.count()) return nullptr;
        auto command = new ChangeEventsCommand(getId().untyped,
                                               tr("Add Box"));
        Event e = m_events.getEventByIndex(row);
        command->add(e);
        return command->finish();
    }

    Command *getRemoveRowCommand(int row) override {
        if (row < 0 || row >= m_events.count()) return nullptr;
        auto command = new ChangeEventsCommand(getId().untyped,
                                               tr("Delete Box"));
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
                     "notifyOnAdd=\"%2\" dataset=\"%3\" subtype=\"%4\" "
                     "minimum=\"%5\" maximum=\"%6\" units=\"%7\" %8")
             .arg(m_resolution)
             .arg("true") // always true after model reaches 100% -
                          // subsequent events are always notified
             .arg(m_events.getExportId())
             .arg("box")
             .arg(m_valueMinimum)
             .arg(m_valueMaximum)
             .arg(encodeEntities(m_units))
             .arg(extraAttributes));

        Event::ExportNameOptions options;
        options.levelAttributeName = "extent";
        
        m_events.toXml(out, indent, QString("dimensions=\"2\""), options);
    }

    QString toDelimitedDataString(QString delimiter,
                                  DataExportOptions,
                                  sv_frame_t startFrame,
                                  sv_frame_t duration) const override {

        // We need a custom format here

        EventVector ee = m_events.getEventsSpanning(startFrame, duration);

        QString s;
        
        for (auto e: ee) {

            QStringList list;

            list << RealTime::frame2RealTime
                (e.getFrame(), getSampleRate())
                .toString().c_str()
                 << RealTime::frame2RealTime
                (e.getFrame() + e.getDuration(), getSampleRate())
                .toString().c_str()
                 << QString("%1").arg(e.getValue())
                 << QString("%1").arg(e.getValue() + fabsf(e.getLevel()));
            
            if (e.getLabel() != "") {
                list << e.getLabel();
            }

            s += list.join(delimiter) + "\n";
        }

        return s;
    }

protected:
    sv_samplerate_t m_sampleRate;
    int m_resolution;

    float m_valueMinimum;
    float m_valueMaximum;
    bool m_haveExtents;
    QString m_units;
    DeferredNotifier m_notifier;
    int m_completion;

    EventSeries m_events;

    mutable QMutex m_mutex;
};

#endif
