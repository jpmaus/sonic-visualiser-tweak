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

#ifndef SV_EVENT_COMMANDS_H
#define SV_EVENT_COMMANDS_H

#include "base/Event.h"
#include "base/Command.h"
#include "base/ById.h"

/**
 * Interface for classes that can be modified through these commands
 */
class EventEditable
{
public:
    virtual void add(Event e) = 0;
    virtual void remove(Event e) = 0;
};

class WithEditable
{
protected:
    WithEditable(int editableId) : m_editableId(editableId) { }

    std::shared_ptr<EventEditable> getEditable() {
        auto editable = AnyById::getAs<EventEditable>(m_editableId);
        if (!editable) {
            SVCERR << "WARNING: Id passed to EventEditable command is not that of an EventEditable" << endl;
        }
        return editable;
    }

private:
    int m_editableId;
};

/**
 * Command to add an event to an editable containing events, with
 * undo.  The id must be that of a type that can be retrieved from the
 * AnyById store and dynamic_cast to EventEditable.
 */
class AddEventCommand : public Command,
                        public WithEditable
{
public:
    AddEventCommand(int editableId, const Event &e, QString name) :
        WithEditable(editableId), m_event(e), m_name(name) { }

    QString getName() const override { return m_name; }
    Event getEvent() const { return m_event; }

    void execute() override {
        auto editable = getEditable();
        if (editable) editable->add(m_event);
    }
    void unexecute() override {
        auto editable = getEditable();
        if (editable) editable->remove(m_event);
    }

private:
    Event m_event;
    QString m_name;
};

/**
 * Command to remove an event from an editable containing events, with
 * undo.  The id must be that of a type that can be retrieved from the
 * AnyById store and dynamic_cast to EventEditable.
 */
class RemoveEventCommand : public Command,
                           public WithEditable
{
public:
    RemoveEventCommand(int editableId, const Event &e, QString name) :
        WithEditable(editableId), m_event(e), m_name(name) { }

    QString getName() const override { return m_name; }
    Event getEvent() const { return m_event; }

    void execute() override {
        auto editable = getEditable();
        if (editable) editable->remove(m_event);
    }
    void unexecute() override {
        auto editable = getEditable();
        if (editable) editable->add(m_event);
    }

private:
    Event m_event;
    QString m_name;
};

/**
 * Command to add or remove a series of events to or from an editable,
 * with undo. Creates and immediately executes a sub-command for each
 * add/remove requested. Consecutive add/remove pairs for the same
 * point are collapsed.  The id must be that of a type that can be
 * retrieved from the AnyById store and dynamic_cast to EventEditable.
 */
class ChangeEventsCommand : public MacroCommand
{
public:
    ChangeEventsCommand(int editableId, QString name) :
        MacroCommand(name), m_editableId(editableId) { }

    void add(Event e) {
        addCommand(new AddEventCommand(m_editableId, e, getName()), true);
    }
    void remove(Event e) {
        addCommand(new RemoveEventCommand(m_editableId, e, getName()), true);
    }

    /**
     * Stack an arbitrary other command in the same sequence.
     */
    void addCommand(Command *command) override { addCommand(command, true); }

    /**
     * If any points have been added or deleted, return this
     * command (so the caller can add it to the command history).
     * Otherwise delete the command and return NULL.
     */
    ChangeEventsCommand *finish() {
        if (!m_commands.empty()) {
            return this;
        } else {
            delete this;
            return nullptr;
        }
    }

protected:
    virtual void addCommand(Command *command, bool executeFirst) {
        
        if (executeFirst) command->execute();

        if (m_commands.empty()) {
            MacroCommand::addCommand(command);
            return;
        }
        
        RemoveEventCommand *r =
            dynamic_cast<RemoveEventCommand *>(command);
        AddEventCommand *a =
            dynamic_cast<AddEventCommand *>(*m_commands.rbegin());
        if (r && a) {
            if (a->getEvent() == r->getEvent()) {
                deleteCommand(a);
                return;
            }
        }
        
        MacroCommand::addCommand(command);
    }

    int m_editableId;
};

#endif
