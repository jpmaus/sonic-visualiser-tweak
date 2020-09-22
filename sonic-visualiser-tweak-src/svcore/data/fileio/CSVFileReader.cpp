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

#include "CSVFileReader.h"

#include "model/Model.h"
#include "base/RealTime.h"
#include "base/StringBits.h"
#include "base/ProgressReporter.h"
#include "base/RecordDirectory.h"
#include "model/SparseOneDimensionalModel.h"
#include "model/SparseTimeValueModel.h"
#include "model/EditableDenseThreeDimensionalModel.h"
#include "model/RegionModel.h"
#include "model/NoteModel.h"
#include "model/BoxModel.h"
#include "model/WritableWaveFileModel.h"
#include "DataFileReaderFactory.h"

#include <QFile>
#include <QDir>
#include <QFileInfo>
#include <QString>
#include <QRegExp>
#include <QStringList>
#include <QTextStream>
#include <QDateTime>

#include <iostream>
#include <map>
#include <string>

using namespace std;

CSVFileReader::CSVFileReader(QString path, CSVFormat format,
                             sv_samplerate_t mainModelSampleRate,
                             ProgressReporter *reporter) :
    m_format(format),
    m_device(nullptr),
    m_ownDevice(true),
    m_warnings(0),
    m_mainModelSampleRate(mainModelSampleRate),
    m_fileSize(0),
    m_readCount(0),
    m_progress(-1),
    m_reporter(reporter)
{
    QFile *file = new QFile(path);
    bool good = false;
    
    if (!file->exists()) {
        m_error = QFile::tr("File \"%1\" does not exist").arg(path);
    } else if (!file->open(QIODevice::ReadOnly | QIODevice::Text)) {
        m_error = QFile::tr("Failed to open file \"%1\"").arg(path);
    } else {
        good = true;
    }

    if (good) {
        m_device = file;
        m_filename = QFileInfo(path).fileName();
        m_fileSize = file->size();
        if (m_reporter) m_reporter->setDefinite(true);
    } else {
        delete file;
    }
}

CSVFileReader::CSVFileReader(QIODevice *device, CSVFormat format,
                             sv_samplerate_t mainModelSampleRate,
                             ProgressReporter *reporter) :
    m_format(format),
    m_device(device),
    m_ownDevice(false),
    m_warnings(0),
    m_mainModelSampleRate(mainModelSampleRate),
    m_fileSize(0),
    m_readCount(0),
    m_progress(-1),
    m_reporter(reporter)
{
    if (m_reporter) m_reporter->setDefinite(false);
}

CSVFileReader::~CSVFileReader()
{
    SVDEBUG << "CSVFileReader::~CSVFileReader: device is " << m_device << endl;

    if (m_device && m_ownDevice) {
        SVDEBUG << "CSVFileReader::CSVFileReader: Closing device" << endl;
        m_device->close();
        delete m_device;
    }
}

bool
CSVFileReader::isOK() const
{
    return (m_device != nullptr);
}

QString
CSVFileReader::getError() const
{
    return m_error;
}

sv_frame_t
CSVFileReader::convertTimeValue(QString s, int lineno,
                                sv_samplerate_t sampleRate,
                                int windowSize) const
{
    QRegExp nonNumericRx("[^0-9eE.,+-]");
    int warnLimit = 10;

    CSVFormat::TimeUnits timeUnits = m_format.getTimeUnits();

    sv_frame_t calculatedFrame = 0;

    bool ok = false;
    QString numeric = s;
    numeric.remove(nonNumericRx);
    
    if (timeUnits == CSVFormat::TimeSeconds) {

        double time = numeric.toDouble(&ok);
        if (!ok) time = StringBits::stringToDoubleLocaleFree(numeric, &ok);
        calculatedFrame = sv_frame_t(time * sampleRate + 0.5);
    
    } else if (timeUnits == CSVFormat::TimeMilliseconds) {

        double time = numeric.toDouble(&ok);
        if (!ok) time = StringBits::stringToDoubleLocaleFree(numeric, &ok);
        calculatedFrame = sv_frame_t((time / 1000.0) * sampleRate + 0.5);
        
    } else {
        
        long n = numeric.toLong(&ok);
        if (n >= 0) calculatedFrame = n;
        
        if (timeUnits == CSVFormat::TimeWindows) {
            calculatedFrame *= windowSize;
        }
    }
    
    if (!ok) {
        if (m_warnings < warnLimit) {
            SVCERR << "WARNING: CSVFileReader::load: "
                      << "Bad time format (\"" << s
                      << "\") in data line "
                      << lineno+1 << endl;
        } else if (m_warnings == warnLimit) {
            SVCERR << "WARNING: Too many warnings" << endl;
        }
        ++m_warnings;
    }

    return calculatedFrame;
}

Model *
CSVFileReader::load() const
{
    if (!m_device) return nullptr;

    CSVFormat::ModelType modelType = m_format.getModelType();
    CSVFormat::TimingType timingType = m_format.getTimingType();
    CSVFormat::TimeUnits timeUnits = m_format.getTimeUnits();
    sv_samplerate_t sampleRate = m_format.getSampleRate();
    int windowSize = m_format.getWindowSize();
    QChar separator = m_format.getSeparator();
    bool allowQuoting = m_format.getAllowQuoting();

    if (timingType == CSVFormat::ExplicitTiming) {
        if (modelType == CSVFormat::ThreeDimensionalModel) {
            // This will be overridden later if more than one line
            // appears in our file, but we want to choose a default
            // that's likely to be visible
            windowSize = 1024;
        } else {
            windowSize = 1;
        }
        if (timeUnits == CSVFormat::TimeSeconds ||
            timeUnits == CSVFormat::TimeMilliseconds) {
            sampleRate = m_mainModelSampleRate;
        }
    }

    SparseOneDimensionalModel *model1 = nullptr;
    SparseTimeValueModel *model2 = nullptr;
    RegionModel *model2a = nullptr;
    NoteModel *model2b = nullptr;
    BoxModel *model2c = nullptr;
    EditableDenseThreeDimensionalModel *model3 = nullptr;
    WritableWaveFileModel *modelW = nullptr;
    Model *model = nullptr;

    QTextStream in(m_device);

    unsigned int warnings = 0, warnLimit = 10;
    unsigned int lineno = 0;

    float min = 0.0, max = 0.0;

    sv_frame_t frameNo = 0;
    sv_frame_t duration = 0;
    sv_frame_t endFrame = 0;

    bool haveAnyValue = false;
    bool haveEndTime = false;
    bool pitchLooksLikeMIDI = true;

    sv_frame_t startFrame = 0; // for calculation of dense model resolution
    bool firstEverValue = true;
    
    int valueColumns = 0;
    for (int i = 0; i < m_format.getColumnCount(); ++i) {
        if (m_format.getColumnPurpose(i) == CSVFormat::ColumnValue) {
            ++valueColumns;
        }
    }

    int audioChannels = 0;
    float **audioSamples = nullptr;
    float sampleShift = 0.f;
    float sampleScale = 1.f;

    if (modelType == CSVFormat::WaveFileModel) {

        audioChannels = valueColumns;
                
        audioSamples =
            breakfastquay::allocate_and_zero_channels<float>
            (audioChannels, 1);

        switch (m_format.getAudioSampleRange()) {
        case CSVFormat::SampleRangeSigned1:
        case CSVFormat::SampleRangeOther:
            sampleShift = 0.f;
            sampleScale = 1.f;
            break;
        case CSVFormat::SampleRangeUnsigned255:
            sampleShift = -128.f;
            sampleScale = 1.f / 128.f;
            break;
        case CSVFormat::SampleRangeSigned32767:
            sampleShift = 0.f;
            sampleScale = 1.f / 32768.f;
            break;
        }
    }

    map<QString, int> labelCountMap;

    bool abandoned = false;
    
    while (!in.atEnd() && !abandoned) {

        // QTextStream's readLine doesn't cope with old-style Mac
        // CR-only line endings.  Why did they bother making the class
        // cope with more than one sort of line ending, if it still
        // can't be configured to cope with all the common sorts?

        // For the time being we'll deal with this case (which is
        // relatively uncommon for us, but still necessary to handle)
        // by reading the entire file using a single readLine, and
        // splitting it.  For CR and CR/LF line endings this will just
        // read a line at a time, and that's obviously OK.

        QString chunk = in.readLine();
        QStringList lines = chunk.split('\r', QString::SkipEmptyParts);

        m_readCount += chunk.size() + 1;

        if (m_reporter) {
            if (m_reporter->wasCancelled()) {
                abandoned = true;
                break;
            }
            int progress;
            if (m_fileSize > 0) {
                progress = int((double(m_readCount) / double(m_fileSize))
                               * 100.0);
            } else {
                progress = int(m_readCount / 10000);
            }
            if (progress != m_progress) {
                m_reporter->setProgress(progress);
                m_progress = progress;
            }
        }
        
        for (int li = 0; li < lines.size(); ++li) {

            QString line = lines[li];
            
            if (line.startsWith("#")) continue;

            QStringList list = StringBits::split(line, separator, allowQuoting);
            if (!model) {

                QString modelName = m_filename;
                
                switch (modelType) {

                case CSVFormat::OneDimensionalModel:
                    model1 = new SparseOneDimensionalModel(sampleRate, windowSize);
                    model = model1;
                    break;
                
                case CSVFormat::TwoDimensionalModel:
                    model2 = new SparseTimeValueModel(sampleRate, windowSize, false);
                    model = model2;
                    break;
                
                case CSVFormat::TwoDimensionalModelWithDuration:
                    model2a = new RegionModel(sampleRate, windowSize, false);
                    model = model2a;
                    break;
                
                case CSVFormat::TwoDimensionalModelWithDurationAndPitch:
                    model2b = new NoteModel(sampleRate, windowSize, false);
                    model = model2b;
                    break;
                
                case CSVFormat::TwoDimensionalModelWithDurationAndExtent:
                    model2c = new BoxModel(sampleRate, windowSize, false);
                    model = model2c;
                    break;
                
                case CSVFormat::ThreeDimensionalModel:
                    model3 = new EditableDenseThreeDimensionalModel
                        (sampleRate, windowSize, valueColumns);
                    model = model3;
                    break;

                case CSVFormat::WaveFileModel:
                {
                    bool normalise = (m_format.getAudioSampleRange()
                                      == CSVFormat::SampleRangeOther);
                    QString path = getConvertedAudioFilePath();
                    modelW = new WritableWaveFileModel
                        (path, sampleRate, valueColumns,
                         normalise ?
                         WritableWaveFileModel::Normalisation::Peak :
                         WritableWaveFileModel::Normalisation::None);
                    modelName = QFileInfo(path).fileName();
                    model = modelW;
                    break;
                }
                }

                if (model && model->isOK()) {
                    if (modelName != "") {
                        model->setObjectName(modelName);
                    }
                }
            }

            if (!model || !model->isOK()) {
                SVCERR << "Failed to create model to load CSV file into"
                       << endl;
                if (model) {
                    delete model;
                    model = nullptr;
                    model1 = nullptr; model2 = nullptr;
                    model2a = nullptr; model2b = nullptr; model2c = nullptr;
                    model3 = nullptr; modelW = nullptr;
                }
                abandoned = true;
                break;
            }
            
            float value = 0.f;
            float otherValue = 0.f;
            float pitch = 0.f;
            QString label = "";

            duration = 0.f;
            haveEndTime = false;
            
            for (int i = 0; i < list.size(); ++i) {

                QString s = list[i];

                CSVFormat::ColumnPurpose purpose = m_format.getColumnPurpose(i);

                switch (purpose) {

                case CSVFormat::ColumnUnknown:
                    break;

                case CSVFormat::ColumnStartTime:
                    frameNo = convertTimeValue(s, lineno, sampleRate, windowSize);
                    break;
                
                case CSVFormat::ColumnEndTime:
                    endFrame = convertTimeValue(s, lineno, sampleRate, windowSize);
                    haveEndTime = true;
                    break;

                case CSVFormat::ColumnDuration:
                    duration = convertTimeValue(s, lineno, sampleRate, windowSize);
                    break;

                case CSVFormat::ColumnValue:
                    if (haveAnyValue) {
                        otherValue = value;
                    }
                    value = s.toFloat();
                    haveAnyValue = true;
                    break;

                case CSVFormat::ColumnPitch:
                    pitch = s.toFloat();
                    if (pitch < 0.f || pitch > 127.f) {
                        pitchLooksLikeMIDI = false;
                    }
                    break;

                case CSVFormat::ColumnLabel:
                    label = s;
                    break;
                }
            }

            ++labelCountMap[label];
            
            if (haveEndTime) { // ... calculate duration now all cols read
                if (endFrame > frameNo) {
                    duration = endFrame - frameNo;
                }
            }

            if (modelType == CSVFormat::OneDimensionalModel) {
            
                Event point(frameNo, label);
                model1->add(point);

            } else if (modelType == CSVFormat::TwoDimensionalModel) {

                Event point(frameNo, value, label);
                model2->add(point);

            } else if (modelType == CSVFormat::TwoDimensionalModelWithDuration) {

                Event region(frameNo, value, duration, label);
                model2a->add(region);

            } else if (modelType == CSVFormat::TwoDimensionalModelWithDurationAndPitch) {

                float level = ((value >= 0.f && value <= 1.f) ? value : 1.f);
                Event note(frameNo, pitch, duration, level, label);
                model2b->add(note);

            } else if (modelType == CSVFormat::TwoDimensionalModelWithDurationAndExtent) {

                float level = 0.f;
                if (value > otherValue) {
                    level = value - otherValue;
                    value = otherValue;
                } else {
                    level = otherValue - value;
                }
                Event box(frameNo, value, duration, level, label);
                model2c->add(box);

            } else if (modelType == CSVFormat::ThreeDimensionalModel) {

                DenseThreeDimensionalModel::Column values;

                for (int i = 0; i < list.size(); ++i) {

                    if (m_format.getColumnPurpose(i) != CSVFormat::ColumnValue) {
                        continue;
                    }

                    bool ok = false;
                    float value = list[i].toFloat(&ok);

                    values.push_back(value);
            
                    if (firstEverValue || value < min) min = value;
                    if (firstEverValue || value > max) max = value;
                    
                    if (firstEverValue) {
                        startFrame = frameNo;
                        model3->setStartFrame(startFrame);
                    } else if (lineno == 1 &&
                               timingType == CSVFormat::ExplicitTiming) {
                        model3->setResolution(int(frameNo - startFrame));
                    }
                    
                    firstEverValue = false;

                    if (!ok) {
                        if (warnings < warnLimit) {
                            SVCERR << "WARNING: CSVFileReader::load: "
                                      << "Non-numeric value \""
                                      << list[i]
                                      << "\" in data line " << lineno+1
                                      << ":" << endl;
                            SVCERR << line << endl;
                            ++warnings;
                        } else if (warnings == warnLimit) {
//                            SVCERR << "WARNING: Too many warnings" << endl;
                        }
                    }
                }
        
//                SVDEBUG << "Setting bin values for count " << lineno << ", frame "
//                          << frameNo << ", time " << RealTime::frame2RealTime(frameNo, sampleRate) << endl;

                model3->setColumn(lineno, values);

            } else if (modelType == CSVFormat::WaveFileModel) {

                int channel = 0;

                for (int i = 0;
                     i < list.size() && channel < audioChannels;
                     ++i) {

                    if (m_format.getColumnPurpose(i) !=
                        CSVFormat::ColumnValue) {
                        continue;
                    }

                    bool ok = false;
                    float value = list[i].toFloat(&ok);
                    if (!ok) {
                        value = 0.f;
                    }

                    value += sampleShift;
                    value *= sampleScale;
                    
                    audioSamples[channel][0] = value;

                    ++channel;
                }

                while (channel < audioChannels) {
                    audioSamples[channel][0] = 0.f;
                    ++channel;
                }

                bool ok = modelW->addSamples(audioSamples, 1);
                
                if (!ok) {
                    if (warnings < warnLimit) {
                        SVCERR << "WARNING: CSVFileReader::load: "
                               << "Unable to add sample to wave-file model"
                               << endl;
                        SVCERR << line << endl;
                        ++warnings;
                    }
                }
            }
            
            ++lineno;
            if (timingType == CSVFormat::ImplicitTiming ||
                list.size() == 0) {
                frameNo += windowSize;
            }
        }
    }

    if (!haveAnyValue) {
        if (model2a) {
            // assign values for regions based on label frequency; we
            // have this in our labelCountMap, sort of

            map<int, map<QString, float> > countLabelValueMap;
            for (map<QString, int>::iterator i = labelCountMap.begin();
                 i != labelCountMap.end(); ++i) {
                countLabelValueMap[i->second][i->first] = -1.f;
            }

            float v = 0.f;
            for (map<int, map<QString, float> >::iterator i =
                     countLabelValueMap.end(); i != countLabelValueMap.begin(); ) {
                --i;
                SVCERR << "count -> " << i->first << endl;
                for (map<QString, float>::iterator j = i->second.begin();
                     j != i->second.end(); ++j) {
                    j->second = v;
                    SVCERR << "label -> " << j->first << ", value " << v << endl;
                    v = v + 1.f;
                }
            }

            map<Event, Event> eventMap;

            EventVector allEvents = model2a->getAllEvents();
            for (const Event &e: allEvents) {
                int count = labelCountMap[e.getLabel()];
                v = countLabelValueMap[count][e.getLabel()];
                // SVCERR << "mapping from label \"" << p.label
                //       << "\" (count " << count
                //       << ") to value " << v << endl;
                eventMap[e] = Event(e.getFrame(), v,
                                    e.getDuration(), e.getLabel());
            }

            for (const auto &i: eventMap) {
                // There could be duplicate regions; if so replace
                // them all -- but we need to check we're not
                // replacing a region by itself (or else this will
                // never terminate)
                if (i.first.getValue() == i.second.getValue()) {
                    continue;
                }
                while (model2a->containsEvent(i.first)) {
                    model2a->remove(i.first);
                    model2a->add(i.second);
                }
            }
        }
    }
                
    if (model2b) {
        if (pitchLooksLikeMIDI) {
            model2b->setScaleUnits("MIDI Pitch");
        } else {
            model2b->setScaleUnits("Hz");
        }
    }

    if (model3) {
        model3->setMinimumLevel(min);
        model3->setMaximumLevel(max);
    }

    if (modelW) {
        breakfastquay::deallocate_channels(audioSamples, audioChannels);
        modelW->updateModel();
        modelW->writeComplete();
    }

    return model;
}

QString
CSVFileReader::getConvertedAudioFilePath() const
{
    QString base = m_filename;
    base.replace(QRegExp("[/\\,.:;~<>\"'|?%*]+"), "_");

    QString convertedFileDir = RecordDirectory::getConvertedAudioDirectory();
    if (convertedFileDir == "") {
        SVCERR << "WARNING: CSVFileReader::getConvertedAudioFilePath: Failed to retrieve converted audio directory" << endl;
        return "";
    }

    auto ms = QDateTime::currentDateTime().toMSecsSinceEpoch();
    auto s = ms / 1000; // there is a toSecsSinceEpoch in Qt 5.8 but
                        // we currently want to support older versions
    
    return QDir(convertedFileDir).filePath
        (QString("%1-%2.wav").arg(base).arg(s));
}

