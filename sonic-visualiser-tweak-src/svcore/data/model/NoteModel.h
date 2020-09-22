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

#ifndef SV_NOTE_MODEL_H
#define SV_NOTE_MODEL_H

#include "Model.h"
#include "TabularModel.h"
#include "EventCommands.h"
#include "DeferredNotifier.h"
#include "base/UnitDatabase.h"
#include "base/EventSeries.h"
#include "base/NoteData.h"
#include "base/NoteExportable.h"
#include "base/RealTime.h"
#include "base/PlayParameterRepository.h"
#include "base/Pitch.h"
#include "system/System.h"

#include <QMutexLocker>

class NoteModel : public Model,
                  public TabularModel,
                  public NoteExportable,
                  public EventEditable
{
    Q_OBJECT
    
public:
    enum Subtype {
        NORMAL_NOTE,
        FLEXI_NOTE
    };
    
    NoteModel(sv_samplerate_t sampleRate,
              int resolution,
              bool notifyOnAdd = true,
              Subtype subtype = NORMAL_NOTE) :
        m_subtype(subtype),
        m_sampleRate(sampleRate),
        m_resolution(resolution),
        m_valueMinimum(0.f),
        m_valueMaximum(0.f),
        m_haveExtents(false),
        m_valueQuantization(0),
        m_units(""),
        m_notifier(this,
                   getId(),
                   notifyOnAdd ?
                   DeferredNotifier::NOTIFY_ALWAYS :
                   DeferredNotifier::NOTIFY_DEFERRED),
        m_completion(100) {
        if (subtype == FLEXI_NOTE) {
            m_valueMinimum = 33.f;
            m_valueMaximum = 88.f;
        }
        PlayParameterRepository::getInstance()->addPlayable
            (getId().untyped, this);
    }

    NoteModel(sv_samplerate_t sampleRate, int resolution,
              float valueMinimum, float valueMaximum,
              bool notifyOnAdd = true,
              Subtype subtype = NORMAL_NOTE) :
        m_subtype(subtype),
        m_sampleRate(sampleRate),
        m_resolution(resolution),
        m_valueMinimum(valueMinimum),
        m_valueMaximum(valueMaximum),
        m_haveExtents(true),
        m_valueQuantization(0),
        m_units(""),
        m_notifier(this,
                   getId(),
                   notifyOnAdd ?
                   DeferredNotifier::NOTIFY_ALWAYS :
                   DeferredNotifier::NOTIFY_DEFERRED),
        m_completion(100) {
        PlayParameterRepository::getInstance()->addPlayable
            (getId().untyped, this);
    }

    virtual ~NoteModel() {
        PlayParameterRepository::getInstance()->removePlayable
            (getId().untyped);
    }

    QString getTypeName() const override { return tr("Note"); }
    Subtype getSubtype() const { return m_subtype; }
    bool isSparse() const override { return true; }
    bool isOK() const override { return true; }
    
    sv_frame_t getStartFrame() const override {
        QMutexLocker locker(&m_mutex);
        return m_events.getStartFrame();
    }
    sv_frame_t getTrueEndFrame() const override {
        QMutexLocker locker(&m_mutex);
        if (m_events.isEmpty()) return 0;
        sv_frame_t e = m_events.getEndFrame();
        if (e % m_resolution == 0) return e;
        else return (e / m_resolution + 1) * m_resolution;
    }

    sv_samplerate_t getSampleRate() const override { return m_sampleRate; }
    int getResolution() const { return m_resolution; }

    bool canPlay() const override { return true; }
    QString getDefaultPlayClipId() const override {
        return "elecpiano";
    }

    QString getScaleUnits() const {
        QMutexLocker locker(&m_mutex);
        return m_units;
    }
    void setScaleUnits(QString units) {
        QMutexLocker locker(&m_mutex);
        m_units = units;
        UnitDatabase::getInstance()->registerUnit(units);
    }

    float getValueQuantization() const { return m_valueQuantization; }
    void setValueQuantization(float q) { m_valueQuantization = q; }

    float getValueMinimum() const { return m_valueMinimum; }
    float getValueMaximum() const { return m_valueMaximum; }
    
    int getCompletion() const override { return m_completion; }

    void setCompletion(int completion, bool update = true) {

        {
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
    int getIndexForEvent(const Event &e) {
        return m_events.getIndexForEvent(e);
    }

    /**
     * Editing methods.
     */
    void add(Event e) override {

        bool allChange = false;
           
        m_events.add(e);
        float v = e.getValue();
        if (!ISNAN(v) && !ISINF(v)) {
            if (!m_haveExtents || v < m_valueMinimum) {
                m_valueMinimum = v; allChange = true;
            }
            if (!m_haveExtents || v > m_valueMaximum) {
                m_valueMaximum = v; allChange = true;
            }
            m_haveExtents = true;
        }
        
        m_notifier.update(e.getFrame(), e.getDuration() + m_resolution);

        if (allChange) {
            emit modelChanged(getId());
        }
    }
    
    void remove(Event e) override {
        m_events.remove(e);
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
        case 2: return tr("Pitch");
        case 3: return tr("Duration");
        case 4: return tr("Level");
        case 5: return tr("Label");
        default: return tr("Unknown");
        }
    }

    QVariant getData(int row, int column, int role) const override {

        if (row < 0 || row >= m_events.count()) {
            return QVariant();
        }

        Event e = m_events.getEventByIndex(row);

        switch (column) {
        case 0: return adaptFrameForRole(e.getFrame(), getSampleRate(), role);
        case 1: return int(e.getFrame());
        case 2: return adaptValueForRole(e.getValue(), getScaleUnits(), role);
        case 3: return int(e.getDuration());
        case 4: return e.getLevel();
        case 5: return e.getLabel();
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
        case 3: e1 = e0.withDuration(value.toInt()); break;
        case 4: e1 = e0.withLevel(float(value.toDouble())); break;
        case 5: e1 = e0.withLabel(value.toString()); break;
        }

        auto command = new ChangeEventsCommand(getId().untyped, tr("Edit Data"));
        command->remove(e0);
        command->add(e1);
        return command->finish();
    }

    SortType getSortType(int column) const override
    {
        if (column == 5) return SortAlphabetical;
        return SortNumeric;
    }

    bool isEditable() const override { return true; }

    Command *getInsertRowCommand(int row) override {
        if (row < 0 || row >= m_events.count()) return nullptr;
        auto command = new ChangeEventsCommand(getId().untyped,
                                               tr("Add Note"));
        Event e = m_events.getEventByIndex(row);
        command->add(e);
        return command->finish();
    }

    Command *getRemoveRowCommand(int row) override {
        if (row < 0 || row >= m_events.count()) return nullptr;
        auto command = new ChangeEventsCommand(getId().untyped,
                                               tr("Delete Note"));
        Event e = m_events.getEventByIndex(row);
        command->remove(e);
        return command->finish();
    }

    /**
     * NoteExportable methods.
     */

    NoteList getNotes() const override {
        return getNotesStartingWithin(getStartFrame(),
                                      getEndFrame() - getStartFrame());
    }

    NoteList getNotesActiveAt(sv_frame_t frame) const override {

        NoteList notes;
        EventVector ee = m_events.getEventsCovering(frame);
        for (const auto &e: ee) {
            notes.push_back(e.toNoteData(getSampleRate(),
                                         getScaleUnits() != "Hz"));
        }
        return notes;
    }
    
    NoteList getNotesStartingWithin(sv_frame_t startFrame,
                                    sv_frame_t duration) const override {

        NoteList notes;
        EventVector ee = m_events.getEventsStartingWithin(startFrame, duration);
        for (const auto &e: ee) {
            notes.push_back(e.toNoteData(getSampleRate(),
                                         getScaleUnits() != "Hz"));
        }
        return notes;
    }

    /**
     * XmlExportable methods.
     */
    
    void toXml(QTextStream &out,
               QString indent = "",
               QString extraAttributes = "") const override {

        //!!! what is valueQuantization used for?
        
        Model::toXml
            (out,
             indent,
             QString("type=\"sparse\" dimensions=\"3\" resolution=\"%1\" "
                     "notifyOnAdd=\"%2\" dataset=\"%3\" subtype=\"%4\" "
                     "valueQuantization=\"%5\" minimum=\"%6\" maximum=\"%7\" "
                     "units=\"%8\" %9")
             .arg(m_resolution)
             .arg("true") // always true after model reaches 100% -
                          // subsequent events are always notified
             .arg(m_events.getExportId())
             .arg(m_subtype == FLEXI_NOTE ? "flexinote" : "note")
             .arg(m_valueQuantization)
             .arg(m_valueMinimum)
             .arg(m_valueMaximum)
             .arg(encodeEntities(m_units))
             .arg(extraAttributes));
        
        m_events.toXml(out, indent, QString("dimensions=\"3\""));
    }

    QString toDelimitedDataString(QString delimiter,
                                  DataExportOptions options,
                                  sv_frame_t startFrame,
                                  sv_frame_t duration) const override {
        return m_events.toDelimitedDataString
            (delimiter,
             options,
             startFrame,
             duration,
             m_sampleRate,
             m_resolution,
             Event().withValue(0.f).withDuration(0.f).withLevel(0.f));
    }

protected:
    Subtype m_subtype;
    sv_samplerate_t m_sampleRate;
    int m_resolution;

    std::atomic<float> m_valueMinimum;
    std::atomic<float> m_valueMaximum;
    std::atomic<bool> m_haveExtents;
    float m_valueQuantization;
    QString m_units;
    DeferredNotifier m_notifier;
    std::atomic<int> m_completion;

    EventSeries m_events;
};

#endif
