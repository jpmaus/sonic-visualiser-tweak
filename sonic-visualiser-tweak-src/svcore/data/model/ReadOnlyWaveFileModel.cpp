/* -*- c-basic-offset: 4 indent-tabs-mode: nil -*-  vi:set ts=8 sts=4 sw=4: */

/*
    Sonic Visualiser
    An audio file viewer and annotation editor.
    Centre for Digital Music, Queen Mary, University of London.
    This file copyright 2006 Chris Cannam and QMUL.
    
    This program is free software; you can redistribute it and/or
    modify it under the terms of the GNU General Public License as
    published by the Free Software Foundation; either version 2 of the
    License, or (at your option) any later version.  See the file
    COPYING included with this distribution for more information.
*/

#include "ReadOnlyWaveFileModel.h"

#include "fileio/AudioFileReader.h"
#include "fileio/AudioFileReaderFactory.h"

#include "system/System.h"

#include "base/Preferences.h"
#include "base/PlayParameterRepository.h"

#include <QFileInfo>
#include <QTextStream>

#include <iostream>
#include <cmath>
#include <sndfile.h>

#include <cassert>

using namespace std;

//#define DEBUG_WAVE_FILE_MODEL 1
//#define DEBUG_WAVE_FILE_MODEL_READ 1

PowerOfSqrtTwoZoomConstraint
ReadOnlyWaveFileModel::m_zoomConstraint;

ReadOnlyWaveFileModel::ReadOnlyWaveFileModel(FileSource source, sv_samplerate_t targetRate) :
    m_source(source),
    m_path(source.getLocation()),
    m_reader(nullptr),
    m_myReader(true),
    m_startFrame(0),
    m_fillThread(nullptr),
    m_updateTimer(nullptr),
    m_lastFillExtent(0),
    m_prevCompletion(0),
    m_exiting(false),
    m_lastDirectReadStart(0),
    m_lastDirectReadCount(0)
{
    SVDEBUG << "ReadOnlyWaveFileModel::ReadOnlyWaveFileModel: path "
            << m_path << ", target rate " << targetRate << endl;
    
    m_source.waitForData();

    if (m_source.isOK()) {

        Preferences *prefs = Preferences::getInstance();
        
        AudioFileReaderFactory::Parameters params;
        params.targetRate = targetRate;

        params.normalisation = prefs->getNormaliseAudio() ?
            AudioFileReaderFactory::Normalisation::Peak :
            AudioFileReaderFactory::Normalisation::None;

        params.gaplessMode = prefs->getUseGaplessMode() ?
            AudioFileReaderFactory::GaplessMode::Gapless :
            AudioFileReaderFactory::GaplessMode::Gappy;
        
        params.threadingMode = AudioFileReaderFactory::ThreadingMode::Threaded;

        m_reader = AudioFileReaderFactory::createReader(m_source, params);
        if (m_reader) {
            SVDEBUG << "ReadOnlyWaveFileModel::ReadOnlyWaveFileModel: reader rate: "
                      << m_reader->getSampleRate() << endl;
        }
    }

    if (m_reader) setObjectName(m_reader->getTitle());
    if (objectName() == "") setObjectName(QFileInfo(m_path).fileName());
    if (isOK()) fillCache();
    
    PlayParameterRepository::getInstance()->addPlayable
        (getId().untyped, this);
}

ReadOnlyWaveFileModel::ReadOnlyWaveFileModel(FileSource source, AudioFileReader *reader) :
    m_source(source),
    m_path(source.getLocation()),
    m_reader(nullptr),
    m_myReader(false),
    m_startFrame(0),
    m_fillThread(nullptr),
    m_updateTimer(nullptr),
    m_lastFillExtent(0),
    m_prevCompletion(0),
    m_exiting(false)
{
    SVDEBUG << "ReadOnlyWaveFileModel::ReadOnlyWaveFileModel: path "
            << m_path << ", with reader" << endl;
    
    m_reader = reader;
    if (m_reader) setObjectName(m_reader->getTitle());
    if (objectName() == "") setObjectName(QFileInfo(m_path).fileName());
    fillCache();
    
    PlayParameterRepository::getInstance()->addPlayable
        (getId().untyped, this);
}

ReadOnlyWaveFileModel::~ReadOnlyWaveFileModel()
{
    PlayParameterRepository::getInstance()->removePlayable
        (getId().untyped);
    
    m_exiting = true;
    if (m_fillThread) m_fillThread->wait();
    if (m_myReader) delete m_reader;
    m_reader = nullptr;

    SVDEBUG << "ReadOnlyWaveFileModel: Destructor exiting; we had caches of "
            << (m_cache[0].size() * sizeof(Range)) << " and "
            << (m_cache[1].size() * sizeof(Range)) << " bytes" << endl;
}

bool
ReadOnlyWaveFileModel::isOK() const
{
    return m_reader && m_reader->isOK();
}

bool
ReadOnlyWaveFileModel::isReady(int *completion) const
{
    bool ready = true;
    if (!isOK()) ready = false;
    if (m_fillThread) ready = false;
    if (m_reader && m_reader->isUpdating()) ready = false;

    double c = double(m_lastFillExtent) /
        double(getEndFrame() - getStartFrame());

    if (completion) {
        *completion = int(c * 100.0 + 0.01);
        if (m_reader) {
            int decodeCompletion = m_reader->getDecodeCompletion();
            if (decodeCompletion < 90) *completion = decodeCompletion;
            else *completion = min(*completion, decodeCompletion);
        }
        if (*completion != 0 &&
            *completion != 100 &&
            m_prevCompletion != 0 &&
            m_prevCompletion > *completion) {
            // just to avoid completion going backwards
            *completion = m_prevCompletion;
        }
        m_prevCompletion = *completion;
    }
    
#ifdef DEBUG_WAVE_FILE_MODEL
    if (completion) {
        SVCERR << "ReadOnlyWaveFileModel(" << objectName() << ")::isReady(): ready = " << ready << ", m_fillThread = " << m_fillThread << ", m_lastFillExtent = " << m_lastFillExtent << ", end frame = " << getEndFrame() << ", start frame = " << getStartFrame() << ", c = " << c << ", completion = " << *completion << endl;
    } else {
        SVCERR << "ReadOnlyWaveFileModel(" << objectName() << ")::isReady(): ready = " << ready << ", m_fillThread = " << m_fillThread << ", m_lastFillExtent = " << m_lastFillExtent << ", end frame = " << getEndFrame() << ", start frame = " << getStartFrame() << ", c = " << c << ", completion not requested" << endl;
    }
#endif
    return ready;
}

sv_frame_t
ReadOnlyWaveFileModel::getFrameCount() const
{
    if (!m_reader) return 0;
    return m_reader->getFrameCount();
}

int
ReadOnlyWaveFileModel::getChannelCount() const
{
    if (!m_reader) return 0;
    return m_reader->getChannelCount();
}

sv_samplerate_t
ReadOnlyWaveFileModel::getSampleRate() const 
{
    if (!m_reader) return 0;
    return m_reader->getSampleRate();
}

sv_samplerate_t
ReadOnlyWaveFileModel::getNativeRate() const 
{
    if (!m_reader) return 0;
    sv_samplerate_t rate = m_reader->getNativeRate();
    if (rate == 0) rate = getSampleRate();
    return rate;
}

QString
ReadOnlyWaveFileModel::getTitle() const
{
    QString title;
    if (m_reader) title = m_reader->getTitle();
    if (title == "") title = objectName();
    return title;
}

QString
ReadOnlyWaveFileModel::getMaker() const
{
    if (m_reader) return m_reader->getMaker();
    return "";
}

QString
ReadOnlyWaveFileModel::getLocation() const
{
    if (m_reader) return m_reader->getLocation();
    return "";
}

QString
ReadOnlyWaveFileModel::getLocalFilename() const
{
#ifdef DEBUG_WAVE_FILE_MODEL
    SVCERR << "ReadOnlyWaveFileModel::getLocalFilename: reader is "
           << m_reader << ", returning "
           << (m_reader ? m_reader->getLocalFilename() : "(none)") << endl;
#endif
    if (m_reader) return m_reader->getLocalFilename();
    return "";
}
    
floatvec_t
ReadOnlyWaveFileModel::getData(int channel,
                               sv_frame_t start,
                               sv_frame_t count)
    const
{
    // Read a single channel (if channel >= 0) or a mixdown of all
    // channels (if channel == -1) directly from the file.  This is
    // used for e.g. audio playback or input to transforms.

    Profiler profiler("ReadOnlyWaveFileModel::getData");
    
#ifdef DEBUG_WAVE_FILE_MODEL_READ
    cout << "ReadOnlyWaveFileModel::getData[" << this << "]: " << channel << ", " << start << ", " << count << endl;
#endif

    int channels = getChannelCount();

    if (channel >= channels) {
        SVCERR << "ERROR: WaveFileModel::getData: channel ("
             << channel << ") >= channel count (" << channels << ")"
             << endl;
        return {};
    }

    if (!m_reader || !m_reader->isOK() || count == 0) {
        return {};
    }

    if (start >= m_startFrame) {
        start -= m_startFrame;
    } else {
        if (count <= m_startFrame - start) {
            return {};
        } else {
            count -= (m_startFrame - start);
            start = 0;
        }
    }

    floatvec_t interleaved = m_reader->getInterleavedFrames(start, count);
    if (channels == 1) return interleaved;

    sv_frame_t obtained = interleaved.size() / channels;
    
    floatvec_t result(obtained, 0.f);
    
    if (channel != -1) {
        // get a single channel
        for (int i = 0; i < obtained; ++i) {
            result[i] = interleaved[i * channels + channel];
        }
    } else {
        // channel == -1, mix down all channels
        for (int i = 0; i < obtained; ++i) {
            for (int c = 0; c < channels; ++c) {
                result[i] += interleaved[i * channels + c];
            }
        }
    }

    return result;
}

vector<floatvec_t>
ReadOnlyWaveFileModel::getMultiChannelData(int fromchannel, int tochannel,
                                           sv_frame_t start, sv_frame_t count) const
{
    // Read a set of channels directly from the file.  This is used
    // for e.g. audio playback or input to transforms.

    Profiler profiler("ReadOnlyWaveFileModel::getMultiChannelData");

#ifdef DEBUG_WAVE_FILE_MODEL_READ
    cout << "ReadOnlyWaveFileModel::getData[" << this << "]: " << fromchannel << "," << tochannel << ", " << start << ", " << count << endl;
#endif

    int channels = getChannelCount();

    if (fromchannel > tochannel) {
        SVCERR << "ERROR: ReadOnlyWaveFileModel::getMultiChannelData: "
               << "fromchannel (" << fromchannel
               << ") > tochannel (" << tochannel << ")"
               << endl;
        return {};
    }

    if (tochannel >= channels) {
        SVCERR << "ERROR: ReadOnlyWaveFileModel::getMultiChannelData: "
               << "tochannel (" << tochannel
               << ") >= channel count (" << channels << ")"
               << endl;
        return {};
    }

    if (!m_reader || !m_reader->isOK() || count == 0) {
        return {};
    }

    int reqchannels = (tochannel - fromchannel) + 1;

    if (start >= m_startFrame) {
        start -= m_startFrame;
    } else {
        if (count <= m_startFrame - start) {
            return {};
        } else {
            count -= (m_startFrame - start);
            start = 0;
        }
    }

    floatvec_t interleaved = m_reader->getInterleavedFrames(start, count);
    if (channels == 1) return { interleaved };

    sv_frame_t obtained = interleaved.size() / channels;
    vector<floatvec_t> result(reqchannels, floatvec_t(obtained, 0.f));

    for (int c = fromchannel; c <= tochannel; ++c) {
        int destc = c - fromchannel;
        for (int i = 0; i < obtained; ++i) {
            result[destc][i] = interleaved[i * channels + c];
        }
    }
    
    return result;
}

int
ReadOnlyWaveFileModel::getSummaryBlockSize(int desired) const
{
    int cacheType = 0;
    int power = m_zoomConstraint.getMinCachePower();
    int roundedBlockSize = m_zoomConstraint.getNearestBlockSize
        (desired, cacheType, power, ZoomConstraint::RoundDown);

    if (cacheType != 0 && cacheType != 1) {
        // We will be reading directly from file, so can satisfy any
        // blocksize requirement
        return desired;
    } else {
        return roundedBlockSize;
    }
}    

void
ReadOnlyWaveFileModel::getSummaries(int channel, sv_frame_t start, sv_frame_t count,
                                    RangeBlock &ranges, int &blockSize) const
{
    ranges.clear();
    if (!isOK()) return;
    ranges.reserve((count / blockSize) + 1);

    if (start > m_startFrame) start -= m_startFrame;
    else if (count <= m_startFrame - start) return;
    else {
        count -= (m_startFrame - start);
        start = 0;
    }

    int cacheType = 0;
    int power = m_zoomConstraint.getMinCachePower();
    int roundedBlockSize = m_zoomConstraint.getNearestBlockSize
        (blockSize, cacheType, power, ZoomConstraint::RoundDown);

    int channels = getChannelCount();

    if (cacheType != 0 && cacheType != 1) {

        // We need to read directly from the file.  We haven't got
        // this cached.  Hope the requested area is small.  This is
        // not optimal -- we'll end up reading the same frames twice
        // for stereo files, in two separate calls to this method.
        // We could fairly trivially handle this for most cases that
        // matter by putting a single cache in getInterleavedFrames
        // for short queries.

        m_directReadMutex.lock();

        if (m_lastDirectReadStart != start ||
            m_lastDirectReadCount != count ||
            m_directRead.empty()) {

            m_directRead = m_reader->getInterleavedFrames(start, count);
            m_lastDirectReadStart = start;
            m_lastDirectReadCount = count;
        }

        float max = 0.0, min = 0.0, total = 0.0;
        sv_frame_t i = 0, got = 0;

        while (i < count) {

            sv_frame_t index = i * channels + channel;
            if (index >= (sv_frame_t)m_directRead.size()) break;
            
            float sample = m_directRead[index];
            if (sample > max || got == 0) max = sample;
            if (sample < min || got == 0) min = sample;
            total += fabsf(sample);

            ++i;
            ++got;
            
            if (got == blockSize) {
                ranges.push_back(Range(min, max, total / float(got)));
                min = max = total = 0.0f;
                got = 0;
            }
        }

        m_directReadMutex.unlock();

        if (got > 0) {
            ranges.push_back(Range(min, max, total / float(got)));
        }

        return;

    } else {

        QMutexLocker locker(&m_mutex);
    
        const RangeBlock &cache = m_cache[cacheType];

        blockSize = roundedBlockSize;

        sv_frame_t cacheBlock, div;

        cacheBlock = (sv_frame_t(1) << m_zoomConstraint.getMinCachePower());
        if (cacheType == 1) {
            cacheBlock = sv_frame_t(double(cacheBlock) * sqrt(2.) + 0.01);
        }
        div = blockSize / cacheBlock;

        sv_frame_t startIndex = start / cacheBlock;
        sv_frame_t endIndex = (start + count) / cacheBlock;

        float max = 0.0, min = 0.0, total = 0.0;
        sv_frame_t i = 0, got = 0;

#ifdef DEBUG_WAVE_FILE_MODEL_READ
        cerr << "blockSize is " << blockSize << ", cacheBlock " << cacheBlock << ", start " << start << ", count " << count << " (frame count " << getFrameCount() << "), power is " << power << ", div is " << div << ", startIndex " << startIndex << ", endIndex " << endIndex << endl;
#endif

        for (i = 0; i <= endIndex - startIndex; ) {
        
            sv_frame_t index = (i + startIndex) * channels + channel;
            if (!in_range_for(cache, index)) break;
            
            const Range &range = cache[index];
            if (range.max() > max || got == 0) max = range.max();
            if (range.min() < min || got == 0) min = range.min();
            total += range.absmean();
            
            ++i;
            ++got;
            
            if (got == div) {
                ranges.push_back(Range(min, max, total / float(got)));
                min = max = total = 0.0f;
                got = 0;
            }
        }
                
        if (got > 0) {
            ranges.push_back(Range(min, max, total / float(got)));
        }
    }

#ifdef DEBUG_WAVE_FILE_MODEL_READ
    cerr << "returning " << ranges.size() << " ranges" << endl;
#endif
    return;
}

ReadOnlyWaveFileModel::Range
ReadOnlyWaveFileModel::getSummary(int channel, sv_frame_t start, sv_frame_t count) const
{
    Range range;
    if (!isOK()) return range;

    if (start > m_startFrame) start -= m_startFrame;
    else if (count <= m_startFrame - start) return range;
    else {
        count -= (m_startFrame - start);
        start = 0;
    }

    int blockSize;
    for (blockSize = 1; blockSize <= count; blockSize *= 2);
    if (blockSize > 1) blockSize /= 2;

    bool first = false;

    sv_frame_t blockStart = (start / blockSize) * blockSize;
    sv_frame_t blockEnd = ((start + count) / blockSize) * blockSize;

    if (blockStart < start) blockStart += blockSize;
        
    if (blockEnd > blockStart) {
        RangeBlock ranges;
        getSummaries(channel, blockStart, blockEnd - blockStart, ranges, blockSize);
        for (int i = 0; i < (int)ranges.size(); ++i) {
            if (first || ranges[i].min() < range.min()) range.setMin(ranges[i].min());
            if (first || ranges[i].max() > range.max()) range.setMax(ranges[i].max());
            if (first || ranges[i].absmean() < range.absmean()) range.setAbsmean(ranges[i].absmean());
            first = false;
        }
    }

    if (blockStart > start) {
        Range startRange = getSummary(channel, start, blockStart - start);
        range.setMin(min(range.min(), startRange.min()));
        range.setMax(max(range.max(), startRange.max()));
        range.setAbsmean(min(range.absmean(), startRange.absmean()));
    }

    if (blockEnd < start + count) {
        Range endRange = getSummary(channel, blockEnd, start + count - blockEnd);
        range.setMin(min(range.min(), endRange.min()));
        range.setMax(max(range.max(), endRange.max()));
        range.setAbsmean(min(range.absmean(), endRange.absmean()));
    }

    return range;
}

void
ReadOnlyWaveFileModel::fillCache()
{
    m_mutex.lock();

    m_updateTimer = new QTimer(this);
    connect(m_updateTimer, SIGNAL(timeout()), this, SLOT(fillTimerTimedOut()));
    m_updateTimer->start(100);

    m_fillThread = new RangeCacheFillThread(*this);
    connect(m_fillThread, SIGNAL(finished()), this, SLOT(cacheFilled()));

    m_mutex.unlock();
    m_fillThread->start();

#ifdef DEBUG_WAVE_FILE_MODEL
    SVCERR << "ReadOnlyWaveFileModel(" << objectName() << ")::fillCache: started fill thread" << endl;
#endif
}   

void
ReadOnlyWaveFileModel::fillTimerTimedOut()
{
    if (m_fillThread) {
        sv_frame_t fillExtent = m_fillThread->getFillExtent();
#ifdef DEBUG_WAVE_FILE_MODEL
        SVCERR << "ReadOnlyWaveFileModel(" << objectName() << ")::fillTimerTimedOut: extent = " << fillExtent << endl;
#endif
        if (fillExtent > m_lastFillExtent) {
            emit modelChangedWithin(getId(), m_lastFillExtent, fillExtent);
            m_lastFillExtent = fillExtent;
        }
    } else {
#ifdef DEBUG_WAVE_FILE_MODEL
        SVCERR << "ReadOnlyWaveFileModel(" << objectName() << ")::fillTimerTimedOut: no thread" << endl;
#endif
        emit modelChanged(getId());
    }
}

void
ReadOnlyWaveFileModel::cacheFilled()
{
    m_mutex.lock();
    delete m_fillThread;
    m_fillThread = nullptr;
    delete m_updateTimer;
    m_updateTimer = nullptr;
    auto prevFillExtent = m_lastFillExtent;
    m_lastFillExtent = getEndFrame();
    m_mutex.unlock();
#ifdef DEBUG_WAVE_FILE_MODEL
    SVCERR << "ReadOnlyWaveFileModel(" << objectName() << ")::cacheFilled, about to emit things" << endl;
#endif
    if (getEndFrame() > prevFillExtent) {
        emit modelChangedWithin(getId(), prevFillExtent, getEndFrame());
    }
    emit modelChanged(getId());
    emit ready(getId());
}

void
ReadOnlyWaveFileModel::RangeCacheFillThread::run()
{
    int cacheBlockSize[2];
    cacheBlockSize[0] = (1 << m_model.m_zoomConstraint.getMinCachePower());
    cacheBlockSize[1] = (int((1 << m_model.m_zoomConstraint.getMinCachePower()) *
                                        sqrt(2.) + 0.01));
    
    sv_frame_t frame = 0;
    const sv_frame_t readBlockSize = 32768;
    floatvec_t block;

    if (!m_model.isOK()) return;
    
    int channels = m_model.getChannelCount();
    bool updating = m_model.m_reader->isUpdating();

    if (updating) {
        while (channels == 0 && !m_model.m_exiting) {
#ifdef DEBUG_WAVE_FILE_MODEL
            SVCERR << "ReadOnlyWaveFileModel(" << objectName() << ")::fill: Waiting for channels..." << endl;
#endif
            sleep(1);
            channels = m_model.getChannelCount();
        }
    }

    Range *range = new Range[2 * channels];
    float *means = new float[2 * channels];
    int count[2];
    count[0] = count[1] = 0;
    for (int i = 0; i < 2 * channels; ++i) {
        means[i] = 0.f;
    }

    bool first = true;

    while (first || updating) {

        updating = m_model.m_reader->isUpdating();
        m_frameCount = m_model.getFrameCount();

        m_model.m_mutex.lock();

        while (frame < m_frameCount) {

            m_model.m_mutex.unlock();

#ifdef DEBUG_WAVE_FILE_MODEL_READ
            cout << "ReadOnlyWaveFileModel(" << m_model.objectName() << ")::fill inner loop: frame = " << frame << ", count = " << m_frameCount << ", blocksize " << readBlockSize << endl;
#endif

            if (updating && (frame + readBlockSize > m_frameCount)) {
                m_model.m_mutex.lock(); // must be locked on exiting loop
                break;
            }

            block = m_model.m_reader->getInterleavedFrames(frame, readBlockSize);

            sv_frame_t gotBlockSize = block.size() / channels;

            m_model.m_mutex.lock();

            for (sv_frame_t i = 0; i < gotBlockSize; ++i) {
                
                for (int ch = 0; ch < channels; ++ch) {

                    sv_frame_t index = channels * i + ch;
                    float sample = block[index];
                    
                    for (int cacheType = 0; cacheType < 2; ++cacheType) {
                        sv_frame_t rangeIndex = ch * 2 + cacheType;
                        range[rangeIndex].sample(sample);
                        means[rangeIndex] += fabsf(sample);
                    }
                }

                for (int cacheType = 0; cacheType < 2; ++cacheType) {

                    if (++count[cacheType] == cacheBlockSize[cacheType]) {
                        
                        for (int ch = 0; ch < int(channels); ++ch) {
                            int rangeIndex = ch * 2 + cacheType;
                            means[rangeIndex] = means[rangeIndex] / float(count[cacheType]);
                            range[rangeIndex].setAbsmean(means[rangeIndex]);
                            m_model.m_cache[cacheType].push_back(range[rangeIndex]);
                            range[rangeIndex] = Range();
                            means[rangeIndex] = 0.f;
                        }

                        count[cacheType] = 0;
                    }
                }
                
                ++frame;
            }

            if (m_model.m_exiting) break;
            m_fillExtent = frame;
        }

        m_model.m_mutex.unlock();
            
        first = false;
        if (m_model.m_exiting) break;
        if (updating) {
            sleep(1);
        }
    }

    if (!m_model.m_exiting) {

        QMutexLocker locker(&m_model.m_mutex);

        for (int cacheType = 0; cacheType < 2; ++cacheType) {

            if (count[cacheType] > 0) {

                for (int ch = 0; ch < int(channels); ++ch) {
                    int rangeIndex = ch * 2 + cacheType;
                    means[rangeIndex] = means[rangeIndex] / float(count[cacheType]);
                    range[rangeIndex].setAbsmean(means[rangeIndex]);
                    m_model.m_cache[cacheType].push_back(range[rangeIndex]);
                    range[rangeIndex] = Range();
                    means[rangeIndex] = 0.f;
                }

                count[cacheType] = 0;
            }
            
            const Range &rr = *m_model.m_cache[cacheType].begin();
            MUNLOCK(&rr, m_model.m_cache[cacheType].capacity() * sizeof(Range));
        }
    }
    
    delete[] means;
    delete[] range;

    m_fillExtent = m_frameCount;

#ifdef DEBUG_WAVE_FILE_MODEL        
    for (int cacheType = 0; cacheType < 2; ++cacheType) {
        SVCERR << "ReadOnlyWaveFileModel(" << m_model.objectName() << "): Cache type " << cacheType << " now contains " << m_model.m_cache[cacheType].size() << " ranges" << endl;
    }
#endif
}

void
ReadOnlyWaveFileModel::toXml(QTextStream &out,
                     QString indent,
                     QString extraAttributes) const
{
    Model::toXml(out, indent,
                 QString("type=\"wavefile\" file=\"%1\" %2")
                 .arg(encodeEntities(m_path)).arg(extraAttributes));
}

    
