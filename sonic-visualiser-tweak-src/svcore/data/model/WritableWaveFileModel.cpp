/* -*- c-basic-offset: 4 indent-tabs-mode: nil -*-  vi:set ts=8 sts=4 sw=4: */

/*
    Sonic Visualiser
    An audio file viewer and annotation editor.
    Centre for Digital Music, Queen Mary, University of London.
    This file copyright 2006 QMUL.
    
    This program is free software; you can redistribute it and/or
    modify it under the terms of the GNU General Public License as
    published by the Free Software Foundation; either version 2 of the
    License, or (at your option) any later version.  See the file
    COPYING included with this distribution for more information.
*/

#include "WritableWaveFileModel.h"

#include "ReadOnlyWaveFileModel.h"

#include "base/TempDirectory.h"
#include "base/Exceptions.h"
#include "base/PlayParameterRepository.h"

#include "fileio/WavFileWriter.h"
#include "fileio/WavFileReader.h"

#include <QDir>
#include <QTextStream>

#include <cassert>
#include <iostream>
#include <stdint.h>

using namespace std;

const int WritableWaveFileModel::PROPORTION_UNKNOWN = -1;

//#define DEBUG_WRITABLE_WAVE_FILE_MODEL 1

WritableWaveFileModel::WritableWaveFileModel(QString path,
                                             sv_samplerate_t sampleRate,
                                             int channels,
                                             Normalisation norm) :
    m_model(nullptr),
    m_temporaryWriter(nullptr),
    m_targetWriter(nullptr),
    m_reader(nullptr),
    m_normalisation(norm),
    m_sampleRate(sampleRate),
    m_channels(channels),
    m_frameCount(0),
    m_startFrame(0),
    m_proportion(PROPORTION_UNKNOWN)
{
    init(path);
}

WritableWaveFileModel::WritableWaveFileModel(sv_samplerate_t sampleRate,
                                             int channels,
                                             Normalisation norm) :
    m_model(nullptr),
    m_temporaryWriter(nullptr),
    m_targetWriter(nullptr),
    m_reader(nullptr),
    m_normalisation(norm),
    m_sampleRate(sampleRate),
    m_channels(channels),
    m_frameCount(0),
    m_startFrame(0),
    m_proportion(PROPORTION_UNKNOWN)
{
    init();
}

WritableWaveFileModel::WritableWaveFileModel(sv_samplerate_t sampleRate,
                                             int channels) :
    m_model(nullptr),
    m_temporaryWriter(nullptr),
    m_targetWriter(nullptr),
    m_reader(nullptr),
    m_normalisation(Normalisation::None),
    m_sampleRate(sampleRate),
    m_channels(channels),
    m_frameCount(0),
    m_startFrame(0),
    m_proportion(PROPORTION_UNKNOWN)
{
    init();
}

void
WritableWaveFileModel::init(QString path)
{
    if (path.isEmpty()) {
        try {
            // Temp dir is exclusive to this run of the application,
            // so the filename only needs to be unique within that -
            // model ID should be ok
            QDir dir(TempDirectory::getInstance()->getPath());
            path = dir.filePath(QString("written_%1.wav")
                                .arg(getId().untyped));
        } catch (const DirectoryCreationFailed &f) {
            SVCERR << "WritableWaveFileModel: Failed to create temporary directory" << endl;
            return;
        }
    }

    m_targetPath = path;
    m_temporaryPath = "";

    // We don't delete or null-out writer/reader members after
    // failures here - they are all deleted in the dtor, and the
    // presence/existence of the model is what's used to determine
    // whether to go ahead, not the writer/readers. If the model is
    // non-null, then the necessary writer/readers must be OK, as the
    // model is the last thing initialised
    
    m_targetWriter = new WavFileWriter(m_targetPath, m_sampleRate, m_channels,
                                       WavFileWriter::WriteToTarget);
    
    if (!m_targetWriter->isOK()) {
        SVCERR << "WritableWaveFileModel: Error in creating WAV file writer: " << m_targetWriter->getError() << endl;
        return;
    }
    
    if (m_normalisation != Normalisation::None) {

        // Temp dir is exclusive to this run of the application, so
        // the filename only needs to be unique within that
        QDir dir(TempDirectory::getInstance()->getPath());
        m_temporaryPath = dir.filePath(QString("prenorm_%1.wav")
                                       .arg(getId().untyped));

        m_temporaryWriter = new WavFileWriter
            (m_temporaryPath, m_sampleRate, m_channels,
             WavFileWriter::WriteToTarget);
    
        if (!m_temporaryWriter->isOK()) {
            SVCERR << "WritableWaveFileModel: Error in creating temporary WAV file writer: " << m_temporaryWriter->getError() << endl;
            return;
        }
    }        

    FileSource source(m_targetPath);

    m_reader = new WavFileReader(source, true);
    if (!m_reader->getError().isEmpty()) {
        SVCERR << "WritableWaveFileModel: Error in creating wave file reader: " << m_reader->getError() << endl;
        return;
    }
    
    m_model = new ReadOnlyWaveFileModel(source, m_reader);
    if (!m_model->isOK()) {
        SVCERR << "WritableWaveFileModel: Error in creating wave file model" << endl;
        delete m_model;
        m_model = nullptr;
        return;
    }
    m_model->setStartFrame(m_startFrame);

    connect(m_model, SIGNAL(modelChanged(ModelId)),
            this, SLOT(componentModelChanged(ModelId)));
    connect(m_model, SIGNAL(modelChangedWithin(ModelId, sv_frame_t, sv_frame_t)),
            this, SLOT(componentModelChangedWithin(ModelId, sv_frame_t, sv_frame_t)));
    
    PlayParameterRepository::getInstance()->addPlayable
        (getId().untyped, this);
}

WritableWaveFileModel::~WritableWaveFileModel()
{
    PlayParameterRepository::getInstance()->removePlayable
        (getId().untyped);
    
    delete m_model;
    delete m_targetWriter;
    delete m_temporaryWriter;
    delete m_reader;
}

void
WritableWaveFileModel::componentModelChanged(ModelId)
{
    emit modelChanged(getId());
}

void
WritableWaveFileModel::componentModelChangedWithin(ModelId, sv_frame_t f0, sv_frame_t f1)
{
    emit modelChangedWithin(getId(), f0, f1);
}

void
WritableWaveFileModel::setStartFrame(sv_frame_t startFrame)
{
    m_startFrame = startFrame;
    if (m_model) {
        m_model->setStartFrame(startFrame);
    }
}

bool
WritableWaveFileModel::addSamples(const float *const *samples, sv_frame_t count)
{
    if (!m_model) return false;

#ifdef DEBUG_WRITABLE_WAVE_FILE_MODEL
//    SVDEBUG << "WritableWaveFileModel::addSamples(" << count << ")" << endl;
#endif

    WavFileWriter *writer = m_targetWriter;
    if (m_normalisation != Normalisation::None) {
        writer = m_temporaryWriter;
    }
    
    if (!writer->writeSamples(samples, count)) {
        SVCERR << "ERROR: WritableWaveFileModel::addSamples: writer failed: " << writer->getError() << endl;
        return false;
    }

    m_frameCount += count;

    if (m_normalisation == Normalisation::None) {
        if (m_reader->getChannelCount() == 0) {
            m_reader->updateFrameCount();
        }
    }

    return true;
}

void
WritableWaveFileModel::updateModel()
{
    if (!m_model) return;
    
    m_reader->updateFrameCount();
}

bool
WritableWaveFileModel::isOK() const
{
    return (m_model && m_model->isOK());
}

void
WritableWaveFileModel::setWriteProportion(int proportion)
{
    m_proportion = proportion;
}

int
WritableWaveFileModel::getWriteProportion() const
{
    return m_proportion;
}

void
WritableWaveFileModel::writeComplete()
{
    if (!m_model) return;

    if (m_normalisation == Normalisation::None) {
        m_targetWriter->close();
    } else {
        m_temporaryWriter->close();
        normaliseToTarget();
    }
    
    m_reader->updateDone();
    m_proportion = 100;
    emit modelChanged(getId());
    emit writeCompleted(getId());
}

void
WritableWaveFileModel::normaliseToTarget()
{
    if (m_temporaryPath == "") {
        SVCERR << "WritableWaveFileModel::normaliseToTarget: No temporary path available" << endl;
        return;
    }
    
    WavFileReader normalisingReader(m_temporaryPath, false,
                                    WavFileReader::Normalisation::Peak);

    if (!normalisingReader.getError().isEmpty()) {
        SVCERR << "WritableWaveFileModel: Error in creating normalising reader: " << normalisingReader.getError() << endl;
        return;
    }

    sv_frame_t frame = 0;
    sv_frame_t block = 65536;
    sv_frame_t count = normalisingReader.getFrameCount();

    while (frame < count) {
        auto frames = normalisingReader.getInterleavedFrames(frame, block);
        if (!m_targetWriter->putInterleavedFrames(frames)) {
            SVCERR << "ERROR: WritableWaveFileModel::normaliseToTarget: writer failed: " << m_targetWriter->getError() << endl;
            return;
        }
        frame += block;
    }

    m_targetWriter->close();

    delete m_temporaryWriter;
    m_temporaryWriter = nullptr;
    QFile::remove(m_temporaryPath);
}

sv_frame_t
WritableWaveFileModel::getFrameCount() const
{
//    SVDEBUG << "WritableWaveFileModel::getFrameCount: count = " << m_frameCount << endl;
    return m_frameCount;
}

floatvec_t
WritableWaveFileModel::getData(int channel, sv_frame_t start, sv_frame_t count) const
{
    if (!m_model || m_model->getChannelCount() == 0) return {};
    return m_model->getData(channel, start, count);
}

vector<floatvec_t>
WritableWaveFileModel::getMultiChannelData(int fromchannel, int tochannel,
                                           sv_frame_t start, sv_frame_t count) const
{
    if (!m_model || m_model->getChannelCount() == 0) return {};
    return m_model->getMultiChannelData(fromchannel, tochannel, start, count);
}    

int
WritableWaveFileModel::getSummaryBlockSize(int desired) const
{
    if (!m_model) return desired;
    return m_model->getSummaryBlockSize(desired);
}

void
WritableWaveFileModel::getSummaries(int channel, sv_frame_t start, sv_frame_t count,
                                    RangeBlock &ranges,
                                    int &blockSize) const
{
    ranges.clear();
    if (!m_model || m_model->getChannelCount() == 0) return;
    m_model->getSummaries(channel, start, count, ranges, blockSize);
}

WritableWaveFileModel::Range
WritableWaveFileModel::getSummary(int channel, sv_frame_t start, sv_frame_t count) const
{
    if (!m_model || m_model->getChannelCount() == 0) return Range();
    return m_model->getSummary(channel, start, count);
}

void
WritableWaveFileModel::toXml(QTextStream &out,
                             QString indent,
                             QString extraAttributes) const
{
    // The assumption here is that the underlying wave file has
    // already been saved somewhere (its location is available through
    // getLocation()) and that the code that uses this class is
    // dealing with the problem of making sure it remains available.
    // We just write this out as if it were a normal wave file.

    Model::toXml
        (out, indent,
         QString("type=\"wavefile\" file=\"%1\" subtype=\"writable\" %2")
         .arg(encodeEntities(m_targetPath))
         .arg(extraAttributes));
}

