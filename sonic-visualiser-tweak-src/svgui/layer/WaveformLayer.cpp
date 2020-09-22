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

#include "WaveformLayer.h"

#include "base/AudioLevel.h"
#include "view/View.h"
#include "base/Profiler.h"
#include "base/RangeMapper.h"
#include "base/Strings.h"

#include "ColourDatabase.h"
#include "PaintAssistant.h"

#include "data/model/WaveformOversampler.h"

#include <QPainter>
#include <QPixmap>
#include <QTextStream>

#include <iostream>
#include <cmath>

//#define DEBUG_WAVEFORM_PAINT 1
//#define DEBUG_WAVEFORM_PAINT_BY_PIXEL 1

using std::vector;

double
WaveformLayer::m_dBMin = -50.0;

WaveformLayer::WaveformLayer() :
    SingleColourLayer(),
    m_gain(1.0f),
    m_autoNormalize(false),
    m_showMeans(true),
    m_channelMode(SeparateChannels),
    m_channel(-1),
    m_channelCount(0),
    m_scale(LinearScale),
    m_middleLineHeight(0.5),
    m_aggressive(false),
    m_cache(nullptr),
    m_cacheValid(false)
{
}

WaveformLayer::~WaveformLayer()
{
    delete m_cache;
}

const ZoomConstraint *
WaveformLayer::getZoomConstraint() const
{
    auto model = ModelById::get(m_model);
    if (model) return model->getZoomConstraint();
    else return nullptr;
}

void
WaveformLayer::setModel(ModelId modelId)
{
    auto oldModel = ModelById::getAs<RangeSummarisableTimeValueModel>(m_model);
    auto newModel = ModelById::getAs<RangeSummarisableTimeValueModel>(modelId);

    if (!modelId.isNone() && !newModel) {
        throw std::logic_error("Not a RangeSummarisableTimeValueModel");
    }

    if (m_model == modelId) return;
    m_model = modelId;

    // NB newModel may legitimately be null
    
    m_cacheValid = false;
    
    bool channelsChanged = false;
    if (m_channel == -1) {
        if (!oldModel) {
            if (newModel) {
                channelsChanged = true;
            }
        } else {
            if (newModel &&
                oldModel->getChannelCount() != newModel->getChannelCount()) {
                channelsChanged = true;
            }
        }
    }

    if (newModel) {
        m_channelCount = newModel->getChannelCount();
        connectSignals(m_model);
    }
        
    emit modelReplaced();

    if (channelsChanged) emit layerParametersChanged();
}

Layer::PropertyList
WaveformLayer::getProperties() const
{
    PropertyList list = SingleColourLayer::getProperties();
    list.push_back("Scale");
    list.push_back("Gain");
    list.push_back("Normalize Visible Area");
    if (m_channelCount > 1 && m_channel == -1) {
        list.push_back("Channels");
    }

    return list;
}

QString
WaveformLayer::getPropertyLabel(const PropertyName &name) const
{
    if (name == "Scale") return tr("Scale");
    if (name == "Gain") return tr("Gain");
    if (name == "Normalize Visible Area") return tr("Normalize Visible Area");
    if (name == "Channels") return tr("Channels");
    return SingleColourLayer::getPropertyLabel(name);
}

QString
WaveformLayer::getPropertyIconName(const PropertyName &name) const
{
    if (name == "Normalize Visible Area") return "normalise";
    return "";
}

Layer::PropertyType
WaveformLayer::getPropertyType(const PropertyName &name) const
{
    if (name == "Gain") return RangeProperty;
    if (name == "Normalize Visible Area") return ToggleProperty;
    if (name == "Channels") return ValueProperty;
    if (name == "Scale") return ValueProperty;
    return SingleColourLayer::getPropertyType(name);
}

QString
WaveformLayer::getPropertyGroupName(const PropertyName &name) const
{
    if (name == "Gain" ||
        name == "Normalize Visible Area" ||
        name == "Scale") return tr("Scale");
    return QString();
}

int
WaveformLayer::getPropertyRangeAndValue(const PropertyName &name,
                                        int *min, int *max, int *deflt) const
{
    int val = 0;

    int garbage0, garbage1, garbage2;
    if (!min) min = &garbage0;
    if (!max) max = &garbage1;
    if (!deflt) deflt = &garbage2;

    if (name == "Gain") {

        *min = -50;
        *max = 50;
        *deflt = 0;

        val = int(lrint(log10(m_gain) * 20.0));
        if (val < *min) val = *min;
        if (val > *max) val = *max;

    } else if (name == "Normalize Visible Area") {

        val = (m_autoNormalize ? 1 : 0);
        *deflt = 0;

    } else if (name == "Channels") {

        *min = 0;
        *max = 2;
        *deflt = 0;
        if (m_channelMode == MixChannels) val = 1;
        else if (m_channelMode == MergeChannels) val = 2;
        else val = 0;

    } else if (name == "Scale") {

        *min = 0;
        *max = 2;
        *deflt = 0;

        val = (int)m_scale;

    } else {
        val = SingleColourLayer::getPropertyRangeAndValue(name, min, max, deflt);
    }

    return val;
}

QString
WaveformLayer::getPropertyValueLabel(const PropertyName &name,
                                    int value) const
{
    if (name == "Scale") {
        switch (value) {
        default:
        case 0: return tr("Linear");
        case 1: return tr("Meter");
        case 2: return tr("dB");
        }
    }
    if (name == "Channels") {
        switch (value) {
        default:
        case 0: return tr("Separate");
        case 1: return tr("Mean");
        case 2: return tr("Butterfly");
        }
    }
    return SingleColourLayer::getPropertyValueLabel(name, value);
}

RangeMapper *
WaveformLayer::getNewPropertyRangeMapper(const PropertyName &name) const
{
    if (name == "Gain") {
        return new LinearRangeMapper(-50, 50, -25, 25, tr("dB"));
    }
    return nullptr;
}

void
WaveformLayer::setProperty(const PropertyName &name, int value)
{
    if (name == "Gain") {
        setGain(float(pow(10, float(value)/20.0)));
    } else if (name == "Normalize Visible Area") {
        setAutoNormalize(value ? true : false);
    } else if (name == "Channels") {
        if (value == 1) setChannelMode(MixChannels);
        else if (value == 2) setChannelMode(MergeChannels);
        else setChannelMode(SeparateChannels);
    } else if (name == "Scale") {
        switch (value) {
        default:
        case 0: setScale(LinearScale); break;
        case 1: setScale(MeterScale); break;
        case 2: setScale(dBScale); break;
        }
    } else {
        SingleColourLayer::setProperty(name, value);
    }
}

void
WaveformLayer::setGain(float gain)
{
    if (m_gain == gain) return;
    m_gain = gain;
    m_cacheValid = false;
    emit layerParametersChanged();
    emit verticalZoomChanged();
}

void
WaveformLayer::setAutoNormalize(bool autoNormalize)
{
    if (m_autoNormalize == autoNormalize) return;
    m_autoNormalize = autoNormalize;
    m_cacheValid = false;
    emit layerParametersChanged();
}

void
WaveformLayer::setShowMeans(bool showMeans)
{
    if (m_showMeans == showMeans) return;
    m_showMeans = showMeans;
    m_cacheValid = false;
    emit layerParametersChanged();
}

void
WaveformLayer::setChannelMode(ChannelMode channelMode)
{
    if (m_channelMode == channelMode) return;
    m_channelMode = channelMode;
    m_cacheValid = false;
    emit layerParametersChanged();
}

void
WaveformLayer::setChannel(int channel)
{
//    SVDEBUG << "WaveformLayer::setChannel(" << channel << ")" << endl;

    if (m_channel == channel) return;
    m_channel = channel;
    m_cacheValid = false;
    emit layerParametersChanged();
}

void
WaveformLayer::setScale(Scale scale)
{
    if (m_scale == scale) return;
    m_scale = scale;
    m_cacheValid = false;
    emit layerParametersChanged();
}

void
WaveformLayer::setMiddleLineHeight(double height)
{
    if (m_middleLineHeight == height) return;
    m_middleLineHeight = height;
    m_cacheValid = false;
    emit layerParametersChanged();
}

void
WaveformLayer::setAggressiveCacheing(bool aggressive)
{
    if (m_aggressive == aggressive) return;
    m_aggressive = aggressive;
    m_cacheValid = false;
    emit layerParametersChanged();
}

int
WaveformLayer::getCompletion(LayerGeometryProvider *) const
{
    int completion = 100;
    auto model = ModelById::getAs<RangeSummarisableTimeValueModel>(m_model);
    if (!model || !model->isOK()) return completion;
    if (model->isReady(&completion)) return 100;
    return completion;
}

bool
WaveformLayer::getValueExtents(double &min, double &max,
                               bool &log, QString &unit) const
{
    // This function serves two purposes. It's used to gather min and
    // max values for a given unit, for cases where there are
    // auto-align layers out there that aren't providing extents of
    // their own and that have no specific other layer with display
    // extents to align to. It's also used to determine whether a
    // layer might be capable of drawing a scale for itself.
    //
    // This makes our situation a bit tricky. There's no point in
    // returning extents that anyone else might try to align to unless
    // we have a scale that they can actually calculate with, which is
    // only the case for certain linear/log arrangements (see
    // getDisplayExtents - we can test this case by checking whether
    // getDisplayExtents returns successfully).
    //
    // However, there is a point in returning something that indicates
    // our own capacity to draw a scale. If we don't do that, then we
    // won't get a scale at all if e.g. we have a time-instant layer
    // on top (or something else that doesn't care about the y axis).
    //
    // Our "solution" to this is to always return true and our
    // extents, but with an empty unit unless we have the sort of nice
    // linear/log scale that others can actually align to.
    //
    // It might be better to respond to capability requests - can draw
    // scale, care about scale, can align unit X etc.
    
    if (getDisplayExtents(min, max)) {
        unit = "V";
        log = (m_scale == dBScale);
    } else {
        max = 1.0;
        min = -1.0;
        log = false;
        unit = "";
    }

    return true;
}

bool
WaveformLayer::getDisplayExtents(double &min, double &max) const
{
    // If we have a single channel visible and either linear or log
    // (dB) scale, then we have a continuous scale that runs from -1
    // to 1 or -dBMin to 0 and we can offer it as an alignment target
    // for other layers with the same unit. We can also do this in
    // butterfly mode, but only with linear scale. Otherwise no.

    if (m_scale == MeterScale) {
        return false;
    }
    
    if (m_channelCount > 1) {
        if (m_channelMode == SeparateChannels) {
            return false;
        }
        if (m_channelMode == MergeChannels && m_scale != LinearScale) {
            return false;
        }
    }

    if (m_scale == LinearScale) {
        max = 1.0;
        min = -1.0;
        return true;
    }

    if (m_scale == dBScale) {
        max = 1.0;
        min = AudioLevel::dB_to_multiplier(m_dBMin);
        return true;
    }

    return false;
}

double
WaveformLayer::dBscale(double sample, int m) const
{
    if (sample < 0.0) return dBscale(-sample, m);
    double dB = AudioLevel::multiplier_to_dB(sample);
    if (dB < m_dBMin) return 0;
    if (dB > 0.0) return m;
    return ((dB - m_dBMin) * m) / (-m_dBMin);
}

int
WaveformLayer::getChannelArrangement(int &min, int &max,
                                     bool &merging, bool &mixing)
    const
{
    int channels = m_channelCount;
    if (channels == 0) return 0;

    int rawChannels = channels;

    if (m_channel == -1) {
        min = 0;
        if (m_channelMode == MergeChannels ||
            m_channelMode == MixChannels) {
            max = 0;
            channels = 1;
        } else {
            max = channels - 1;
        }
    } else {
        min = m_channel;
        max = m_channel;
        rawChannels = 1;
        channels = 1;
    }

    // "Merging" -> "butterfly mode" - use +ve side of "waveform" for
    // channel 0 and -ve side for channel 1. If we only have one
    // channel, we still do this but just duplicate channel 0 onto
    // channel 1 - this is the only way to get a classic-looking
    // waveform with meter or db scale from a single-channel file,
    // although it isn't currently exposed in the SV UI
    merging = (m_channelMode == MergeChannels);

    // "Mixing" -> produce a single waveform from the mean of the
    // channels. Unlike merging, this really does only make sense if
    // we have >1 channel.
    mixing = (m_channelMode == MixChannels && rawChannels > 1);

//    SVDEBUG << "WaveformLayer::getChannelArrangement: min " << min << ", max " << max << ", merging " << merging << ", channels " << channels << endl;

    return channels;
}    

bool
WaveformLayer::isLayerScrollable(const LayerGeometryProvider *) const
{
    return !m_autoNormalize;
}

static float meterdbs[] = { -40, -30, -20, -15, -10,
                            -5, -3, -2, -1, -0.5, 0 };

bool
WaveformLayer::getSourceFramesForX(LayerGeometryProvider *v,
                                   int x, int modelZoomLevel,
                                   sv_frame_t &f0, sv_frame_t &f1) const
{
    auto model = ModelById::getAs<RangeSummarisableTimeValueModel>(m_model);
    if (!model) return false;
    
    sv_frame_t viewFrame = v->getFrameForX(x);
    if (viewFrame < 0) {
        f0 = 0;
        f1 = 0;
        return false;
    }

    f0 = viewFrame;
    f0 = f0 / modelZoomLevel;
    f0 = f0 * modelZoomLevel;

    if (v->getZoomLevel().zone == ZoomLevel::PixelsPerFrame) {
        f1 = f0 + 1;
    } else {
        viewFrame = v->getFrameForX(x + 1);
        f1 = viewFrame;
        f1 = f1 / modelZoomLevel;
        f1 = f1 * modelZoomLevel;
    }
    
    return (f0 < model->getEndFrame());
}

float
WaveformLayer::getNormalizeGain(LayerGeometryProvider *v, int channel) const
{
    auto model = ModelById::getAs<RangeSummarisableTimeValueModel>(m_model);
    if (!model) return 0.f;
    
    sv_frame_t startFrame = v->getStartFrame();
    sv_frame_t endFrame = v->getEndFrame();

    sv_frame_t modelStart = model->getStartFrame();
    sv_frame_t modelEnd = model->getEndFrame();
    
    sv_frame_t rangeStart, rangeEnd;
            
    if (startFrame < modelStart) rangeStart = modelStart;
    else rangeStart = startFrame;

    if (endFrame < 0) rangeEnd = 0;
    else if (endFrame > modelEnd) rangeEnd = modelEnd;
    else rangeEnd = endFrame;

    if (rangeEnd < rangeStart) rangeEnd = rangeStart;

    RangeSummarisableTimeValueModel::Range range =
        model->getSummary(channel, rangeStart, rangeEnd - rangeStart);

    int minChannel = 0, maxChannel = 0;
    bool mergingChannels = false, mixingChannels = false;

    (void)getChannelArrangement(minChannel, maxChannel,
                                mergingChannels, mixingChannels);

    if (mergingChannels || mixingChannels) {
        if (m_channelCount > 1) {
            RangeSummarisableTimeValueModel::Range otherRange =
                model->getSummary(1, rangeStart, rangeEnd - rangeStart);
            range.setMax(std::max(range.max(), otherRange.max()));
            range.setMin(std::min(range.min(), otherRange.min()));
            range.setAbsmean(std::min(range.absmean(), otherRange.absmean()));
        }
    }

    return float(1.0 / std::max(fabs(range.max()), fabs(range.min())));
}

void
WaveformLayer::paint(LayerGeometryProvider *v, QPainter &viewPainter, QRect rect) const
{
    auto model = ModelById::getAs<RangeSummarisableTimeValueModel>(m_model);
    if (!model || !model->isOK()) {
        return;
    }
  
    ZoomLevel zoomLevel = v->getZoomLevel();

#ifdef DEBUG_WAVEFORM_PAINT
    Profiler profiler("WaveformLayer::paint", true);
    SVCERR << "WaveformLayer::paint (" << rect.x() << "," << rect.y()
              << ") [" << rect.width() << "x" << rect.height() << "]: zoom " << zoomLevel << endl;
#endif

    int channels = 0, minChannel = 0, maxChannel = 0;
    bool mergingChannels = false, mixingChannels = false;

    channels = getChannelArrangement(minChannel, maxChannel,
                                     mergingChannels, mixingChannels);
    if (channels == 0) return;

    int w = v->getPaintWidth();
    int h = v->getPaintHeight();

    QPainter *paint;

    if (m_aggressive) {

#ifdef DEBUG_WAVEFORM_PAINT
        SVCERR << "WaveformLayer::paint: aggressive is true" << endl;
#endif

        using namespace std::rel_ops;
        
        if (m_cacheValid && (zoomLevel != m_cacheZoomLevel)) {
            m_cacheValid = false;
        }

        if (!m_cache || m_cache->width() != w || m_cache->height() != h) {
#ifdef DEBUG_WAVEFORM_PAINT
            if (m_cache) {
                SVCERR << "WaveformLayer::paint: cache size " << m_cache->width() << "x" << m_cache->height() << " differs from view size " << w << "x" << h << ": regenerating aggressive cache" << endl;
            }
#endif
            delete m_cache;
            m_cache = new QPixmap(w, h);
            m_cacheValid = false;
        }

        if (m_cacheValid) {
            viewPainter.drawPixmap(rect, *m_cache, rect);
            return;
        }

        paint = new QPainter(m_cache);

        paint->setPen(Qt::NoPen);
        paint->setBrush(getBackgroundQColor(v));
        paint->drawRect(rect);

        paint->setPen(getForegroundQColor(v));
        paint->setBrush(Qt::NoBrush);

    } else {
        paint = &viewPainter;
    }

    paint->setRenderHint(QPainter::Antialiasing, true);

    if (m_middleLineHeight != 0.5) {
        paint->save();
        double space = m_middleLineHeight * 2;
        if (space > 1.0) space = 2.0 - space;
        double yt = h * (m_middleLineHeight - space/2);
        paint->translate(QPointF(0, yt));
        paint->scale(1.0, space);
    }

    int x0 = 0, x1 = w - 1;

    x0 = rect.left();
    x1 = rect.right();

    if (x0 > 0) {
        rect.adjust(-1, 0, 0, 0);
        x0 = rect.left();
    }

    if (x1 < w) {
        rect.adjust(0, 0, 1, 0);
        x1 = rect.right();
    }

    // Our zoom level may differ from that at which the underlying
    // model has its blocks.

    // Each pixel within our visible range must always draw from
    // exactly the same set of underlying audio frames, no matter what
    // the range being drawn is.  And that set of underlying frames
    // must remain the same when we scroll one or more pixels left or
    // right.

    int desiredBlockSize = 1;
    if (zoomLevel.zone == ZoomLevel::FramesPerPixel) {
        desiredBlockSize = zoomLevel.level;
    }
    int blockSize = model->getSummaryBlockSize(desiredBlockSize);

    sv_frame_t frame0;
    sv_frame_t frame1;
    sv_frame_t spare;

    getSourceFramesForX(v, x0, blockSize, frame0, spare);
    getSourceFramesForX(v, x1, blockSize, spare, frame1);
    
#ifdef DEBUG_WAVEFORM_PAINT
    SVCERR << "Painting waveform from " << frame0 << " to " << frame1 << " (" << (x1-x0+1) << " pixels at zoom " << zoomLevel << " and model zoom " << blockSize << ")" <<  endl;
#endif

    m_effectiveGains.clear();
    while ((int)m_effectiveGains.size() <= maxChannel) {
        m_effectiveGains.push_back(m_gain);
    }
    if (m_autoNormalize) {
        for (int ch = minChannel; ch <= maxChannel; ++ch) {
            m_effectiveGains[ch] = getNormalizeGain(v, ch);
        }
    }

    RangeVec ranges;

    if (v->getZoomLevel().zone == ZoomLevel::FramesPerPixel) {
        getSummaryRanges(minChannel, maxChannel,
                         mixingChannels || mergingChannels,
                         frame0, frame1,
                         blockSize, ranges);
    } else {
        getOversampledRanges(minChannel, maxChannel,
                             mixingChannels || mergingChannels,
                             frame0, frame1,
                             v->getZoomLevel().level, ranges);
    }

    if (!ranges.empty()) {
        for (int ch = minChannel; ch <= maxChannel; ++ch) {
            paintChannel(v, paint, rect, ch, ranges, blockSize,
                         frame0, frame1);
        }
    }
    
    if (m_middleLineHeight != 0.5) {
        paint->restore();
    }

    if (m_aggressive) {
        if (model->isReady() && rect == v->getPaintRect()) {
            m_cacheValid = true;
            m_cacheZoomLevel = zoomLevel;
        }
        paint->end();
        delete paint;
        viewPainter.drawPixmap(rect, *m_cache, rect);
    }
}

void
WaveformLayer::getSummaryRanges(int minChannel, int maxChannel,
                                bool mixingOrMerging,
                                sv_frame_t frame0, sv_frame_t frame1,
                                int blockSize, RangeVec &ranges)
    const
{
    auto model = ModelById::getAs<RangeSummarisableTimeValueModel>(m_model);
    if (!model) return;
    
    for (int ch = minChannel; ch <= maxChannel; ++ch) {
        ranges.push_back({});
        model->getSummaries(ch, frame0, frame1 - frame0,
                              ranges[ch - minChannel], blockSize);
#ifdef DEBUG_WAVEFORM_PAINT
            SVCERR << "channel " << ch << ": " << ranges[ch - minChannel].size() << " ranges from " << frame0 << " to " << frame1 << " at zoom level " << blockSize << endl;
#endif
    }
    
    if (mixingOrMerging) {
        if (minChannel != 0 || maxChannel != 0) {
            throw std::logic_error("Internal error: min & max channels should be 0 when merging or mixing all channels");
        } else if (m_channelCount > 1) {
            ranges.push_back({});
            model->getSummaries
                (1, frame0, frame1 - frame0, ranges[1], blockSize);
        } else {
            ranges.push_back(ranges[0]);
        }
    }
}

void
WaveformLayer::getOversampledRanges(int minChannel, int maxChannel,
                                    bool mixingOrMerging,
                                    sv_frame_t frame0, sv_frame_t frame1,
                                    int oversampleBy, RangeVec &ranges)
    const
{
    auto model = ModelById::getAs<RangeSummarisableTimeValueModel>(m_model);
    if (!model) return;
    
    if (mixingOrMerging) {
        if (minChannel != 0 || maxChannel != 0) {
            throw std::logic_error("Internal error: min & max channels should be 0 when merging or mixing all channels");
        }
        if (m_channelCount > 1) {
            // call back on self for the individual channels with
            // mixingOrMerging false
            getOversampledRanges
                (0, 1, false, frame0, frame1, oversampleBy, ranges);
            return;
        } else {
            // call back on self for a single channel, then duplicate
            getOversampledRanges
                (0, 0, false, frame0, frame1, oversampleBy, ranges);
            ranges.push_back(ranges[0]);
            return;
        }
    }
    
    // These frame values, tail length, etc variables are at the model
    // sample rate, not the oversampled rate

    sv_frame_t tail = 16;
    sv_frame_t startFrame = model->getStartFrame();
    sv_frame_t endFrame = model->getEndFrame();

    sv_frame_t rf0 = frame0 - tail;
    if (rf0 < startFrame) {
        rf0 = 0;
    }

    sv_frame_t rf1 = frame1 + tail;
    if (rf1 >= endFrame) {
        rf1 = endFrame - 1;
    }
    if (rf1 <= rf0) {
        SVCERR << "WARNING: getOversampledRanges: rf1 (" << rf1 << ") <= rf0 ("
               << rf0 << ")" << endl;
        return;
    }
    
    for (int ch = minChannel; ch <= maxChannel; ++ch) {
        floatvec_t oversampled = WaveformOversampler::getOversampledData
            (*model, ch, frame0, frame1 - frame0, oversampleBy);
        RangeSummarisableTimeValueModel::RangeBlock rr;
        for (float v: oversampled) {
            RangeSummarisableTimeValueModel::Range r;
            r.sample(v);
            rr.push_back(r);
        }
        ranges.push_back(rr);

#ifdef DEBUG_WAVEFORM_PAINT
        SVCERR << "getOversampledRanges: " << frame0 << " -> " << frame1
               << " (" << frame1 - frame0 << "-frame range) at ratio "
               << oversampleBy << " with tail " << tail
               << " -> got " << oversampled.size()
               << " oversampled values for channel " << ch
               << ", from which returning " << rr.size() << " ranges" << endl;
#endif    
    }
    
    return;
}

void
WaveformLayer::paintChannel(LayerGeometryProvider *v,
                            QPainter *paint,
                            QRect rect, int ch,
                            const RangeVec &ranges,
                            int blockSize,
                            sv_frame_t frame0,
                            sv_frame_t frame1)
    const
{
    auto model = ModelById::getAs<RangeSummarisableTimeValueModel>(m_model);
    if (!model) return;
    
    int x0 = rect.left();
    int y0 = rect.top();

    int x1 = rect.right();
    int y1 = rect.bottom();

    int h = v->getPaintHeight();

    int channels = 0, minChannel = 0, maxChannel = 0;
    bool mergingChannels = false, mixingChannels = false;

    channels = getChannelArrangement(minChannel, maxChannel,
                                     mergingChannels, mixingChannels);
    if (channels == 0) return;

    QColor baseColour = getBaseQColor();
    QColor midColour = baseColour;
    
    if (midColour == Qt::black) {
        midColour = Qt::gray;
    } else if (v->hasLightBackground()) {
        midColour = midColour.lighter(150);
    } else {
        midColour = midColour.lighter(50);
    }

    double gain = m_effectiveGains[ch];

    int m = (h / channels) / 2;
    int my = m + (((ch - minChannel) * h) / channels);

#ifdef DEBUG_WAVEFORM_PAINT        
    SVCERR << "ch = " << ch << ", channels = " << channels << ", m = " << m << ", my = " << my << ", h = " << h << endl;
#endif

    if (my - m > y1 || my + m < y0) return;

    if ((m_scale == dBScale || m_scale == MeterScale) &&
        m_channelMode != MergeChannels) {
        m = (h / channels);
        my = m + (((ch - minChannel) * h) / channels);
    }

    // Horizontal axis along middle
    paint->setPen(QPen(midColour, 0));
    paint->drawLine(QPointF(x0, my + 0.5), QPointF(x1, my + 0.5));

    paintChannelScaleGuides(v, paint, rect, ch);
  
    int rangeix = ch - minChannel;

#ifdef DEBUG_WAVEFORM_PAINT
    SVCERR << "paint channel " << ch << ": frame0 = " << frame0 << ", frame1 = " << frame1 << ", blockSize = " << blockSize << ", have " << ranges.size() << " range blocks of which ours is index " << rangeix << " with " << ranges[rangeix].size() << " ranges in it" << endl;
#else
    (void)frame1; // not actually used
#endif

    QPainterPath waveformPath;
    QPainterPath meanPath;
    QPainterPath clipPath;
    vector<QPointF> individualSamplePoints;

    bool firstPoint = true;
    double prevRangeBottom = 0, prevRangeTop = 0;
    
    for (int x = x0; x <= x1; ++x) {

        sv_frame_t f0, f1;
        sv_frame_t i0, i1;

        bool showIndividualSample = false;
        
        if (v->getZoomLevel().zone == ZoomLevel::FramesPerPixel) {
            if (!getSourceFramesForX(v, x, blockSize, f0, f1)) {
                continue;
            }
            f1 = f1 - 1;
            i0 = (f0 - frame0) / blockSize;
            i1 = (f1 - frame0) / blockSize;
        } else {
            int oversampleBy = v->getZoomLevel().level;
            f0 = f1 = v->getFrameForX(x);
            int xf0 = v->getXForFrame(f0);
            showIndividualSample = (x == xf0);
            i0 = i1 = (f0 - frame0) * oversampleBy + (x - xf0);
        }

        if (f0 < frame0) {
            // Not an error, this simply occurs when painting the
            // start of a signal in PixelsPerFrame zone
            continue;
        }

#ifdef DEBUG_WAVEFORM_PAINT_BY_PIXEL
        SVCERR << "WaveformLayer::paint: pixel " << x << ": i0 " << i0 << " (f " << f0 << "), i1 " << i1 << " (f " << f1 << ")" << endl;
#endif

        if (i1 > i0 + 1) {
            SVCERR << "WaveformLayer::paint: ERROR: i1 " << i1 << " > i0 " << i0 << " plus one (zoom = " << v->getZoomLevel() << ", model zoom = " << blockSize << ")" << endl;
        }

        const auto &r = ranges[rangeix];
        RangeSummarisableTimeValueModel::Range range;
            
        if (in_range_for(r, i0)) {

            range = r[i0];

            if (i1 > i0 && in_range_for(r, i1)) {
                range.setMax(std::max(range.max(), r[i1].max()));
                range.setMin(std::min(range.min(), r[i1].min()));
                range.setAbsmean((range.absmean() + r[i1].absmean()) / 2);
            }

        } else {
#ifdef DEBUG_WAVEFORM_PAINT
            SVCERR << "No (or not enough) ranges for index i0 = " << i0 << " (there are " << r.size() << " range(s))" << endl;
#endif
            continue;
        }

        double rangeBottom = 0, rangeTop = 0, meanBottom = 0, meanTop = 0;

        if (mergingChannels && ranges.size() > 1) {

            const auto &other = ranges[1];
            
            if (in_range_for(other, i0)) {

                range.setMax(fabsf(range.max()));
                range.setMin(-fabsf(other[i0].max()));
                range.setAbsmean
                    ((range.absmean() + other[i0].absmean()) / 2);

                if (i1 > i0 && in_range_for(other, i1)) {
                    // let's not concern ourselves about the mean
                    range.setMin(std::min(range.min(),
                                          -fabsf(other[i1].max())));
                }
            }

        } else if (mixingChannels && ranges.size() > 1) {

            const auto &other = ranges[1];
            
            if (in_range_for(other, i0)) {

                range.setMax((range.max() + other[i0].max()) / 2);
                range.setMin((range.min() + other[i0].min()) / 2);
                range.setAbsmean((range.absmean() + other[i0].absmean()) / 2);
            }
        }

        switch (m_scale) {

        case LinearScale:
            rangeBottom = range.min() * gain * m;
            rangeTop    = range.max() * gain * m;
            meanBottom  = range.absmean() * gain * (-m);
            meanTop     = range.absmean() * gain * m;
            break;

        case dBScale:
            if (!mergingChannels) {
                double db0 = dBscale(range.min() * gain, m);
                double db1 = dBscale(range.max() * gain, m);
                rangeTop = std::max(db0, db1);
                meanTop = std::min(db0, db1);
                if (mixingChannels) rangeBottom = meanTop;
                else rangeBottom = dBscale(range.absmean() * gain, m);
                meanBottom = rangeBottom;
            } else {
                rangeBottom = -dBscale(range.min() * gain, m);
                rangeTop = dBscale(range.max() * gain, m);
                meanBottom = -dBscale(range.absmean() * gain, m);
                meanTop = dBscale(range.absmean() * gain, m);
            }
            break;

        case MeterScale:
            if (!mergingChannels) {
                double r0 = std::abs(AudioLevel::multiplier_to_preview
                                 (range.min() * gain, m));
                double r1 = std::abs(AudioLevel::multiplier_to_preview
                                 (range.max() * gain, m));
                rangeTop = std::max(r0, r1);
                meanTop = std::min(r0, r1);
                if (mixingChannels) rangeBottom = meanTop;
                else rangeBottom = AudioLevel::multiplier_to_preview
                         (range.absmean() * gain, m);
                meanBottom  = rangeBottom;
            } else {
                rangeBottom = -AudioLevel::multiplier_to_preview
                    (range.min() * gain, m);
                rangeTop = AudioLevel::multiplier_to_preview
                    (range.max() * gain, m);
                meanBottom = -AudioLevel::multiplier_to_preview
                    (range.absmean() * gain, m);
                meanTop = AudioLevel::multiplier_to_preview
                    (range.absmean() * gain, m);
            }
            break;
        }

        rangeBottom = my - rangeBottom;
        rangeTop = my - rangeTop;
        meanBottom = my - meanBottom;
        meanTop = my - meanTop;

        bool clipped = false;

        if (rangeTop < my - m) { rangeTop = my - m; }
        if (rangeTop > my + m) { rangeTop = my + m; }
        if (rangeBottom < my - m) { rangeBottom = my - m; }
        if (rangeBottom > my + m) { rangeBottom = my + m; }

        if (range.max() <= -1.0 || range.max() >= 1.0) {
            clipped = true;
        }
            
        bool drawMean = m_showMeans;

        meanTop = meanTop - 0.5;
        meanBottom = meanBottom + 0.5;
        
        if (meanTop <= rangeTop + 1.0) {
            meanTop = rangeTop + 1.0;
        }
        if (meanBottom >= rangeBottom - 1.0 && m_scale == LinearScale) {
            meanBottom = rangeBottom - 1.0;
        }
        if (meanTop > meanBottom - 1.0) {
            drawMean = false;
        }

#ifdef DEBUG_WAVEFORM_PAINT_BY_PIXEL
        SVCERR << "range " << rangeBottom << " -> " << rangeTop << ", means " << meanBottom << " -> " << meanTop << ", raw range " << range.min() << " -> " << range.max() << endl;
#endif

        double rangeMiddle = (rangeTop + rangeBottom) / 2.0;
        bool trivialRange = (fabs(rangeTop - rangeBottom) < 1.0);
        double px = x + 0.5;
        
        if (showIndividualSample) {
            individualSamplePoints.push_back(QPointF(px, rangeTop));
            if (!trivialRange) {
                // common e.g. in "butterfly" merging mode
                individualSamplePoints.push_back(QPointF(px, rangeBottom));
            }
        }

        bool contiguous = true;
        if (rangeTop > prevRangeBottom + 0.5 ||
            rangeBottom < prevRangeTop - 0.5) {
            contiguous = false;
        }
        
        if (firstPoint || (contiguous && !trivialRange)) {
            waveformPath.moveTo(QPointF(px, rangeTop));
            waveformPath.lineTo(QPointF(px, rangeBottom));
            waveformPath.moveTo(QPointF(px, rangeMiddle));
        } else {
            waveformPath.lineTo(QPointF(px, rangeMiddle));
            if (!trivialRange) {
                waveformPath.lineTo(QPointF(px, rangeTop));
                waveformPath.lineTo(QPointF(px, rangeBottom));
                waveformPath.lineTo(QPointF(px, rangeMiddle));
            }
        }

        firstPoint = false;
        prevRangeTop = rangeTop;
        prevRangeBottom = rangeBottom;
        
        if (drawMean) {
            meanPath.moveTo(QPointF(px, meanBottom));
            meanPath.lineTo(QPointF(px, meanTop));
        }

        if (clipped) {
            if (trivialRange) {
                clipPath.moveTo(QPointF(px, rangeMiddle));
                clipPath.lineTo(QPointF(px+1, rangeMiddle));
            } else {
                clipPath.moveTo(QPointF(px, rangeBottom));
                clipPath.lineTo(QPointF(px, rangeTop));
            }
        }
    }

    double penWidth = 1.0;
    if (v->getZoomLevel().zone == ZoomLevel::FramesPerPixel) {
        penWidth = 0.0;
    }
    
    if (model->isReady()) {
        paint->setPen(QPen(baseColour, penWidth));
    } else {
        paint->setPen(QPen(midColour, penWidth));
    }
    paint->drawPath(waveformPath);

    if (!clipPath.isEmpty()) {
        paint->save();
        paint->setPen(QPen(ColourDatabase::getInstance()->
                           getContrastingColour(m_colour), penWidth));
        paint->drawPath(clipPath);
        paint->restore();
    }

    if (!meanPath.isEmpty()) {
        paint->save();
        paint->setPen(QPen(midColour, penWidth));
        paint->drawPath(meanPath);
        paint->restore();
    }
    
    if (!individualSamplePoints.empty()) {
        double sz = v->scaleSize(2.0);
        if (v->getZoomLevel().zone == ZoomLevel::PixelsPerFrame) {
            if (v->getZoomLevel().level < 10) {
                sz = v->scaleSize(1.2);
            }
        }
        paint->save();
        paint->setPen(QPen(baseColour, penWidth));
        for (QPointF p: individualSamplePoints) {
            paint->drawRect(QRectF(p.x() - sz/2, p.y() - sz/2, sz, sz));
        }
        paint->restore();
    }
}

void
WaveformLayer::paintChannelScaleGuides(LayerGeometryProvider *v,
                                       QPainter *paint,
                                       QRect rect,
                                       int ch) const
{
    int x0 = rect.left();
    int x1 = rect.right();

    int n = 10;
    int py = -1;

    double gain = m_effectiveGains[ch];
        
    if (v->hasLightBackground() &&
        v->getViewManager() &&
        v->getViewManager()->shouldShowScaleGuides()) {

        paint->setPen(QColor(240, 240, 240));

        for (int i = 1; i < n; ++i) {
                
            double val = 0.0, nval = 0.0;

            switch (m_scale) {

            case LinearScale:
                val = (i * gain) / n;
                if (i > 0) nval = -val;
                break;

            case MeterScale:
                val = AudioLevel::dB_to_multiplier(meterdbs[i]) * gain;
                break;

            case dBScale:
                val = AudioLevel::dB_to_multiplier(-(10*n) + i * 10) * gain;
                break;
            }

            if (val < -1.0 || val > 1.0) continue;

            int y = getYForValue(v, val, ch);

            if (py >= 0 && abs(y - py) < 10) continue;
            else py = y;

            int ny = y;
            if (nval != 0.0) {
                ny = getYForValue(v, nval, ch);
            }

            paint->drawLine(x0, y, x1, y);
            if (ny != y) {
                paint->drawLine(x0, ny, x1, ny);
            }
        }
    }
}

QString
WaveformLayer::getFeatureDescription(LayerGeometryProvider *v, QPoint &pos) const
{
    int x = pos.x();

    auto model = ModelById::getAs<RangeSummarisableTimeValueModel>(m_model);
    if (!model || !model->isOK()) return "";

    ZoomLevel zoomLevel = v->getZoomLevel();

    int desiredBlockSize = 1;
    if (zoomLevel.zone == ZoomLevel::FramesPerPixel) {
        desiredBlockSize = zoomLevel.level;
    }

    int blockSize = model->getSummaryBlockSize(desiredBlockSize);

    sv_frame_t f0, f1;
    if (!getSourceFramesForX(v, x, blockSize, f0, f1)) return "";
    
    QString text;

    RealTime rt0 = RealTime::frame2RealTime(f0, model->getSampleRate());
    RealTime rt1 = RealTime::frame2RealTime(f1, model->getSampleRate());

    if (f1 != f0 + 1 && (rt0.sec != rt1.sec || rt0.msec() != rt1.msec())) {
        text += tr("Time:\t%1 - %2")
            .arg(rt0.toText(true).c_str())
            .arg(rt1.toText(true).c_str());
    } else {
        text += tr("Time:\t%1")
            .arg(rt0.toText(true).c_str());
    }

    int channels = 0, minChannel = 0, maxChannel = 0;
    bool mergingChannels = false, mixingChannels = false;

    channels = getChannelArrangement(minChannel, maxChannel,
                                     mergingChannels, mixingChannels);
    if (channels == 0) return "";

    for (int ch = minChannel; ch <= maxChannel; ++ch) {

        RangeSummarisableTimeValueModel::RangeBlock ranges;
        model->getSummaries(ch, f0, f1 - f0, ranges, blockSize);

        if (ranges.empty()) continue;
        
        RangeSummarisableTimeValueModel::Range range = ranges[0];
        
        QString label = tr("Level:");
        if (minChannel != maxChannel) {
            if (ch == 0) label = tr("Left:");
            else if (ch == 1) label = tr("Right:");
            else label = tr("Channel %1").arg(ch + 1);
        }

        bool singleValue = false;
        double min, max;

        if (fabs(range.min()) < 0.01) {
            min = range.min();
            max = range.max();
            singleValue = (min == max);
        } else {
            int imin = int(lrint(range.min() * 10000));
            int imax = int(lrint(range.max() * 10000));
            singleValue = (imin == imax);
            min = double(imin)/10000;
            max = double(imax)/10000;
        }

        int db = int(AudioLevel::multiplier_to_dB(std::max(fabsf(range.min()),
                                                           fabsf(range.max())))
                     * 100);

        if (!singleValue) {
            text += tr("\n%1\t%2 - %3 (%4 dB peak)")
                .arg(label).arg(min).arg(max).arg(double(db)/100);
        } else {
            text += tr("\n%1\t%2 (%3 dB peak)")
                .arg(label).arg(min).arg(double(db)/100);
        }
    }

    return text;
}

int
WaveformLayer::getYForValue(const LayerGeometryProvider *v, double value, int channel) const
{
    int channels = 0, minChannel = 0, maxChannel = 0;
    bool mergingChannels = false, mixingChannels = false;

    channels = getChannelArrangement(minChannel, maxChannel,
                                     mergingChannels, mixingChannels);
    if (channels == 0) return 0;
    if (maxChannel < minChannel || channel < minChannel) return 0;

    int h = v->getPaintHeight();
    int m = (h / channels) / 2;
        
    if ((m_scale == dBScale || m_scale == MeterScale) &&
        m_channelMode != MergeChannels) {
        m = (h / channels);
    }

    int my = m + (((channel - minChannel) * h) / channels);

    int vy = 0;

    switch (m_scale) {

    case LinearScale:
        vy = int(m * value);
        break;

    case MeterScale:
        vy = AudioLevel::multiplier_to_preview(value, m);
        break;

    case dBScale:
        vy = int(dBscale(value, m));
        break;
    }

//    SVCERR << "mergingChannels= " << mergingChannels << ", channel  = " << channel << ", value = " << value << ", vy = " << vy << endl;

    return my - vy;
}

double
WaveformLayer::getValueForY(const LayerGeometryProvider *v, int y, int &channel) const
{
    int channels = 0, minChannel = 0, maxChannel = 0;
    bool mergingChannels = false, mixingChannels = false;

    channels = getChannelArrangement(minChannel, maxChannel,
                                     mergingChannels, mixingChannels);
    if (channels == 0) return 0;
    if (maxChannel < minChannel) return 0;

    int h = v->getPaintHeight();
    int m = (h / channels) / 2;

    if ((m_scale == dBScale || m_scale == MeterScale) &&
        m_channelMode != MergeChannels) {
        m = (h / channels);
    }
  
    channel = (y * channels) / h + minChannel;

    int my = m + (((channel - minChannel) * h) / channels);

    int vy = my - y;
    double value = 0;
    double thresh = m_dBMin;

    switch (m_scale) {

    case LinearScale:
        value = double(vy) / m;
        break;

    case MeterScale:
        value = AudioLevel::preview_to_multiplier(vy, m);
        break;

    case dBScale:
        value = (-thresh * double(vy)) / m + thresh;
        value = AudioLevel::dB_to_multiplier(value);
        break;
    }

    return value / m_gain;
}

bool
WaveformLayer::getYScaleValue(const LayerGeometryProvider *v, int y,
                              double &value, QString &unit) const
{
    int channel;

    value = getValueForY(v, y, channel);

    if (m_scale == dBScale || m_scale == MeterScale) {

        double thresh = m_dBMin;
        
        if (value > 0.0) {
            value = 10.0 * log10(value);
            if (value < thresh) value = thresh;
        } else value = thresh;

        unit = "dBV";

    } else {
        unit = "V";
    }

    return true;
}

bool
WaveformLayer::getYScaleDifference(const LayerGeometryProvider *v, int y0, int y1,
                                   double &diff, QString &unit) const
{
    int c0, c1;
    double v0 = getValueForY(v, y0, c0);
    double v1 = getValueForY(v, y1, c1);

    if (c0 != c1) {
        // different channels, not comparable
        diff = 0.0;
        unit = "";
        return false;
    }

    if (m_scale == dBScale || m_scale == MeterScale) {

        double thresh = m_dBMin;

        if (v1 == v0) diff = thresh;
        else {
            if (v1 > v0) diff = v0 / v1;
            else diff = v1 / v0;

            diff = 10.0 * log10(diff);
            if (diff < thresh) diff = thresh;
        }

        unit = "dBV";

    } else {
        diff = fabs(v1 - v0);
        unit = "V";
    }

    return true;
}

int
WaveformLayer::getVerticalScaleWidth(LayerGeometryProvider *, bool, QPainter &paint) const
{
    // Qt 5.13 deprecates QFontMetrics::width(), but its suggested
    // replacement (horizontalAdvance) was only added in Qt 5.11
    // which is too new for us
#pragma GCC diagnostic ignored "-Wdeprecated-declarations"

    if (m_scale == LinearScale) {
        return paint.fontMetrics().width("0.0") + 13;
    } else {
        return std::max(paint.fontMetrics().width(tr("0dB")),
                        paint.fontMetrics().width(Strings::minus_infinity)) + 13;
    }
}

void
WaveformLayer::paintVerticalScale(LayerGeometryProvider *v, bool, QPainter &paint, QRect rect) const
{
    auto model = ModelById::getAs<RangeSummarisableTimeValueModel>(m_model);
    if (!model || !model->isOK()) {
        return;
    }

    int channels = 0, minChannel = 0, maxChannel = 0;
    bool mergingChannels = false, mixingChannels = false;

    channels = getChannelArrangement(minChannel, maxChannel,
                                     mergingChannels, mixingChannels);
    if (channels == 0) return;

    int h = rect.height(), w = rect.width();
    int textHeight = paint.fontMetrics().height();
    int toff = -textHeight/2 + paint.fontMetrics().ascent() + 1;

    double gain = m_gain;

    for (int ch = minChannel; ch <= maxChannel; ++ch) {

        int lastLabelledY = -1;

        if (ch < (int)m_effectiveGains.size()) gain = m_effectiveGains[ch];

        int n = 10;

        for (int i = 0; i <= n; ++i) {

            double val = 0.0, nval = 0.0;
            QString text = "";

            switch (m_scale) {
                
            case LinearScale:
                val = (i * gain) / n;
                text = QString("%1").arg(double(i) / n);
                if (i == 0) text = "0.0";
                else {
                    nval = -val;
                    if (i == n) text = "1.0";
                }
                break;

            case MeterScale:
                val = AudioLevel::dB_to_multiplier(meterdbs[i]) * gain;
                text = QString("%1").arg(meterdbs[i]);
                if (i == n) text = tr("0dB");
                if (i == 0) {
                    text = Strings::minus_infinity;
                    val = 0.0;
                }
                break;

            case dBScale:
                val = AudioLevel::dB_to_multiplier(-(10*n) + i * 10) * gain;
                text = QString("%1").arg(-(10*n) + i * 10);
                if (i == n) text = tr("0dB");
                if (i == 0) {
                    text = Strings::minus_infinity;
                    val = 0.0;
                }
                break;
            }

            if (val < -1.0 || val > 1.0) continue;

            int y = getYForValue(v, val, ch);

            int ny = y;
            if (nval != 0.0) {
                ny = getYForValue(v, nval, ch);
            }

            bool spaceForLabel = (i == 0 ||
                                  abs(y - lastLabelledY) >= textHeight - 1);

            if (spaceForLabel) {

                int tx = 3;
                if (m_scale != LinearScale) {
                    tx = w - 10 - paint.fontMetrics().width(text);
                }
                  
                int ty = y;
                if (ty < paint.fontMetrics().ascent()) {
                    ty = paint.fontMetrics().ascent();
                } else if (ty > h - paint.fontMetrics().descent()) {
                    ty = h - paint.fontMetrics().descent();
                } else {
                    ty += toff;
                }
                paint.drawText(tx, ty, text);

                lastLabelledY = ty - toff;

                if (ny != y) {
                    ty = ny;
                    if (ty < paint.fontMetrics().ascent()) {
                        ty = paint.fontMetrics().ascent();
                    } else if (ty > h - paint.fontMetrics().descent()) {
                        ty = h - paint.fontMetrics().descent();
                    } else {
                        ty += toff;
                    }
                    paint.drawText(tx, ty, text);
                }

                paint.drawLine(w - 7, y, w, y);
                if (ny != y) paint.drawLine(w - 7, ny, w, ny);

            } else {

                paint.drawLine(w - 4, y, w, y);
                if (ny != y) paint.drawLine(w - 4, ny, w, ny);
            }
        }
    }
}

void
WaveformLayer::toXml(QTextStream &stream,
                     QString indent, QString extraAttributes) const
{
    QString s;
    
    QString colourName, colourSpec, darkbg;
    ColourDatabase::getInstance()->getStringValues
        (m_colour, colourName, colourSpec, darkbg);

    s += QString("gain=\"%1\" "
                 "showMeans=\"%2\" "
                 "greyscale=\"%3\" "
                 "channelMode=\"%4\" "
                 "channel=\"%5\" "
                 "scale=\"%6\" "
                 "middleLineHeight=\"%7\" "
                 "aggressive=\"%8\" "
                 "autoNormalize=\"%9\"")
        .arg(m_gain)
        .arg(m_showMeans)
        .arg(true) // Option removed, but effectively always on, so
                   // retained in the session file for compatibility
        .arg(m_channelMode)
        .arg(m_channel)
        .arg(m_scale)
        .arg(m_middleLineHeight)
        .arg(m_aggressive)
        .arg(m_autoNormalize);

    SingleColourLayer::toXml(stream, indent, extraAttributes + " " + s);
}

void
WaveformLayer::setProperties(const QXmlAttributes &attributes)
{
    bool ok = false;

    SingleColourLayer::setProperties(attributes);

    float gain = attributes.value("gain").toFloat(&ok);
    if (ok) setGain(gain);

    bool showMeans = (attributes.value("showMeans") == "1" ||
                      attributes.value("showMeans") == "true");
    setShowMeans(showMeans);

    ChannelMode channelMode = (ChannelMode)
        attributes.value("channelMode").toInt(&ok);
    if (ok) setChannelMode(channelMode);

    int channel = attributes.value("channel").toInt(&ok);
    if (ok) setChannel(channel);

    Scale scale = (Scale)attributes.value("scale").toInt(&ok);
    if (ok) setScale(scale);

    float middleLineHeight = attributes.value("middleLineHeight").toFloat(&ok);
    if (ok) setMiddleLineHeight(middleLineHeight);

    bool aggressive = (attributes.value("aggressive") == "1" ||
                       attributes.value("aggressive") == "true");
    setAggressiveCacheing(aggressive);

    bool autoNormalize = (attributes.value("autoNormalize") == "1" ||
                          attributes.value("autoNormalize") == "true");
    setAutoNormalize(autoNormalize);
}

int
WaveformLayer::getVerticalZoomSteps(int &defaultStep) const
{
    defaultStep = 50;
    return 100;
}

int
WaveformLayer::getCurrentVerticalZoomStep() const
{
    int val = int(lrint(log10(m_gain) * 20.0) + 50);
    if (val < 0) val = 0;
    if (val > 100) val = 100;
    return val;
}

void
WaveformLayer::setVerticalZoomStep(int step)
{
    setGain(powf(10, float(step - 50) / 20.f));
}

