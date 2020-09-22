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

#include "Colour3DPlotLayer.h"

#include "base/Profiler.h"
#include "base/LogRange.h"
#include "base/RangeMapper.h"

#include "ColourMapper.h"
#include "LayerGeometryProvider.h"
#include "PaintAssistant.h"

#include "data/model/Dense3DModelPeakCache.h"

#include "view/ViewManager.h"

#include <QPainter>
#include <QImage>
#include <QRect>
#include <QTextStream>
#include <QSettings>

#include <iostream>

#include <cassert>

using std::vector;

//#define DEBUG_COLOUR_3D_PLOT_LAYER_PAINT 1


Colour3DPlotLayer::Colour3DPlotLayer() :
    m_colourScale(ColourScaleType::Linear),
    m_colourScaleSet(false),
    m_colourMap(0),
    m_colourInverted(false),
    m_gain(1.0),
    m_binScale(BinScale::Linear),
    m_normalization(ColumnNormalization::None),
    m_normalizeVisibleArea(false),
    m_invertVertical(false),
    m_opaque(false),
    m_smooth(false),
    m_peakResolution(256),
    m_miny(0),
    m_maxy(0),
    m_synchronous(false),
    m_peakCacheDivisor(8)
{
    QSettings settings;
    settings.beginGroup("Preferences");
    setColourMap(settings.value("colour-3d-plot-colour",
                                ColourMapper::Green).toInt());
    settings.endGroup();
}

Colour3DPlotLayer::~Colour3DPlotLayer()
{
    invalidateRenderers();
}

const ZoomConstraint *
Colour3DPlotLayer::getZoomConstraint() const 
{
    auto model = ModelById::get(m_model);
    if (model) return model->getZoomConstraint();
    else return nullptr;
}

ColourScaleType
Colour3DPlotLayer::convertToColourScale(int value)
{
    switch (value) {
    default:
    case 0: return ColourScaleType::Linear;
    case 1: return ColourScaleType::Log;
    case 2: return ColourScaleType::PlusMinusOne;
    case 3: return ColourScaleType::Absolute;
    }
}

int
Colour3DPlotLayer::convertFromColourScale(ColourScaleType scale)
{
    switch (scale) {
    case ColourScaleType::Linear: return 0;
    case ColourScaleType::Log: return 1;
    case ColourScaleType::PlusMinusOne: return 2;
    case ColourScaleType::Absolute: return 3;

    case ColourScaleType::Meter:
    case ColourScaleType::Phase:
    default: return 0;
    }
}

std::pair<ColumnNormalization, bool>
Colour3DPlotLayer::convertToColumnNorm(int value)
{
    switch (value) {
    default:
    case 0: return { ColumnNormalization::None, false };
    case 1: return { ColumnNormalization::Range01, false };
    case 2: return { ColumnNormalization::None, true }; // visible area
    case 3: return { ColumnNormalization::Hybrid, false };
    }
}

int
Colour3DPlotLayer::convertFromColumnNorm(ColumnNormalization norm, bool visible)
{
    if (visible) return 2;
    switch (norm) {
    case ColumnNormalization::None: return 0;
    case ColumnNormalization::Range01: return 1;
    case ColumnNormalization::Hybrid: return 3;

    case ColumnNormalization::Sum1:
    case ColumnNormalization::Max1:
    default: return 0;
    }
}

void
Colour3DPlotLayer::setSynchronousPainting(bool synchronous)
{
    m_synchronous = synchronous;
}

void
Colour3DPlotLayer::setModel(ModelId modelId)
{
    auto newModel = ModelById::getAs<DenseThreeDimensionalModel>(modelId);
    
    if (!modelId.isNone() && !newModel) {
        throw std::logic_error("Not a DenseThreeDimensionalModel");
    }
    
    if (m_model == modelId) return;
    m_model = modelId;

    if (newModel) {
        connectSignals(m_model);

        connect(newModel.get(), SIGNAL(modelChanged(ModelId)),
                this, SLOT(handleModelChanged(ModelId)));
        connect(newModel.get(), SIGNAL(modelChangedWithin(ModelId, sv_frame_t, sv_frame_t)),
                this, SLOT(handleModelChangedWithin(ModelId, sv_frame_t, sv_frame_t)));

        m_peakResolution = 256;
        if (newModel->getResolution() > 512) {
            m_peakResolution = 16;
        } else if (newModel->getResolution() > 128) {
            m_peakResolution = 64;
        } else if (newModel->getResolution() > 2) {
            m_peakResolution = 128;
        }
    }
    
    invalidatePeakCache();

    emit modelReplaced();
}

void
Colour3DPlotLayer::invalidatePeakCache()
{
    // renderers use the peak cache, so we must invalidate those too
    invalidateRenderers();
    invalidateMagnitudes();

    if (!m_peakCache.isNone()) {
        ModelById::release(m_peakCache);
        m_peakCache = {};
    }
}

void
Colour3DPlotLayer::invalidateRenderers()
{
    for (ViewRendererMap::iterator i = m_renderers.begin();
         i != m_renderers.end(); ++i) {
        delete i->second;
    }
    m_renderers.clear();
}

void
Colour3DPlotLayer::invalidateMagnitudes()
{
#ifdef DEBUG_COLOUR_3D_PLOT_LAYER_PAINT
    SVDEBUG << "Colour3DPlotLayer::invalidateMagnitudes called" << endl;
#endif
    m_viewMags.clear();
}

ModelId
Colour3DPlotLayer::getPeakCache() const
{
    if (m_peakCache.isNone()) {
        auto peakCache = std::make_shared<Dense3DModelPeakCache>
            (m_model, m_peakCacheDivisor);
        m_peakCache = ModelById::add(peakCache);
    }
    return m_peakCache;
}

void
Colour3DPlotLayer::handleModelChanged(ModelId modelId)
{
    if (!m_colourScaleSet && m_colourScale == ColourScaleType::Linear) {
        auto model = ModelById::getAs<DenseThreeDimensionalModel>(m_model);
        if (model) {
            if (model->shouldUseLogValueScale()) {
                setColourScale(ColourScaleType::Log);
            } else {
                m_colourScaleSet = true;
            }
        }
    }
    invalidatePeakCache();
    emit modelChanged(modelId);
}

void
Colour3DPlotLayer::handleModelChangedWithin(ModelId modelId,
                                            sv_frame_t startFrame,
                                            sv_frame_t endFrame)
{
    if (!m_colourScaleSet && m_colourScale == ColourScaleType::Linear) {
        auto model = ModelById::getAs<DenseThreeDimensionalModel>(m_model);
        if (model && model->getWidth() > 50) {
            if (model->shouldUseLogValueScale()) {
                setColourScale(ColourScaleType::Log);
            } else {
                m_colourScaleSet = true;
            }
        }
    }
    emit modelChangedWithin(modelId, startFrame, endFrame);
}

Layer::PropertyList
Colour3DPlotLayer::getProperties() const
{
    PropertyList list;
    list.push_back("Colour");
    list.push_back("Colour Scale");
    list.push_back("Normalization");
    list.push_back("Gain");
    list.push_back("Bin Scale");
    list.push_back("Invert Vertical Scale");
    list.push_back("Opaque");
    list.push_back("Smooth");
    return list;
}

QString
Colour3DPlotLayer::getPropertyLabel(const PropertyName &name) const
{
    if (name == "Colour") return tr("Colour");
    if (name == "Colour Scale") return tr("Scale");
    if (name == "Normalization") return tr("Normalization");
    if (name == "Invert Vertical Scale") return tr("Invert Vertical Scale");
    if (name == "Gain") return tr("Gain");
    if (name == "Opaque") return tr("Always Opaque");
    if (name == "Smooth") return tr("Smooth");
    if (name == "Bin Scale") return tr("Bin Scale");
    return "";
}

QString
Colour3DPlotLayer::getPropertyIconName(const PropertyName &name) const
{
    if (name == "Invert Vertical Scale") return "invert-vertical";
    if (name == "Opaque") return "opaque";
    if (name == "Smooth") return "smooth";
    return "";
}

Layer::PropertyType
Colour3DPlotLayer::getPropertyType(const PropertyName &name) const
{
    if (name == "Gain") return RangeProperty;
    if (name == "Invert Vertical Scale") return ToggleProperty;
    if (name == "Opaque") return ToggleProperty;
    if (name == "Smooth") return ToggleProperty;
    if (name == "Colour") return ColourMapProperty;
    return ValueProperty;
}

QString
Colour3DPlotLayer::getPropertyGroupName(const PropertyName &name) const
{
    if (name == "Normalization" ||
        name == "Colour Scale" ||
        name == "Gain") {
        return tr("Scale");
    }
    if (name == "Bin Scale" ||
        name == "Invert Vertical Scale") {
        return tr("Bins");
    }
    if (name == "Opaque" ||
        name == "Smooth" ||
        name == "Colour") {
        return tr("Colour");
    }
    return QString();
}

int
Colour3DPlotLayer::getPropertyRangeAndValue(const PropertyName &name,
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

        *deflt = int(lrint(log10(1.0) * 20.0));
        if (*deflt < *min) *deflt = *min;
        if (*deflt > *max) *deflt = *max;

        val = int(lrint(log10(m_gain) * 20.0));
        if (val < *min) val = *min;
        if (val > *max) val = *max;

    } else if (name == "Colour Scale") {

        // linear, log, +/-1, abs
        *min = 0;
        *max = 3;
        *deflt = 0;

        val = convertFromColourScale(m_colourScale);

    } else if (name == "Colour") {

        *min = 0;
        *max = ColourMapper::getColourMapCount() - 1;
        *deflt = 0;

        val = m_colourMap;

    } else if (name == "Normalization") {
        
        *min = 0;
        *max = 3;
        *deflt = 0;

        val = convertFromColumnNorm(m_normalization, m_normalizeVisibleArea);

    } else if (name == "Invert Vertical Scale") {

        *min = 0;
        *max = 1;
        *deflt = 0;
        val = (m_invertVertical ? 1 : 0);

    } else if (name == "Bin Scale") {

        *min = 0;
        *max = 1;
        *deflt = int(BinScale::Linear);
        val = (int)m_binScale;

    } else if (name == "Opaque") {
        
        *min = 0;
        *max = 1;
        *deflt = 0;
        val = (m_opaque ? 1 : 0);
        
    } else if (name == "Smooth") {
        
        *min = 0;
        *max = 1;
        *deflt = 0;
        val = (m_smooth ? 1 : 0);
        
    } else {
        val = Layer::getPropertyRangeAndValue(name, min, max, deflt);
    }

    return val;
}

QString
Colour3DPlotLayer::getPropertyValueLabel(const PropertyName &name,
                                    int value) const
{
    if (name == "Colour") {
        return ColourMapper::getColourMapLabel(value);
    }
    if (name == "Colour Scale") {
        switch (value) {
        default:
        case 0: return tr("Linear");
        case 1: return tr("Log");
        case 2: return tr("+/-1");
        case 3: return tr("Absolute");
        }
    }
    if (name == "Normalization") {
        switch(value) {
        default:
        case 0: return tr("None");
        case 1: return tr("Col");
        case 2: return tr("View");
        case 3: return tr("Hybrid");
        }
//        return ""; // icon only
    }
    if (name == "Bin Scale") {
        switch (value) {
        default:
        case 0: return tr("Linear");
        case 1: return tr("Log");
        }
    }
    return tr("<unknown>");
}

QString
Colour3DPlotLayer::getPropertyValueIconName(const PropertyName &name,
                                            int value) const
{
    if (name == "Normalization") {
        switch(value) {
        default:
        case 0: return "normalise-none";
        case 1: return "normalise-columns";
        case 2: return "normalise";
        case 3: return "normalise-hybrid";
        }
    }
    return "";
}

RangeMapper *
Colour3DPlotLayer::getNewPropertyRangeMapper(const PropertyName &name) const
{
    if (name == "Gain") {
        return new LinearRangeMapper(-50, 50, -25, 25, tr("dB"));
    }
    return nullptr;
}

void
Colour3DPlotLayer::setProperty(const PropertyName &name, int value)
{
    if (name == "Gain") {
        setGain(float(pow(10, value/20.0)));
    } else if (name == "Colour Scale") {
        setColourScale(convertToColourScale(value));
    } else if (name == "Colour") {
        setColourMap(value);
    } else if (name == "Invert Vertical Scale") {
        setInvertVertical(value ? true : false);
    } else if (name == "Opaque") {
        setOpaque(value ? true : false);
    } else if (name == "Smooth") {
        setSmooth(value ? true : false);
    } else if (name == "Bin Scale") {
        switch (value) {
        default:
        case 0: setBinScale(BinScale::Linear); break;
        case 1: setBinScale(BinScale::Log); break;
        }
    } else if (name == "Normalization") {
        auto n = convertToColumnNorm(value);
        setNormalization(n.first);
        setNormalizeVisibleArea(n.second);
    }
}

void
Colour3DPlotLayer::setColourScale(ColourScaleType scale)
{
    m_colourScaleSet = true; // even if setting to the same thing
    if (m_colourScale == scale) return;
    m_colourScale = scale;
    invalidateRenderers();
    emit layerParametersChanged();
}

void
Colour3DPlotLayer::setColourMap(int map)
{
    if (m_colourMap == map) return;
    m_colourMap = map;
    invalidateRenderers();
    emit layerParametersChanged();
}

void
Colour3DPlotLayer::setGain(float gain)
{
    if (m_gain == gain) return;
    m_gain = gain;
    invalidateRenderers();
    emit layerParametersChanged();
}

float
Colour3DPlotLayer::getGain() const
{
    return m_gain;
}

void
Colour3DPlotLayer::setBinScale(BinScale binScale)
{
    if (m_binScale == binScale) return;
    m_binScale = binScale;
    invalidateRenderers();
    emit layerParametersChanged();
}

BinScale
Colour3DPlotLayer::getBinScale() const
{
    return m_binScale;
}

void
Colour3DPlotLayer::setNormalization(ColumnNormalization n)
{
    if (m_normalization == n) return;

    m_normalization = n;
    invalidateRenderers();
    
    emit layerParametersChanged();
}

ColumnNormalization
Colour3DPlotLayer::getNormalization() const
{
    return m_normalization;
}

void
Colour3DPlotLayer::setNormalizeVisibleArea(bool n)
{
    if (m_normalizeVisibleArea == n) return;

    invalidateRenderers();
    invalidateMagnitudes();
    m_normalizeVisibleArea = n;
    
    emit layerParametersChanged();
}

bool
Colour3DPlotLayer::getNormalizeVisibleArea() const
{
    return m_normalizeVisibleArea;
}

void
Colour3DPlotLayer::setInvertVertical(bool n)
{
    if (m_invertVertical == n) return;
    m_invertVertical = n;
    invalidateRenderers();
    emit layerParametersChanged();
}

void
Colour3DPlotLayer::setOpaque(bool n)
{
    if (m_opaque == n) return;
    m_opaque = n;
    invalidateRenderers();
    emit layerParametersChanged();
}

void
Colour3DPlotLayer::setSmooth(bool n)
{
    if (m_smooth == n) return;
    m_smooth = n;
    invalidateRenderers();
    emit layerParametersChanged();
}

bool
Colour3DPlotLayer::getInvertVertical() const
{
    return m_invertVertical;
}

bool
Colour3DPlotLayer::getOpaque() const
{
    return m_opaque;
}

bool
Colour3DPlotLayer::getSmooth() const
{
    return m_smooth;
}

bool
Colour3DPlotLayer::hasLightBackground() const 
{
    return ColourMapper(m_colourMap, m_colourInverted, 1.f, 255.f)
        .hasLightBackground();
}

void
Colour3DPlotLayer::setLayerDormant(const LayerGeometryProvider *v, bool dormant)
{
    if (dormant) {

#ifdef DEBUG_COLOUR_3D_PLOT_LAYER_PAINT
        SVDEBUG << "Colour3DPlotLayer::setLayerDormant(" << dormant << ")"
                  << endl;
#endif

        if (isLayerDormant(v)) {
            return;
        }

        Layer::setLayerDormant(v, true);

        invalidatePeakCache(); // for memory-saving purposes
        
    } else {

        Layer::setLayerDormant(v, false);
    }
}

bool
Colour3DPlotLayer::isLayerScrollable(const LayerGeometryProvider * /* v */) const
{
    // we do our own cacheing, and don't want to be responsible for
    // guaranteeing to get an invisible seam if someone else scrolls
    // us and we just fill in
    return false;
}

int
Colour3DPlotLayer::getCompletion(LayerGeometryProvider *) const
{
    auto model = ModelById::get(m_model);
    if (model) return model->getCompletion();
    else return 0;
}

bool
Colour3DPlotLayer::getValueExtents(double &min, double &max,
                                   bool &logarithmic, QString &unit) const
{
    auto model = ModelById::getAs<DenseThreeDimensionalModel>(m_model);
    if (!model) return false;

    min = 0;
    max = double(model->getHeight());

    logarithmic = (m_binScale == BinScale::Log);
    unit = "";

    return true;
}

bool
Colour3DPlotLayer::getDisplayExtents(double &min, double &max) const
{
    auto model = ModelById::getAs<DenseThreeDimensionalModel>(m_model);
    if (!model) return false;

    double hmax = double(model->getHeight());
    
    min = m_miny;
    max = m_maxy;
    if (max <= min) {
        min = 0;
        max = hmax;
    }
    if (min < 0) min = 0;
    if (max > hmax) max = hmax;

    return true;
}

bool
Colour3DPlotLayer::setDisplayExtents(double min, double max)
{
    m_miny = int(lrint(min));
    m_maxy = int(lrint(max));
    
    invalidateRenderers();
    
    emit layerParametersChanged();
    return true;
}

bool
Colour3DPlotLayer::getYScaleValue(const LayerGeometryProvider *, int,
                                  double &, QString &) const
{
    return false;//!!!
}

int
Colour3DPlotLayer::getVerticalZoomSteps(int &defaultStep) const
{
    auto model = ModelById::getAs<DenseThreeDimensionalModel>(m_model);
    if (!model) return 0;

    defaultStep = 0;
    int h = model->getHeight();
    return h;
}

int
Colour3DPlotLayer::getCurrentVerticalZoomStep() const
{
    auto model = ModelById::getAs<DenseThreeDimensionalModel>(m_model);
    if (!model) return 0;

    double min, max;
    getDisplayExtents(min, max);
    return model->getHeight() - int(lrint(max - min));
}

void
Colour3DPlotLayer::setVerticalZoomStep(int step)
{
    auto model = ModelById::getAs<DenseThreeDimensionalModel>(m_model);
    if (!model) return;

//    SVDEBUG << "Colour3DPlotLayer::setVerticalZoomStep(" <<step <<"): before: miny = " << m_miny << ", maxy = " << m_maxy << endl;

    int dist = model->getHeight() - step;
    if (dist < 1) dist = 1;
    double centre = m_miny + (m_maxy - m_miny) / 2.0;
    m_miny = int(lrint(centre - dist/2.0));
    if (m_miny < 0) m_miny = 0;
    m_maxy = m_miny + dist;
    if (m_maxy > model->getHeight()) m_maxy = model->getHeight();

    invalidateRenderers();
    
//    SVDEBUG << "Colour3DPlotLayer::setVerticalZoomStep(" <<step <<"):  after: miny = " << m_miny << ", maxy = " << m_maxy << endl;
    
    emit layerParametersChanged();
}

RangeMapper *
Colour3DPlotLayer::getNewVerticalZoomRangeMapper() const
{
    //!!! most of our uses of model in these functions is just to
    //!!! retrieve the model's height - perhaps we should cache it
    
    auto model = ModelById::getAs<DenseThreeDimensionalModel>(m_model);
    if (!model) return nullptr;

    return new LinearRangeMapper(0, model->getHeight(),
                                 0, model->getHeight(), "");
}

double
Colour3DPlotLayer::getYForBin(const LayerGeometryProvider *v, double bin) const
{
    double y = bin;
    auto model = ModelById::getAs<DenseThreeDimensionalModel>(m_model);
    if (!model) return y;
    double mn = 0, mx = model->getHeight();
    getDisplayExtents(mn, mx);
    double h = v->getPaintHeight();
    if (m_binScale == BinScale::Linear) {
        y = h - (((bin - mn) * h) / (mx - mn));
    } else {
        double logmin = mn + 1, logmax = mx + 1;
        LogRange::mapRange(logmin, logmax);
        y = h - (((LogRange::map(bin + 1) - logmin) * h) / (logmax - logmin));
    }
    return y;
}

double
Colour3DPlotLayer::getBinForY(const LayerGeometryProvider *v, double y) const
{
    double bin = y;
    auto model = ModelById::getAs<DenseThreeDimensionalModel>(m_model);
    if (!model) return bin;
    double mn = 0, mx = model->getHeight();
    getDisplayExtents(mn, mx);
    double h = v->getPaintHeight();
    if (m_binScale == BinScale::Linear) {
        // Arrange that the first bin (mn) appears as the exact result
        // for the first pixel (which is pixel h-1) and the first
        // out-of-range bin (mx) would appear as the exact result for
        // the first out-of-range pixel (which would be pixel -1)
        bin = mn + ((h - y - 1) * (mx - mn)) / h;
    } else {
        double logmin = mn + 1, logmax = mx + 1;
        LogRange::mapRange(logmin, logmax);
        bin = LogRange::unmap(logmin + ((h - y - 1) * (logmax - logmin)) / h) - 1;
    }
    return bin;
}

QString
Colour3DPlotLayer::getFeatureDescription(LayerGeometryProvider *v, QPoint &pos) const
{
    auto model = ModelById::getAs<DenseThreeDimensionalModel>(m_model);
    if (!model) return "";

    int x = pos.x();
    int y = pos.y();

    sv_frame_t modelStart = model->getStartFrame();
    int modelResolution = model->getResolution();

    double srRatio =
        v->getViewManager()->getMainModelSampleRate() /
        model->getSampleRate();

    int sx0 = int((double(v->getFrameForX(x)) / srRatio - double(modelStart)) /
                  modelResolution);

    int f0 = sx0 * modelResolution;
    int f1 =  f0 + modelResolution;

    int sh = model->getHeight();

    int symin = m_miny;
    int symax = m_maxy;
    if (symax <= symin) {
        symin = 0;
        symax = sh;
    }
    if (symin < 0) symin = 0;
    if (symax > sh) symax = sh;

 //    double binHeight = double(v->getPaintHeight()) / (symax - symin);
//    int sy = int((v->getPaintHeight() - y) / binHeight) + symin;

    int sy = getIBinForY(v, y);

    if (sy < 0 || sy >= model->getHeight()) {
        return "";
    }

    if (m_invertVertical) {
        sy = model->getHeight() - sy - 1;
    }

    float value = model->getValueAt(sx0, sy);

//    cerr << "bin value (" << sx0 << "," << sy << ") is " << value << endl;
    
    QString binName = model->getBinName(sy);
    if (binName == "") binName = QString("[%1]").arg(sy + 1);
    else binName = QString("%1 [%2]").arg(binName).arg(sy + 1);

    QString text = tr("Time:\t%1 - %2\nBin:\t%3\nValue:\t%4")
        .arg(RealTime::frame2RealTime(f0, model->getSampleRate())
             .toText(true).c_str())
        .arg(RealTime::frame2RealTime(f1, model->getSampleRate())
             .toText(true).c_str())
        .arg(binName)
        .arg(value);

    return text;
}

int
Colour3DPlotLayer::getColourScaleWidth(QPainter &p) const
{
    // Font is rotated
    int cw = p.fontMetrics().height();
    return cw;
}

int
Colour3DPlotLayer::getVerticalScaleWidth(LayerGeometryProvider *, bool, QPainter &paint) const
{
    auto model = ModelById::getAs<DenseThreeDimensionalModel>(m_model);
    if (!model) return 0;

    // Qt 5.13 deprecates QFontMetrics::width(), but its suggested
    // replacement (horizontalAdvance) was only added in Qt 5.11 which
    // is too new for us
#pragma GCC diagnostic ignored "-Wdeprecated-declarations"

    QString sampleText = QString("[%1]").arg(model->getHeight());
    int tw = paint.fontMetrics().width(sampleText);
    bool another = false;

    for (int i = 0; i < model->getHeight(); ++i) {
        if (model->getBinName(i).length() > sampleText.length()) {
            sampleText = model->getBinName(i);
            another = true;
        }
    }
    if (another) {
        tw = std::max(tw, paint.fontMetrics().width(sampleText));
    }

    return tw + 13 + getColourScaleWidth(paint);
}

void
Colour3DPlotLayer::paintVerticalScale(LayerGeometryProvider *v, bool, QPainter &paint, QRect rect) const
{
    auto model = ModelById::getAs<DenseThreeDimensionalModel>(m_model);
    if (!model) return;

    int h = rect.height(), w = rect.width();

    int cw = getColourScaleWidth(paint);
    
    int ch = h - 20;
    if (ch > 20) {

        double min = m_viewMags[v->getId()].getMin();
        double max = m_viewMags[v->getId()].getMax();

        if (max <= min) max = min + 0.1;

        paint.setPen(v->getForeground());
        paint.drawRect(4, 10, cw - 8, ch+1);

        for (int y = 0; y < ch; ++y) {
            double value = ((max - min) * (double(ch-y) - 1.0)) / double(ch) + min;
            paint.setPen(getRenderer(v)->getColour(value));
            paint.drawLine(5, 11 + y, cw - 5, 11 + y);
        }

        QString minstr = QString("%1").arg(min);
        QString maxstr = QString("%1").arg(max);
        
        paint.save();

        QFont font = paint.font();
        if (font.pixelSize() > 0) {
            int newSize = int(font.pixelSize() * 0.65);
            if (newSize < 6) newSize = 6;
            font.setPixelSize(newSize);
            paint.setFont(font);
        }

        int msw = paint.fontMetrics().width(maxstr);

        QTransform m;
        m.translate(cw - 6, ch + 10);
        m.rotate(-90);

        paint.setWorldTransform(m);

        PaintAssistant::drawVisibleText(v, paint, 2, 0, minstr,
                                        PaintAssistant::OutlinedText);

        m.translate(ch - msw - 2, 0);
        paint.setWorldTransform(m);

        PaintAssistant::drawVisibleText(v, paint, 0, 0, maxstr,
                                        PaintAssistant::OutlinedText);

        paint.restore();
    }

    paint.setPen(v->getForeground());

    int sh = model->getHeight();

    int symin = m_miny;
    int symax = m_maxy;
    if (symax <= symin) {
        symin = 0;
        symax = sh;
    }
    if (symin < 0) symin = 0;
    if (symax > sh) symax = sh;

    paint.save();

    int py = h;

    int defaultFontHeight = paint.fontMetrics().height();
    
    for (int i = symin; i <= symax; ++i) {

        int y0;

        y0 = getIYForBin(v, i);
        int h = py - y0;

        if (i > symin) {
            if (paint.fontMetrics().height() >= h) {
                if (h >= defaultFontHeight * 0.8) {
                    QFont tf = paint.font();
                    tf.setPixelSize(int(h * 0.8));
                    paint.setFont(tf);
                } else {
                    continue;
                }
            }
        }
        
        py = y0;

        if (i < symax) {
            paint.drawLine(cw, y0, w, y0);
        }

        if (i > symin) {

            int idx = i - 1;
            if (m_invertVertical) {
                idx = model->getHeight() - idx - 1;
            }

            QString text = model->getBinName(idx);
            if (text == "") text = QString("[%1]").arg(idx + 1);

            int ty = y0 + (h/2) - (paint.fontMetrics().height()/2) +
                paint.fontMetrics().ascent() + 1;

            paint.drawText(cw + 5, ty, text);
        }
    }

    paint.restore();
}

Colour3DPlotRenderer *
Colour3DPlotLayer::getRenderer(const LayerGeometryProvider *v) const
{
    auto model = ModelById::getAs<DenseThreeDimensionalModel>(m_model);
    if (!model) return nullptr;
    
    int viewId = v->getId();
    
    if (m_renderers.find(viewId) == m_renderers.end()) {

        Colour3DPlotRenderer::Sources sources;
        sources.verticalBinLayer = this;
        sources.source = m_model;
        sources.peakCaches.push_back(getPeakCache());

        ColourScale::Parameters cparams;
        cparams.colourMap = m_colourMap;
        cparams.inverted = m_colourInverted;
        cparams.scaleType = m_colourScale;
        cparams.gain = m_gain;

        double minValue = 0.0;
        double maxValue = 1.0;
        
        if (m_normalizeVisibleArea && m_viewMags[viewId].isSet()) {
            minValue = m_viewMags[viewId].getMin();
            maxValue = m_viewMags[viewId].getMax();
        } else if (m_normalization == ColumnNormalization::Hybrid) {
            minValue = 0;
            maxValue = log10(model->getMaximumLevel() + 1.0);
        } else if (m_normalization == ColumnNormalization::None) {
            minValue = model->getMinimumLevel();
            maxValue = model->getMaximumLevel();
        }

        SVDEBUG << "Colour3DPlotLayer: rebuilding renderer, value range is "
                << minValue << " -> " << maxValue
                << " (model min = " << model->getMinimumLevel()
                << ", max = " << model->getMaximumLevel() << ")"
                << endl;

        if (maxValue <= minValue) {
            maxValue = minValue + 0.1f;

            if (!(maxValue > minValue)) { // one of them must be NaN or Inf
                SVCERR << "WARNING: Colour3DPlotLayer::getRenderer: resetting "
                       << "minValue and maxValue to zero and one" << endl;
                minValue = 0.f;
                maxValue = 1.f;
            }
        }

        cparams.threshold = minValue;
        cparams.minValue = minValue;
        cparams.maxValue = maxValue;
        
        m_lastRenderedMags[viewId] = MagnitudeRange(float(minValue),
                                                    float(maxValue));

        Colour3DPlotRenderer::Parameters params;
        params.colourScale = ColourScale(cparams);
        params.normalization = m_normalization;
        params.binScale = m_binScale;
        params.alwaysOpaque = m_opaque;
        params.invertVertical = m_invertVertical;
        params.interpolate = m_smooth;

        m_renderers[viewId] = new Colour3DPlotRenderer(sources, params);
    }

    return m_renderers[viewId];
}

void
Colour3DPlotLayer::paintWithRenderer(LayerGeometryProvider *v,
                                     QPainter &paint, QRect rect) const
{
    Colour3DPlotRenderer *renderer = getRenderer(v);

    Colour3DPlotRenderer::RenderResult result;
    MagnitudeRange magRange;
    int viewId = v->getId();

    bool continuingPaint = !renderer->geometryChanged(v);
    
    if (continuingPaint) {
        magRange = m_viewMags[viewId];
    }
    
    if (m_synchronous) {

        result = renderer->render(v, paint, rect);

    } else {

        result = renderer->renderTimeConstrained(v, paint, rect);

        QRect uncached = renderer->getLargestUncachedRect(v);
        if (uncached.width() > 0) {
            v->updatePaintRect(uncached);
        }
    }
    
    magRange.sample(result.range);

    if (magRange.isSet()) {
        if (m_viewMags[viewId] != magRange) {
            m_viewMags[viewId] = magRange;
#ifdef DEBUG_COLOUR_3D_PLOT_LAYER_PAINT
            SVDEBUG << "mag range in this view has changed: "
                    << magRange.getMin() << " -> "
                    << magRange.getMax() << endl;
#endif
        }
    }
    
    if (!continuingPaint && m_normalizeVisibleArea &&
        m_viewMags[viewId] != m_lastRenderedMags[viewId]) {
#ifdef DEBUG_COLOUR_3D_PLOT_LAYER_PAINT
        SVDEBUG << "mag range has changed from last rendered range: re-rendering"
             << endl;
#endif
        delete m_renderers[viewId];
        m_renderers.erase(viewId);
        v->updatePaintRect(v->getPaintRect());
    }
}

void
Colour3DPlotLayer::paint(LayerGeometryProvider *v, QPainter &paint, QRect rect) const
{
    auto model = ModelById::getAs<DenseThreeDimensionalModel>(m_model);
/*
    if (model) {
        SVDEBUG << "Colour3DPlotLayer::paint: model says shouldUseLogValueScale = " << model->shouldUseLogValueScale() << endl;
    }
*/
    Profiler profiler("Colour3DPlotLayer::paint");
#ifdef DEBUG_COLOUR_3D_PLOT_LAYER_PAINT
    SVDEBUG << "Colour3DPlotLayer::paint(): m_model is " << m_model << ", zoom level is " << v->getZoomLevel() << ", rect is (" << rect.x() << "," << rect.y() << ") " << rect.width() << "x" << rect.height() << endl;
#endif

    int completion = 0;
    if (!model || !model->isOK() || !model->isReady(&completion)) {
        if (completion > 0) {
            paint.fillRect(0, 10, v->getPaintWidth() * completion / 100,
                           10, QColor(120, 120, 120));
        }
        return;
    }

    if (model->getWidth() == 0) {
#ifdef DEBUG_COLOUR_3D_PLOT_LAYER_PAINT
        SVDEBUG << "Colour3DPlotLayer::paint(): model width == 0, "
             << "nothing to paint (yet)" << endl;
#endif
        return;
    }

    paintWithRenderer(v, paint, rect);
}

bool
Colour3DPlotLayer::snapToFeatureFrame(LayerGeometryProvider *v,
                                      sv_frame_t &frame,
                                      int &resolution,
                                      SnapType snap,
                                      int ycoord) const
{
    auto model = ModelById::getAs<DenseThreeDimensionalModel>(m_model);
    if (!model) {
        return Layer::snapToFeatureFrame(v, frame, resolution, snap, ycoord);
    }

    resolution = model->getResolution();
    sv_frame_t left = (frame / resolution) * resolution;
    sv_frame_t right = left + resolution;

    switch (snap) {
    case SnapLeft:  frame = left;  break;
    case SnapRight: frame = right; break;
    case SnapNeighbouring:
        if (frame - left > right - frame) frame = right;
        else frame = left;
        break;
    }
    
    return true;
}

void
Colour3DPlotLayer::toXml(QTextStream &stream,
                         QString indent, QString extraAttributes) const
{
    QString s = QString("scale=\"%1\" "
                        "minY=\"%2\" "
                        "maxY=\"%3\" "
                        "invertVertical=\"%4\" "
                        "opaque=\"%5\" %6")
        .arg(convertFromColourScale(m_colourScale))
        .arg(m_miny)
        .arg(m_maxy)
        .arg(m_invertVertical ? "true" : "false")
        .arg(m_opaque ? "true" : "false")
        .arg(QString("binScale=\"%1\" smooth=\"%2\" gain=\"%3\" ")
             .arg(int(m_binScale))
             .arg(m_smooth ? "true" : "false")
             .arg(m_gain));

    // New-style colour map attribute, by string id rather than by
    // number

    s += QString("colourMap=\"%1\" ")
        .arg(ColourMapper::getColourMapId(m_colourMap));

    // Old-style colour map attribute

    s += QString("colourScheme=\"%1\" ")
        .arg(ColourMapper::getBackwardCompatibilityColourMap(m_colourMap));
    
    // New-style normalization attributes, allowing for more types of
    // normalization in future: write out the column normalization
    // type separately, and then whether we are normalizing visible
    // area as well afterwards
    
    s += QString("columnNormalization=\"%1\" ")
        .arg(m_normalization == ColumnNormalization::Range01 ? "peak" :
             m_normalization == ColumnNormalization::Hybrid ? "hybrid" : "none");

    // Old-style normalization attribute, for backward compatibility
    
    s += QString("normalizeColumns=\"%1\" ")
        .arg(m_normalization == ColumnNormalization::Range01 ? "true" : "false");

    // And this applies to both old- and new-style attributes
    
    s += QString("normalizeVisibleArea=\"%1\" ")
        .arg(m_normalizeVisibleArea ? "true" : "false");
    
    Layer::toXml(stream, indent, extraAttributes + " " + s);
}

void
Colour3DPlotLayer::setProperties(const QXmlAttributes &attributes)
{
    bool ok = false, alsoOk = false;

    ColourScaleType colourScale = convertToColourScale
        (attributes.value("scale").toInt(&ok));
    if (ok) setColourScale(colourScale);

    QString colourMapId = attributes.value("colourMap");
    int colourMap = ColourMapper::getColourMapById(colourMapId);
    if (colourMap >= 0) {
        setColourMap(colourMap);
    } else {
        colourMap = attributes.value("colourScheme").toInt(&ok);
        if (ok && colourMap < ColourMapper::getColourMapCount()) {
            setColourMap(colourMap);
        }
    }

    BinScale binScale = (BinScale)
        attributes.value("binScale").toInt(&ok);
    if (ok) setBinScale(binScale);

    bool invertVertical =
        (attributes.value("invertVertical").trimmed() == "true");
    setInvertVertical(invertVertical);

    bool opaque =
        (attributes.value("opaque").trimmed() == "true");
    setOpaque(opaque);

    bool smooth =
        (attributes.value("smooth").trimmed() == "true");
    setSmooth(smooth);

    float gain = attributes.value("gain").toFloat(&ok);
    if (ok) setGain(gain);

    float min = attributes.value("minY").toFloat(&ok);
    float max = attributes.value("maxY").toFloat(&alsoOk);
    if (ok && alsoOk) setDisplayExtents(min, max);

    bool haveNewStyleNormalization = false;
    
    QString columnNormalization = attributes.value("columnNormalization");

    if (columnNormalization != "") {

        haveNewStyleNormalization = true;

        if (columnNormalization == "peak") {
            setNormalization(ColumnNormalization::Range01);
        } else if (columnNormalization == "hybrid") {
            setNormalization(ColumnNormalization::Hybrid);
        } else if (columnNormalization == "none") {
            setNormalization(ColumnNormalization::None);
        } else {
            SVCERR << "NOTE: Unknown or unsupported columnNormalization attribute \""
                   << columnNormalization << "\"" << endl;
        }
    }

    if (!haveNewStyleNormalization) {

        setNormalization(ColumnNormalization::None);

        bool normalizeColumns =
            (attributes.value("normalizeColumns").trimmed() == "true");
        if (normalizeColumns) {
            setNormalization(ColumnNormalization::Range01);
        }

        bool normalizeHybrid =
            (attributes.value("normalizeHybrid").trimmed() == "true");
        if (normalizeHybrid) {
            setNormalization(ColumnNormalization::Hybrid);
        }
    }
    
    bool normalizeVisibleArea =
        (attributes.value("normalizeVisibleArea").trimmed() == "true");
    setNormalizeVisibleArea(normalizeVisibleArea);

    //!!! todo: check save/reload scaling, compare with
    //!!! SpectrogramLayer, compare with prior SV versions, compare
    //!!! with Tony v1 and v2 and their save files
}

