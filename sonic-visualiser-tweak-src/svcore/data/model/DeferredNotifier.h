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

#ifndef SV_DEFERRED_NOTIFIER_H
#define SV_DEFERRED_NOTIFIER_H

#include "Model.h"

#include "base/Extents.h"

#include <QMutex>
#include <QMutexLocker>

class DeferredNotifier
{
public:
    enum Mode {
        NOTIFY_ALWAYS,
        NOTIFY_DEFERRED
    };
    
    DeferredNotifier(Model *m, ModelId id, Mode mode) :
        m_model(m), m_modelId(id), m_mode(mode) { }

    Mode getMode() const {
        return m_mode;
    }
    void switchMode(Mode newMode) {
        m_mode = newMode;
    }
    
    void update(sv_frame_t frame, sv_frame_t duration) {
        if (m_mode == NOTIFY_ALWAYS) {
            m_model->modelChangedWithin(m_modelId, frame, frame + duration);
        } else {
            QMutexLocker locker(&m_mutex);
            m_extents.sample(frame);
            m_extents.sample(frame + duration);
        }
    }
    
    void makeDeferredNotifications() {
        bool shouldEmit = false;
        sv_frame_t from, to;
        {   QMutexLocker locker(&m_mutex);
            if (m_extents.isSet()) {
                shouldEmit = true;
                from = m_extents.getMin();
                to = m_extents.getMax();
            }
        }
        if (shouldEmit) {
            m_model->modelChangedWithin(m_modelId, from, to);
            QMutexLocker locker(&m_mutex);
            m_extents.reset();
        }
    }

private:
    Model *m_model;
    ModelId m_modelId;
    Mode m_mode;
    QMutex m_mutex;
    Extents<sv_frame_t> m_extents;
};

#endif
