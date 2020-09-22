/* -*- c-basic-offset: 4 indent-tabs-mode: nil -*-  vi:set ts=8 sts=4 sw=4: */

/*
    Sonic Visualiser
    An audio file viewer and annotation editor.
    Centre for Digital Music, Queen Mary, University of London.
    This file copyright 2007 QMUL.
    
    This program is free software; you can redistribute it and/or
    modify it under the terms of the GNU General Public License as
    published by the Free Software Foundation; either version 2 of the
    License, or (at your option) any later version.  See the file
    COPYING included with this distribution for more information.
*/

#include "AggregateWaveModel.h"

#include <iostream>

#include <QTextStream>

using namespace std;

//#define DEBUG_AGGREGATE_WAVE_FILE_MODEL 1

PowerOfSqrtTwoZoomConstraint
AggregateWaveModel::m_zoomConstraint;

AggregateWaveModel::AggregateWaveModel(ChannelSpecList channelSpecs) :
    m_components(channelSpecs)
{
    sv_samplerate_t overallRate = 0;

    for (int channel = 0; in_range_for(m_components, channel); ++channel) {

        auto model = ModelById::getAs<RangeSummarisableTimeValueModel>
            (m_components[channel].model);

        if (!model) {
            SVCERR << "AggregateWaveModel: WARNING: component for channel "
                   << channel << " is not found or is of wrong model type"
                   << endl;
            continue;
        }

        sv_samplerate_t rate = model->getSampleRate();

        if (!rate) {
            SVCERR << "AggregateWaveModel: WARNING: component for channel "
                   << channel << " reports zero sample rate" << endl;

        } else if (!overallRate) {

            overallRate = rate;

        } else if (rate != overallRate) {
            SVCERR << "AggregateWaveModel: WARNING: component for channel "
                   << channel << " has different sample rate from earlier "
                   << "channels (has " << rate << ", expected " << overallRate
                   << ")" << endl;
        }

        connect(model.get(), SIGNAL(modelChanged(ModelId)),
                this, SLOT(componentModelChanged(ModelId)));
        connect(model.get(), SIGNAL(modelChangedWithin(ModelId, sv_frame_t, sv_frame_t)),
                this, SLOT(componentModelChangedWithin(ModelId, sv_frame_t, sv_frame_t)));
        connect(model.get(), SIGNAL(completionChanged(ModelId)),
                this, SLOT(componentModelCompletionChanged(ModelId)));
    }
}

AggregateWaveModel::~AggregateWaveModel()
{
    SVDEBUG << "AggregateWaveModel::~AggregateWaveModel" << endl;
}

bool
AggregateWaveModel::isOK() const
{
    if (m_components.empty()) {
        return false;
    }
    for (const auto &c: m_components) {
        auto model = ModelById::get(c.model);
        if (!model || !model->isOK()) {
            return false;
        }
    }
    return true;
}

bool
AggregateWaveModel::isReady(int *completion) const
{
    if (completion) *completion = 100;

    bool ready = true;
    for (auto c: m_components) {
        int completionHere = 100;
        auto model = ModelById::get(c.model);
        if (!model) continue;
        if (!model->isReady(&completionHere)) {
            ready = false;
        }
        if (completion && completionHere < *completion) {
            *completion = completionHere;
        }
    }

#ifdef DEBUG_AGGREGATE_WAVE_FILE_MODEL
    SVDEBUG << "AggregateWaveModel(" << objectName()
            << ")::isReady: returning " << ready << endl;
#endif
    
    return ready;
}

sv_frame_t
AggregateWaveModel::getFrameCount() const
{
    sv_frame_t count = 0;
    for (auto c: m_components) {
        auto model = ModelById::get(c.model);
        if (!model) continue;
        sv_frame_t thisCount = model->getEndFrame() - model->getStartFrame();
        if (thisCount > count) count = thisCount;
    }
    return count;
}

int
AggregateWaveModel::getChannelCount() const
{
    return int(m_components.size());
}

sv_samplerate_t
AggregateWaveModel::getSampleRate() const
{
    if (m_components.empty()) return 0;
    auto model = ModelById::get(m_components.begin()->model);
    if (!model) return 0;
    return model->getSampleRate();
}

floatvec_t
AggregateWaveModel::getData(int channel, sv_frame_t start, sv_frame_t count) const
{
    if (m_components.empty()) return {};

    int ch0 = channel, ch1 = channel;
    if (channel == -1) {
        ch0 = 0;
        ch1 = getChannelCount()-1;
    } else if (!in_range_for(m_components, channel)) {
        return {};
    }

    floatvec_t result(count, 0.f);
    sv_frame_t longest = 0;
    
    for (int c = ch0; c <= ch1; ++c) {

        auto model = ModelById::getAs<RangeSummarisableTimeValueModel>
            (m_components[c].model);
        if (!model) continue;

        auto here = model->getData(m_components[c].channel, start, count);
        if (sv_frame_t(here.size()) > longest) {
            longest = sv_frame_t(here.size());
        }
        for (sv_frame_t i = 0; in_range_for(here, i); ++i) {
            result[i] += here[i];
        }
    }

    result.resize(longest);
    return result;
}

vector<floatvec_t>
AggregateWaveModel::getMultiChannelData(int fromchannel, int tochannel,
                                        sv_frame_t start, sv_frame_t count) const
{
    sv_frame_t min = count;

    vector<floatvec_t> result;

    for (int c = fromchannel; c <= tochannel; ++c) {
        auto here = getData(c, start, count);
        if (sv_frame_t(here.size()) < min) {
            min = sv_frame_t(here.size());
        }
        result.push_back(here);
    }

    if (min < count) {
        for (auto &v : result) v.resize(min);
    }
    
    return result;
}

int
AggregateWaveModel::getSummaryBlockSize(int desired) const
{
    //!!! complete
    return desired;
}
        
void
AggregateWaveModel::getSummaries(int, sv_frame_t, sv_frame_t,
                                 RangeBlock &, int &) const
{
    //!!! complete
}

AggregateWaveModel::Range
AggregateWaveModel::getSummary(int, sv_frame_t, sv_frame_t) const
{
    //!!! complete
    return Range();
}
        
int
AggregateWaveModel::getComponentCount() const
{
    return int(m_components.size());
}

AggregateWaveModel::ModelChannelSpec
AggregateWaveModel::getComponent(int c) const
{
    return m_components[c];
}

void
AggregateWaveModel::componentModelChanged(ModelId)
{
    emit modelChanged(getId());
}

void
AggregateWaveModel::componentModelChangedWithin(ModelId, sv_frame_t start, sv_frame_t end)
{
    emit modelChangedWithin(getId(), start, end);
}

void
AggregateWaveModel::componentModelCompletionChanged(ModelId)
{
    emit completionChanged(getId());
}

void
AggregateWaveModel::toXml(QTextStream &out,
                          QString indent,
                          QString extraAttributes) const
{
    QStringList componentStrings;
    for (const auto &c: m_components) {
        auto model = ModelById::get(c.model);
        if (!model) continue;
        componentStrings.push_back(QString("%1").arg(model->getExportId()));
    }
    Model::toXml(out, indent,
                 QString("type=\"aggregatewave\" components=\"%1\" %2")
                 .arg(componentStrings.join(","))
                 .arg(extraAttributes));
}

