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

#include "AudioCallbackRecordTarget.h"

#include "base/ViewManagerBase.h"
#include "base/RecordDirectory.h"
#include "base/Debug.h"

#include "data/model/WritableWaveFileModel.h"

#include <QDir>
#include <QTimer>
#include <QDateTime>

//#define DEBUG_AUDIO_CALLBACK_RECORD_TARGET 1

static const int recordUpdateTimeout = 200; // ms

AudioCallbackRecordTarget::AudioCallbackRecordTarget(ViewManagerBase *manager,
                                                     QString clientName) :
    m_viewManager(manager),
    m_clientName(clientName.toUtf8().data()),
    m_recording(false),
    m_recordSampleRate(44100),
    m_recordChannelCount(2),
    m_frameCount(0),
    m_model(nullptr),
    m_buffers(nullptr),
    m_bufferCount(0),
    m_inputLeft(0.f),
    m_inputRight(0.f),
    m_levelsSet(false)
{
    m_viewManager->setAudioRecordTarget(this);

    connect(this, SIGNAL(recordStatusChanged(bool)),
            m_viewManager, SLOT(recordStatusChanged(bool)));

    recreateBuffers();
}

AudioCallbackRecordTarget::~AudioCallbackRecordTarget()
{
#ifdef DEBUG_AUDIO_CALLBACK_RECORD_TARGET
    cerr << "AudioCallbackRecordTarget dtor" << endl;
#endif
    
    m_viewManager->setAudioRecordTarget(nullptr);

    QMutexLocker locker(&m_bufPtrMutex);
    for (int c = 0; c < m_bufferCount; ++c) {
        delete m_buffers[c];
    }
    delete[] m_buffers;
}

void
AudioCallbackRecordTarget::recreateBuffers()
{
    static int bufferSize = 441000;
    
    int count = m_recordChannelCount;

    if (count > m_bufferCount) {

        RingBuffer<float> **newBuffers = new RingBuffer<float> *[count];
        for (int c = 0; c < m_bufferCount; ++c) {
            newBuffers[c] = m_buffers[c];
        }
        for (int c = m_bufferCount; c < count; ++c) {
            newBuffers[c] = new RingBuffer<float>(bufferSize);
        }

        // This is the only place where m_buffers is rewritten and
        // should be the only possible source of contention against
        // putSamples for this mutex (as the model-updating code is
        // supposed to run in the same thread as this)
        QMutexLocker locker(&m_bufPtrMutex);
        delete[] m_buffers;
        m_buffers = newBuffers;
        m_bufferCount = count;
    }
}    
    
int
AudioCallbackRecordTarget::getApplicationSampleRate() const
{
    return 0; // don't care
}

int
AudioCallbackRecordTarget::getApplicationChannelCount() const
{
    return m_recordChannelCount;
}

void
AudioCallbackRecordTarget::setSystemRecordBlockSize(int)
{
}

void
AudioCallbackRecordTarget::setSystemRecordSampleRate(int n)
{
    m_recordSampleRate = n;
}

void
AudioCallbackRecordTarget::setSystemRecordLatency(int)
{
}

void
AudioCallbackRecordTarget::setSystemRecordChannelCount(int c)
{
    m_recordChannelCount = c;
    recreateBuffers();
}

void
AudioCallbackRecordTarget::putSamples(const float *const *samples, int, int nframes)
{
    // This may be called from RT context, and in a different thread
    // from everything else in this class. It takes a mutex that
    // should almost never be contended (see recreateBuffers())
    if (!m_recording) return;

    QMutexLocker locker(&m_bufPtrMutex);
    if (m_buffers && m_bufferCount >= m_recordChannelCount) {
        for (int c = 0; c < m_recordChannelCount; ++c) {
            m_buffers[c]->write(samples[c], nframes);
        }
    }
}

void
AudioCallbackRecordTarget::updateModel()
{
#ifdef DEBUG_AUDIO_CALLBACK_RECORD_TARGET
    cerr << "AudioCallbackRecordTarget::updateModel" << endl;
#endif
    
    sv_frame_t frameToEmit = 0;

    int nframes = 0;
    for (int c = 0; c < m_recordChannelCount; ++c) {
        if (c == 0 || m_buffers[c]->getReadSpace() < nframes) {
            nframes = m_buffers[c]->getReadSpace();
        }
    }

    if (nframes == 0) {
#ifdef DEBUG_AUDIO_CALLBACK_RECORD_TARGET
        cerr << "AudioCallbackRecordTarget::updateModel: no frames available" << endl;
#endif 
        if (m_recording) {
            QTimer::singleShot(recordUpdateTimeout, this, SLOT(updateModel()));
        }    
        return;
    }

#ifdef DEBUG_AUDIO_CALLBACK_RECORD_TARGET
    cerr << "AudioCallbackRecordTarget::updateModel: have " << nframes << " frames" << endl;
#endif

    if (!m_model) {
#ifdef DEBUG_AUDIO_CALLBACK_RECORD_TARGET
        cerr << "AudioCallbackRecordTarget::updateModel: have no model to update; I am hoping there is a good reason for this" << endl;
#endif
        return;
    }

    float **samples = new float *[m_recordChannelCount];
    for (int c = 0; c < m_recordChannelCount; ++c) {
        samples[c] = new float[nframes];
        m_buffers[c]->read(samples[c], nframes);
    }

    m_model->addSamples(samples, nframes);

    for (int c = 0; c < m_recordChannelCount; ++c) {
        delete[] samples[c];
    }
    delete[] samples;
    
    m_frameCount += nframes;
    
    m_model->updateModel();
    frameToEmit = m_frameCount;
    emit recordDurationChanged(frameToEmit, m_recordSampleRate);

    if (m_recording) {
        QTimer::singleShot(recordUpdateTimeout, this, SLOT(updateModel()));
    }    
}

void
AudioCallbackRecordTarget::setInputLevels(float left, float right)
{
    if (left > m_inputLeft) m_inputLeft = left;
    if (right > m_inputRight) m_inputRight = right;
    m_levelsSet = true;
}

bool
AudioCallbackRecordTarget::getInputLevels(float &left, float &right)
{
    left = m_inputLeft;
    right = m_inputRight;
    bool valid = m_levelsSet;
    m_inputLeft = 0.f;
    m_inputRight = 0.f;
    m_levelsSet = false;
    return valid;
}

void
AudioCallbackRecordTarget::modelAboutToBeDeleted()
{
    if (sender() == m_model) {
#ifdef DEBUG_AUDIO_CALLBACK_RECORD_TARGET
        cerr << "AudioCallbackRecordTarget::modelAboutToBeDeleted: taking note" << endl;
#endif
        m_model = nullptr;
        m_recording = false;
    } else if (m_model) {
        SVCERR << "WARNING: AudioCallbackRecordTarget::modelAboutToBeDeleted: this is not my model!" << endl;
    }
}

WritableWaveFileModel *
AudioCallbackRecordTarget::startRecording()
{
    if (m_recording) {
        SVCERR << "WARNING: AudioCallbackRecordTarget::startRecording: We are already recording" << endl;
        return nullptr;
    }

    m_model = nullptr;
    m_frameCount = 0;

    QString folder = RecordDirectory::getRecordDirectory();
    if (folder == "") return nullptr;
    QDir recordedDir(folder);

    QDateTime now = QDateTime::currentDateTime();

    // Don't use QDateTime::toString(Qt::ISODate) as the ":" character
    // isn't permitted in filenames on Windows
    QString nowString = now.toString("yyyyMMdd-HHmmss-zzz");
    
    QString filename = tr("recorded-%1.wav").arg(nowString);
    QString label = tr("Recorded %1").arg(nowString);

    m_audioFileName = recordedDir.filePath(filename);

    m_model = new WritableWaveFileModel
        (m_audioFileName,
         m_recordSampleRate,
         m_recordChannelCount,
         WritableWaveFileModel::Normalisation::None);

    if (!m_model->isOK()) {
        SVCERR << "ERROR: AudioCallbackRecordTarget::startRecording: Recording failed"
               << endl;
        //!!! and throw?
        delete m_model;
        m_model = nullptr;
        return nullptr;
    }

    connect(m_model, SIGNAL(aboutToBeDeleted()), 
            this, SLOT(modelAboutToBeDeleted()));

    m_model->setObjectName(label);
    m_recording = true;

    emit recordStatusChanged(true);

    QTimer::singleShot(recordUpdateTimeout, this, SLOT(updateModel()));
    
    return m_model;
}

void
AudioCallbackRecordTarget::stopRecording()
{
    if (!m_recording) {
        SVCERR << "WARNING: AudioCallbackRecordTarget::startRecording: Not recording" << endl;
        return;
    }

    m_recording = false;

    m_bufPtrMutex.lock();
    m_bufPtrMutex.unlock();

    // buffers should now be up-to-date
    updateModel();

    m_model->writeComplete();
    m_model = nullptr;
    
    emit recordStatusChanged(false);
    emit recordCompleted();
}


