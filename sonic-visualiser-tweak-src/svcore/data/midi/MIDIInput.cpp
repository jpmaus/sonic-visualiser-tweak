/* -*- c-basic-offset: 4 indent-tabs-mode: nil -*-  vi:set ts=8 sts=4 sw=4: */

/*
    Sonic Visualiser
    An audio file viewer and annotation editor.
    Centre for Digital Music, Queen Mary, University of London.
    This file copyright 2006-2009 Chris Cannam and QMUL.
    
    This program is free software; you can redistribute it and/or
    modify it under the terms of the GNU General Public License as
    published by the Free Software Foundation; either version 2 of the
    License, or (at your option) any later version.  See the file
    COPYING included with this distribution for more information.
*/

#include "MIDIInput.h"

#include "rtmidi/RtMidi.h"

#include "system/System.h"

MIDIInput::MIDIInput(QString name, FrameTimer *timer) :
    m_rtmidi(nullptr),
    m_frameTimer(timer),
    m_buffer(1023)
{
    try {
        std::vector<RtMidi::Api> apis;
        RtMidi::getCompiledApi(apis);
        RtMidi::Api preferredApi = RtMidi::UNSPECIFIED;
        for (auto a: apis) {
            if (a == RtMidi::UNSPECIFIED || a == RtMidi::RTMIDI_DUMMY) {
                continue;
            }
            preferredApi = a;
            break;
        }
        if (preferredApi == RtMidi::UNSPECIFIED) {
            SVCERR << "ERROR: MIDIInput: No RtMidi APIs compiled in" << endl;
        } else {

            m_rtmidi = new RtMidiIn(preferredApi, name.toStdString());

            int n = m_rtmidi->getPortCount();

            if (n == 0) {

                SVDEBUG << "NOTE: MIDIInput: No input ports available" << endl;
                delete m_rtmidi;
                m_rtmidi = nullptr;

            } else {

                m_rtmidi->setCallback(staticCallback, this);

                SVDEBUG << "MIDIInput: Available ports are:" << endl;
                for (int i = 0; i < n; ++i) {
                    SVDEBUG << i << ". " << m_rtmidi->getPortName(i) << endl;
                }
                SVDEBUG << "MIDIInput: Using first port (\""
                        << m_rtmidi->getPortName(0) << "\")" << endl;
            
                m_rtmidi->openPort(0, tr("Input").toStdString());
            }
        }
        
    } catch (const RtMidiError &e) {
        SVCERR << "ERROR: RtMidi error: " << e.getMessage() << endl;
        delete m_rtmidi;
        m_rtmidi = nullptr;
    }
}

MIDIInput::~MIDIInput()
{
    delete m_rtmidi;
}

void
MIDIInput::staticCallback(double timestamp, std::vector<unsigned char> *message,
                          void *userData)
{
    ((MIDIInput *)userData)->callback(timestamp, message);
}

void
MIDIInput::callback(double timestamp, std::vector<unsigned char> *message)
{
    SVDEBUG << "MIDIInput::callback(" << timestamp << ")" << endl;
    // In my experience so far, the timings passed to this function
    // are not reliable enough to use.  We request instead an audio
    // frame time from whatever FrameTimer we have been given, and use
    // that as the event time.
    if (!message || message->empty()) return;
    unsigned long t = m_frameTimer->getFrame();
    MIDIByte code = (*message)[0];
    MIDIEvent ev(t,
                 code,
                 message->size() > 1 ? (*message)[1] : 0,
                 message->size() > 2 ? (*message)[2] : 0);
    postEvent(ev);
}

MIDIEvent
MIDIInput::readEvent()
{
    MIDIEvent *event = m_buffer.readOne();
    MIDIEvent revent = *event;
    delete event;
    return revent;
}

void
MIDIInput::postEvent(MIDIEvent e)
{
    int count = 0, max = 5;
    while (m_buffer.getWriteSpace() == 0) {
        if (count == max) {
            SVCERR << "ERROR: MIDIInput::postEvent: MIDI event queue is full and not clearing -- abandoning incoming event" << endl;
            return;
        }
        SVCERR << "WARNING: MIDIInput::postEvent: MIDI event queue (capacity " << m_buffer.getSize() << ") is full!" << endl;
        SVDEBUG << "Waiting for something to be processed" << endl;
#ifdef _WIN32
        Sleep(1);
#else
        sleep(1);
#endif
        count++;
    }

    MIDIEvent *me = new MIDIEvent(e);
    m_buffer.write(&me, 1);
    emit eventsAvailable();
}

