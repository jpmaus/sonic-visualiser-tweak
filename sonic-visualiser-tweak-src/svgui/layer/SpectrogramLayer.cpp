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

#include "SpectrogramLayer.h"

#include "view/View.h"
#include "base/Profiler.h"
#include "base/AudioLevel.h"
#include "base/Window.h"
#include "base/Pitch.h"
#include "base/Preferences.h"
#include "base/RangeMapper.h"
#include "base/LogRange.h"
#include "base/ColumnOp.h"
#include "base/Strings.h"
#include "base/StorageAdviser.h"
#include "base/Exceptions.h"
#include "widgets/CommandHistory.h"
#include "data/model/Dense3DModelPeakCache.h"

#include "ColourMapper.h"
#include "PianoScale.h"
#include "PaintAssistant.h"
#include "Colour3DPlotRenderer.h"

#include <QPainter>
#include <QImage>
#include <QPixmap>
#include <QRect>
#include <QApplication>
#include <QMessageBox>
#include <QMouseEvent>
#include <QTextStream>
#include <QSettings>

#include <iostream>

#include <cassert>
#include <cmath>

//#define DEBUG_SPECTROGRAM 1
//#define DEBUG_SPECTROGRAM_REPAINT 1

using namespace std;

SpectrogramLayer::SpectrogramLayer(Configuration config) :
    m_channel(0),
    m_windowSize(1024),
    m_windowType(HanningWindow),
    m_windowHopLevel(2),
    m_oversampling(1),
    m_gain(1.0),
    m_initialGain(1.0),
    m_threshold(1.0e-8f),
    m_initialThreshold(1.0e-8f),
    m_colourRotation(0),
    m_initialRotation(0),
    m_minFrequency(10),
    m_maxFrequency(8000),
    m_initialMaxFrequency(8000),
    m_verticallyFixed(false),
    m_colourScale(ColourScaleType::Log),
    m_colourScaleMultiple(1.0),
    m_colourMap(0),
    m_colourInverted(false),
    m_binScale(BinScale::Linear),
    m_binDisplay(BinDisplay::AllBins),
    m_normalization(ColumnNormalization::None),
    m_normalizeVisibleArea(false),
    m_lastEmittedZoomStep(-1),
    m_synchronous(false),
    m_haveDetailedScale(false),
    m_exiting(false),
    m_peakCacheDivisor(8)
{
    QString colourConfigName = "spectrogram-colour";
    int colourConfigDefault = int(ColourMapper::Green);
    
    if (config == FullRangeDb) {
        m_initialMaxFrequency = 0;
        setMaxFrequency(0);
    } else if (config == MelodicRange) {
        setWindowSize(8192);
        setWindowHopLevel(4);
        m_initialMaxFrequency = 1500;
        setMaxFrequency(1500);
        setMinFrequency(40);
        setColourScale(ColourScaleType::Linear);
        setColourMap(ColourMapper::Sunset);
        setBinScale(BinScale::Log);
        colourConfigName = "spectrogram-melodic-colour";
        colourConfigDefault = int(ColourMapper::Sunset);
//        setGain(20);
    } else if (config == MelodicPeaks) {
        setWindowSize(4096);
        setWindowHopLevel(5);
        m_initialMaxFrequency = 2000;
        setMaxFrequency(2000);
        setMinFrequency(40);
        setBinScale(BinScale::Log);
        setColourScale(ColourScaleType::Linear);
        setBinDisplay(BinDisplay::PeakFrequencies);
        setNormalization(ColumnNormalization::Max1);
        colourConfigName = "spectrogram-melodic-colour";
        colourConfigDefault = int(ColourMapper::Sunset);
    }

    QSettings settings;
    settings.beginGroup("Preferences");
    setColourMap(settings.value(colourConfigName, colourConfigDefault).toInt());
    settings.endGroup();
    
    Preferences *prefs = Preferences::getInstance();
    connect(prefs, SIGNAL(propertyChanged(PropertyContainer::PropertyName)),
            this, SLOT(preferenceChanged(PropertyContainer::PropertyName)));
    setWindowType(prefs->getWindowType());
}

SpectrogramLayer::~SpectrogramLayer()
{
    invalidateRenderers();
    deleteDerivedModels();
}

void
SpectrogramLayer::setVerticallyFixed()
{
    if (m_verticallyFixed) return;
    m_verticallyFixed = true;
    recreateFFTModel();
}

void
SpectrogramLayer::deleteDerivedModels()
{
    ModelById::release(m_fftModel);
    ModelById::release(m_peakCache);
    ModelById::release(m_wholeCache);

    m_fftModel = {};
    m_peakCache = {};
    m_wholeCache = {};
}

pair<ColourScaleType, double>
SpectrogramLayer::convertToColourScale(int value)
{
    switch (value) {
    case 0: return { ColourScaleType::Linear, 1.0 };
    case 1: return { ColourScaleType::Meter, 1.0 };
    case 2: return { ColourScaleType::Log, 2.0 }; // dB^2 (i.e. log of power)
    case 3: return { ColourScaleType::Log, 1.0 }; // dB   (of magnitude)
    case 4: return { ColourScaleType::Phase, 1.0 };
    default: return { ColourScaleType::Linear, 1.0 };
    }
}

int
SpectrogramLayer::convertFromColourScale(ColourScaleType scale, double multiple)
{
    switch (scale) {
    case ColourScaleType::Linear: return 0;
    case ColourScaleType::Meter: return 1;
    case ColourScaleType::Log: return (multiple > 1.5 ? 2 : 3);
    case ColourScaleType::Phase: return 4;
    case ColourScaleType::PlusMinusOne:
    case ColourScaleType::Absolute:
    default: return 0;
    }
}

std::pair<ColumnNormalization, bool>
SpectrogramLayer::convertToColumnNorm(int value)
{
    switch (value) {
    default:
    case 0: return { ColumnNormalization::None, false };
    case 1: return { ColumnNormalization::Max1, false };
    case 2: return { ColumnNormalization::None, true }; // visible area
    case 3: return { ColumnNormalization::Hybrid, false };
    }
}

int
SpectrogramLayer::convertFromColumnNorm(ColumnNormalization norm, bool visible)
{
    if (visible) return 2;
    switch (norm) {
    case ColumnNormalization::None: return 0;
    case ColumnNormalization::Max1: return 1;
    case ColumnNormalization::Hybrid: return 3;

    case ColumnNormalization::Sum1:
    case ColumnNormalization::Range01:
    default: return 0;
    }
}

void
SpectrogramLayer::setModel(ModelId modelId)
{
    auto newModel = ModelById::getAs<DenseTimeValueModel>(modelId);
    if (!modelId.isNone() && !newModel) {
        throw std::logic_error("Not a DenseTimeValueModel");
    }
    
    if (modelId == m_model) return;
    m_model = modelId;

    if (newModel) {
        recreateFFTModel();

        connectSignals(m_model);

        connect(newModel.get(),
                SIGNAL(modelChanged(ModelId)),
                this, SLOT(cacheInvalid(ModelId)));
        connect(newModel.get(),
                SIGNAL(modelChangedWithin(ModelId, sv_frame_t, sv_frame_t)),
                this, SLOT(cacheInvalid(ModelId, sv_frame_t, sv_frame_t)));
    }
    
    emit modelReplaced();
}

Layer::PropertyList
SpectrogramLayer::getProperties() const
{
    PropertyList list;
    list.push_back("Colour");
    list.push_back("Colour Scale");
    list.push_back("Window Size");
    list.push_back("Window Increment");
    list.push_back("Oversampling");
    list.push_back("Normalization");
    list.push_back("Bin Display");
    list.push_back("Threshold");
    list.push_back("Gain");
    list.push_back("Colour Rotation");
//    list.push_back("Min Frequency");
//    list.push_back("Max Frequency");
    list.push_back("Frequency Scale");
    return list;
}

QString
SpectrogramLayer::getPropertyLabel(const PropertyName &name) const
{
    if (name == "Colour") return tr("Colour");
    if (name == "Colour Scale") return tr("Colour Scale");
    if (name == "Window Size") return tr("Window Size");
    if (name == "Window Increment") return tr("Window Overlap");
    if (name == "Oversampling") return tr("Oversampling");
    if (name == "Normalization") return tr("Normalization");
    if (name == "Bin Display") return tr("Bin Display");
    if (name == "Threshold") return tr("Threshold");
    if (name == "Gain") return tr("Gain");
    if (name == "Colour Rotation") return tr("Colour Rotation");
    if (name == "Min Frequency") return tr("Min Frequency");
    if (name == "Max Frequency") return tr("Max Frequency");
    if (name == "Frequency Scale") return tr("Frequency Scale");
    return "";
}

QString
SpectrogramLayer::getPropertyIconName(const PropertyName &) const
{
    return "";
}

Layer::PropertyType
SpectrogramLayer::getPropertyType(const PropertyName &name) const
{
    if (name == "Gain") return RangeProperty;
    if (name == "Colour Rotation") return RangeProperty;
    if (name == "Threshold") return RangeProperty;
    if (name == "Colour") return ColourMapProperty;
    return ValueProperty;
}

QString
SpectrogramLayer::getPropertyGroupName(const PropertyName &name) const
{
    if (name == "Bin Display" ||
        name == "Frequency Scale") return tr("Bins");
    if (name == "Window Size" ||
        name == "Window Increment" ||
        name == "Oversampling") return tr("Window");
    if (name == "Colour" ||
        name == "Threshold" ||
        name == "Colour Rotation") return tr("Colour");
    if (name == "Normalization" ||
        name == "Gain" ||
        name == "Colour Scale") return tr("Scale");
    return QString();
}

int
SpectrogramLayer::getPropertyRangeAndValue(const PropertyName &name,
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

        *deflt = int(lrint(log10(m_initialGain) * 20.0));
        if (*deflt < *min) *deflt = *min;
        if (*deflt > *max) *deflt = *max;

        val = int(lrint(log10(m_gain) * 20.0));
        if (val < *min) val = *min;
        if (val > *max) val = *max;

    } else if (name == "Threshold") {

        *min = -81;
        *max = -1;

        *deflt = int(lrint(AudioLevel::multiplier_to_dB(m_initialThreshold)));
        if (*deflt < *min) *deflt = *min;
        if (*deflt > *max) *deflt = *max;

        val = int(lrint(AudioLevel::multiplier_to_dB(m_threshold)));
        if (val < *min) val = *min;
        if (val > *max) val = *max;

    } else if (name == "Colour Rotation") {

        *min = 0;
        *max = 256;
        *deflt = m_initialRotation;

        val = m_colourRotation;

    } else if (name == "Colour Scale") {

        // linear, meter, db^2, db, phase
        *min = 0;
        *max = 4;
        *deflt = 2;

        val = convertFromColourScale(m_colourScale, m_colourScaleMultiple);

    } else if (name == "Colour") {

        *min = 0;
        *max = ColourMapper::getColourMapCount() - 1;
        *deflt = 0;

        val = m_colourMap;

    } else if (name == "Window Size") {

        *min = 0;
        *max = 10;
        *deflt = 5;
        
        val = 0;
        int ws = m_windowSize;
        while (ws > 32) { ws >>= 1; val ++; }

    } else if (name == "Window Increment") {
        
        *min = 0;
        *max = 5;
        *deflt = 2;

        val = m_windowHopLevel;

    } else if (name == "Oversampling") {

        *min = 0;
        *max = 3;
        *deflt = 0;

        val = 0;
        int ov = m_oversampling;
        while (ov > 1) { ov >>= 1; val ++; }
        
    } else if (name == "Min Frequency") {

        *min = 0;
        *max = 9;
        *deflt = 1;

        switch (m_minFrequency) {
        case 0: default: val = 0; break;
        case 10: val = 1; break;
        case 20: val = 2; break;
        case 40: val = 3; break;
        case 100: val = 4; break;
        case 250: val = 5; break;
        case 500: val = 6; break;
        case 1000: val = 7; break;
        case 4000: val = 8; break;
        case 10000: val = 9; break;
        }
    
    } else if (name == "Max Frequency") {

        *min = 0;
        *max = 9;
        *deflt = 6;

        switch (m_maxFrequency) {
        case 500: val = 0; break;
        case 1000: val = 1; break;
        case 1500: val = 2; break;
        case 2000: val = 3; break;
        case 4000: val = 4; break;
        case 6000: val = 5; break;
        case 8000: val = 6; break;
        case 12000: val = 7; break;
        case 16000: val = 8; break;
        default: val = 9; break;
        }

    } else if (name == "Frequency Scale") {

        *min = 0;
        *max = 1;
        *deflt = int(BinScale::Linear);
        val = (int)m_binScale;

    } else if (name == "Bin Display") {

        *min = 0;
        *max = 2;
        *deflt = int(BinDisplay::AllBins);
        val = (int)m_binDisplay;

    } else if (name == "Normalization") {
        
        *min = 0;
        *max = 3;
        *deflt = 0;
        
        val = convertFromColumnNorm(m_normalization, m_normalizeVisibleArea);

    } else {
        val = Layer::getPropertyRangeAndValue(name, min, max, deflt);
    }

    return val;
}

QString
SpectrogramLayer::getPropertyValueLabel(const PropertyName &name,
                                        int value) const
{
    if (name == "Colour") {
        return ColourMapper::getColourMapLabel(value);
    }
    if (name == "Colour Scale") {
        switch (value) {
        default:
        case 0: return tr("Linear");
        case 1: return tr("Meter");
        case 2: return tr("dBV^2");
        case 3: return tr("dBV");
        case 4: return tr("Phase");
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
    if (name == "Window Size") {
        return QString("%1").arg(32 << value);
    }
    if (name == "Window Increment") {
        switch (value) {
        default:
        case 0: return tr("None");
        case 1: return tr("25 %");
        case 2: return tr("50 %");
        case 3: return tr("75 %");
        case 4: return tr("87.5 %");
        case 5: return tr("93.75 %");
        }
    }
    if (name == "Oversampling") {
        switch (value) {
        default:
        case 0: return tr("1x");
        case 1: return tr("2x");
        case 2: return tr("4x");
        case 3: return tr("8x");
        }
    }
    if (name == "Min Frequency") {
        switch (value) {
        default:
        case 0: return tr("No min");
        case 1: return tr("10 Hz");
        case 2: return tr("20 Hz");
        case 3: return tr("40 Hz");
        case 4: return tr("100 Hz");
        case 5: return tr("250 Hz");
        case 6: return tr("500 Hz");
        case 7: return tr("1 KHz");
        case 8: return tr("4 KHz");
        case 9: return tr("10 KHz");
        }
    }
    if (name == "Max Frequency") {
        switch (value) {
        default:
        case 0: return tr("500 Hz");
        case 1: return tr("1 KHz");
        case 2: return tr("1.5 KHz");
        case 3: return tr("2 KHz");
        case 4: return tr("4 KHz");
        case 5: return tr("6 KHz");
        case 6: return tr("8 KHz");
        case 7: return tr("12 KHz");
        case 8: return tr("16 KHz");
        case 9: return tr("No max");
        }
    }
    if (name == "Frequency Scale") {
        switch (value) {
        default:
        case 0: return tr("Linear");
        case 1: return tr("Log");
        }
    }
    if (name == "Bin Display") {
        switch (value) {
        default:
        case 0: return tr("All Bins");
        case 1: return tr("Peak Bins");
        case 2: return tr("Frequencies");
        }
    }
    return tr("<unknown>");
}

QString
SpectrogramLayer::getPropertyValueIconName(const PropertyName &name,
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
SpectrogramLayer::getNewPropertyRangeMapper(const PropertyName &name) const
{
    if (name == "Gain") {
        return new LinearRangeMapper(-50, 50, -25, 25, tr("dB"));
    }
    if (name == "Threshold") {
        return new LinearRangeMapper(-81, -1, -81, -1, tr("dB"), false,
                                     { { -81, Strings::minus_infinity } });
    }
    return nullptr;
}

void
SpectrogramLayer::setProperty(const PropertyName &name, int value)
{
    if (name == "Gain") {
        setGain(float(pow(10, float(value)/20.0)));
    } else if (name == "Threshold") {
        if (value == -81) setThreshold(0.0);
        else setThreshold(float(AudioLevel::dB_to_multiplier(value)));
    } else if (name == "Colour Rotation") {
        setColourRotation(value);
    } else if (name == "Colour") {
        setColourMap(value);
    } else if (name == "Window Size") {
        setWindowSize(32 << value);
    } else if (name == "Window Increment") {
        setWindowHopLevel(value);
    } else if (name == "Oversampling") {
        setOversampling(1 << value);
    } else if (name == "Min Frequency") {
        switch (value) {
        default:
        case 0: setMinFrequency(0); break;
        case 1: setMinFrequency(10); break;
        case 2: setMinFrequency(20); break;
        case 3: setMinFrequency(40); break;
        case 4: setMinFrequency(100); break;
        case 5: setMinFrequency(250); break;
        case 6: setMinFrequency(500); break;
        case 7: setMinFrequency(1000); break;
        case 8: setMinFrequency(4000); break;
        case 9: setMinFrequency(10000); break;
        }
        int vs = getCurrentVerticalZoomStep();
        if (vs != m_lastEmittedZoomStep) {
            emit verticalZoomChanged();
            m_lastEmittedZoomStep = vs;
        }
    } else if (name == "Max Frequency") {
        switch (value) {
        case 0: setMaxFrequency(500); break;
        case 1: setMaxFrequency(1000); break;
        case 2: setMaxFrequency(1500); break;
        case 3: setMaxFrequency(2000); break;
        case 4: setMaxFrequency(4000); break;
        case 5: setMaxFrequency(6000); break;
        case 6: setMaxFrequency(8000); break;
        case 7: setMaxFrequency(12000); break;
        case 8: setMaxFrequency(16000); break;
        default:
        case 9: setMaxFrequency(0); break;
        }
        int vs = getCurrentVerticalZoomStep();
        if (vs != m_lastEmittedZoomStep) {
            emit verticalZoomChanged();
            m_lastEmittedZoomStep = vs;
        }
    } else if (name == "Colour Scale") {
        setColourScaleMultiple(1.0);
        switch (value) {
        default:
        case 0: setColourScale(ColourScaleType::Linear); break;
        case 1: setColourScale(ColourScaleType::Meter); break;
        case 2:
            setColourScale(ColourScaleType::Log);
            setColourScaleMultiple(2.0);
            break;
        case 3: setColourScale(ColourScaleType::Log); break;
        case 4: setColourScale(ColourScaleType::Phase); break;
        }
    } else if (name == "Frequency Scale") {
        switch (value) {
        default:
        case 0: setBinScale(BinScale::Linear); break;
        case 1: setBinScale(BinScale::Log); break;
        }
    } else if (name == "Bin Display") {
        switch (value) {
        default:
        case 0: setBinDisplay(BinDisplay::AllBins); break;
        case 1: setBinDisplay(BinDisplay::PeakBins); break;
        case 2: setBinDisplay(BinDisplay::PeakFrequencies); break;
        }
    } else if (name == "Normalization") {
        auto n = convertToColumnNorm(value);
        setNormalization(n.first);
        setNormalizeVisibleArea(n.second);
    }
}

void
SpectrogramLayer::invalidateRenderers()
{
#ifdef DEBUG_SPECTROGRAM
    cerr << "SpectrogramLayer::invalidateRenderers called" << endl;
#endif

    for (ViewRendererMap::iterator i = m_renderers.begin();
         i != m_renderers.end(); ++i) {
        delete i->second;
    }
    m_renderers.clear();
}

void
SpectrogramLayer::preferenceChanged(PropertyContainer::PropertyName name)
{
    SVDEBUG << "SpectrogramLayer::preferenceChanged(" << name << ")" << endl;

    if (name == "Window Type") {
        setWindowType(Preferences::getInstance()->getWindowType());
        return;
    }
    if (name == "Spectrogram Y Smoothing") {
        invalidateRenderers();
        invalidateMagnitudes();
        emit layerParametersChanged();
    }
    if (name == "Spectrogram X Smoothing") {
        invalidateRenderers();
        invalidateMagnitudes();
        emit layerParametersChanged();
    }
    if (name == "Tuning Frequency") {
        emit layerParametersChanged();
    }
}

void
SpectrogramLayer::setChannel(int ch)
{
    if (m_channel == ch) return;

    invalidateRenderers();
    m_channel = ch;
    recreateFFTModel();

    emit layerParametersChanged();
}

int
SpectrogramLayer::getChannel() const
{
    return m_channel;
}

int
SpectrogramLayer::getFFTSize() const
{
    return m_windowSize * m_oversampling;
}

void
SpectrogramLayer::setWindowSize(int ws)
{
    if (m_windowSize == ws) return;
    invalidateRenderers();
    m_windowSize = ws;
    recreateFFTModel();
    emit layerParametersChanged();
}

int
SpectrogramLayer::getWindowSize() const
{
    return m_windowSize;
}

void
SpectrogramLayer::setWindowHopLevel(int v)
{
    if (m_windowHopLevel == v) return;
    invalidateRenderers();
    m_windowHopLevel = v;
    recreateFFTModel();
    emit layerParametersChanged();
}

int
SpectrogramLayer::getWindowHopLevel() const
{
    return m_windowHopLevel;
}

void
SpectrogramLayer::setOversampling(int oversampling)
{
    if (m_oversampling == oversampling) return;
    invalidateRenderers();
    m_oversampling = oversampling;
    recreateFFTModel();
    emit layerParametersChanged();
}

int
SpectrogramLayer::getOversampling() const
{
    return m_oversampling;
}

void
SpectrogramLayer::setWindowType(WindowType w)
{
    if (m_windowType == w) return;

    invalidateRenderers();
    
    m_windowType = w;

    recreateFFTModel();

    emit layerParametersChanged();
}

WindowType
SpectrogramLayer::getWindowType() const
{
    return m_windowType;
}

void
SpectrogramLayer::setGain(float gain)
{
//    SVDEBUG << "SpectrogramLayer::setGain(" << gain << ") (my gain is now "
//            << m_gain << ")" << endl;

    if (m_gain == gain) return;

    invalidateRenderers();
    
    m_gain = gain;
    
    emit layerParametersChanged();
}

float
SpectrogramLayer::getGain() const
{
    return m_gain;
}

void
SpectrogramLayer::setThreshold(float threshold)
{
    if (m_threshold == threshold) return;

    invalidateRenderers();
    
    m_threshold = threshold;

    emit layerParametersChanged();
}

float
SpectrogramLayer::getThreshold() const
{
    return m_threshold;
}

void
SpectrogramLayer::setMinFrequency(int mf)
{
    if (m_minFrequency == mf) return;

    if (m_verticallyFixed) {
        throw std::logic_error("setMinFrequency called with value differing from the default, on SpectrogramLayer with verticallyFixed true");
    }

//    SVDEBUG << "SpectrogramLayer::setMinFrequency: " << mf << endl;

    invalidateRenderers();
    invalidateMagnitudes();
    
    m_minFrequency = mf;

    emit layerParametersChanged();
}

int
SpectrogramLayer::getMinFrequency() const
{
    return m_minFrequency;
}

void
SpectrogramLayer::setMaxFrequency(int mf)
{
    if (m_maxFrequency == mf) return;

    if (m_verticallyFixed) {
        throw std::logic_error("setMaxFrequency called with value differing from the default, on SpectrogramLayer with verticallyFixed true");
    }
    
//    SVDEBUG << "SpectrogramLayer::setMaxFrequency: " << mf << endl;

    invalidateRenderers();
    invalidateMagnitudes();
    
    m_maxFrequency = mf;
    
    emit layerParametersChanged();
}

int
SpectrogramLayer::getMaxFrequency() const
{
    return m_maxFrequency;
}

void
SpectrogramLayer::setColourRotation(int r)
{
    if (r < 0) r = 0;
    if (r > 256) r = 256;
    int distance = r - m_colourRotation;

    if (distance != 0) {
        m_colourRotation = r;
    }

    // Initially the idea with colour rotation was that we would just
    // rotate the palette of an already-generated cache. That's not
    // really practical now that cacheing is handled in a separate
    // class in which the main cache no longer has a palette.
    invalidateRenderers();
    
    emit layerParametersChanged();
}

void
SpectrogramLayer::setColourScale(ColourScaleType colourScale)
{
    if (m_colourScale == colourScale) return;

    invalidateRenderers();
    
    m_colourScale = colourScale;
    
    emit layerParametersChanged();
}

ColourScaleType
SpectrogramLayer::getColourScale() const
{
    return m_colourScale;
}

void
SpectrogramLayer::setColourScaleMultiple(double multiple)
{
    if (m_colourScaleMultiple == multiple) return;

    invalidateRenderers();
    
    m_colourScaleMultiple = multiple;
    
    emit layerParametersChanged();
}

double
SpectrogramLayer::getColourScaleMultiple() const
{
    return m_colourScaleMultiple;
}

void
SpectrogramLayer::setColourMap(int map)
{
    if (m_colourMap == map) return;

    invalidateRenderers();
    
    m_colourMap = map;

    emit layerParametersChanged();
}

int
SpectrogramLayer::getColourMap() const
{
    return m_colourMap;
}

void
SpectrogramLayer::setBinScale(BinScale binScale)
{
    if (m_binScale == binScale) return;

    invalidateRenderers();
    m_binScale = binScale;

    emit layerParametersChanged();
}

BinScale
SpectrogramLayer::getBinScale() const
{
    return m_binScale;
}

void
SpectrogramLayer::setBinDisplay(BinDisplay binDisplay)
{
    if (m_binDisplay == binDisplay) return;

    invalidateRenderers();
    m_binDisplay = binDisplay;

    emit layerParametersChanged();
}

BinDisplay
SpectrogramLayer::getBinDisplay() const
{
    return m_binDisplay;
}

void
SpectrogramLayer::setNormalization(ColumnNormalization n)
{
    if (m_normalization == n) return;

    invalidateRenderers();
    invalidateMagnitudes();
    m_normalization = n;

    emit layerParametersChanged();
}

ColumnNormalization
SpectrogramLayer::getNormalization() const
{
    return m_normalization;
}

void
SpectrogramLayer::setNormalizeVisibleArea(bool n)
{
    if (m_normalizeVisibleArea == n) return;

    invalidateRenderers();
    invalidateMagnitudes();
    m_normalizeVisibleArea = n;
    
    emit layerParametersChanged();
}

bool
SpectrogramLayer::getNormalizeVisibleArea() const
{
    return m_normalizeVisibleArea;
}

void
SpectrogramLayer::setLayerDormant(const LayerGeometryProvider *v, bool dormant)
{
    if (dormant) {

#ifdef DEBUG_SPECTROGRAM_REPAINT
        cerr << "SpectrogramLayer::setLayerDormant(" << dormant << ")"
                  << endl;
#endif

        if (isLayerDormant(v)) {
            return;
        }

        Layer::setLayerDormant(v, true);

        invalidateRenderers();
        
    } else {

        Layer::setLayerDormant(v, false);
    }
}

bool
SpectrogramLayer::isLayerScrollable(const LayerGeometryProvider *) const
{
    // we do our own cacheing, and don't want to be responsible for
    // guaranteeing to get an invisible seam if someone else scrolls
    // us and we just fill in
    return false;
}

void
SpectrogramLayer::cacheInvalid(ModelId)
{
#ifdef DEBUG_SPECTROGRAM_REPAINT
    cerr << "SpectrogramLayer::cacheInvalid()" << endl;
#endif

    invalidateRenderers();
    invalidateMagnitudes();
}

void
SpectrogramLayer::cacheInvalid(
    ModelId,
#ifdef DEBUG_SPECTROGRAM_REPAINT
    sv_frame_t from, sv_frame_t to
#else 
    sv_frame_t     , sv_frame_t
#endif
    )
{
#ifdef DEBUG_SPECTROGRAM_REPAINT
    cerr << "SpectrogramLayer::cacheInvalid(" << from << ", " << to << ")" << endl;
#endif

    // We used to call invalidateMagnitudes(from, to) to invalidate
    // only those caches whose views contained some of the (from, to)
    // range. That's the right thing to do; it has been lost in
    // pulling out the image cache code, but it might not matter very
    // much, since the underlying models for spectrogram layers don't
    // change very often. Let's see.
    invalidateRenderers();
    invalidateMagnitudes();
}

bool
SpectrogramLayer::hasLightBackground() const 
{
    return ColourMapper(m_colourMap, m_colourInverted, 1.f, 255.f)
        .hasLightBackground();
}

double
SpectrogramLayer::getEffectiveMinFrequency() const
{
    auto model = ModelById::getAs<DenseTimeValueModel>(m_model);
    if (!model) return 0.0;
    
    sv_samplerate_t sr = model->getSampleRate();
    double minf = double(sr) / getFFTSize();

    if (m_minFrequency > 0.0) {
        int minbin = int((double(m_minFrequency) * getFFTSize()) / sr + 0.01);
        if (minbin < 1) minbin = 1;
        minf = minbin * sr / getFFTSize();
    }

    return minf;
}

double
SpectrogramLayer::getEffectiveMaxFrequency() const
{
    auto model = ModelById::getAs<DenseTimeValueModel>(m_model);
    if (!model) return 0.0;
    
    sv_samplerate_t sr = model->getSampleRate();
    double maxf = double(sr) / 2;

    if (m_maxFrequency > 0.0) {
        int maxbin = int((double(m_maxFrequency) * getFFTSize()) / sr + 0.1);
        if (maxbin > getFFTSize() / 2) maxbin = getFFTSize() / 2;
        maxf = maxbin * sr / getFFTSize();
    }

    return maxf;
}

bool
SpectrogramLayer::getYBinRange(LayerGeometryProvider *v, int y, double &q0, double &q1) const
{
    Profiler profiler("SpectrogramLayer::getYBinRange");
    int h = v->getPaintHeight();
    if (y < 0 || y >= h) return false;
    q0 = getBinForY(v, y);
    q1 = getBinForY(v, y-1);
    return true;
}

double
SpectrogramLayer::getYForBin(const LayerGeometryProvider *v, double bin) const
{
    auto model = ModelById::getAs<DenseTimeValueModel>(m_model);
    if (!model) return 0.0;
    
    double minf = getEffectiveMinFrequency();
    double maxf = getEffectiveMaxFrequency();
    bool logarithmic = (m_binScale == BinScale::Log);
    sv_samplerate_t sr = model->getSampleRate();

    double freq = (bin * sr) / getFFTSize();
    
    double y = v->getYForFrequency(freq, minf, maxf, logarithmic);
    
    return y;
}

double
SpectrogramLayer::getBinForY(const LayerGeometryProvider *v, double y) const
{
    auto model = ModelById::getAs<DenseTimeValueModel>(m_model);
    if (!model) return 0.0;

    sv_samplerate_t sr = model->getSampleRate();
    double minf = getEffectiveMinFrequency();
    double maxf = getEffectiveMaxFrequency();

    bool logarithmic = (m_binScale == BinScale::Log);

    double freq = v->getFrequencyForY(y, minf, maxf, logarithmic);

    // Now map on to ("proportion of") actual bins
    double bin = (freq * getFFTSize()) / sr;

    return bin;
}

bool
SpectrogramLayer::getXBinRange(LayerGeometryProvider *v, int x, double &s0, double &s1) const
{
    auto model = ModelById::getAs<DenseTimeValueModel>(m_model);
    if (!model) return false;

    sv_frame_t modelStart = model->getStartFrame();
    sv_frame_t modelEnd = model->getEndFrame();

    // Each pixel column covers an exact range of sample frames:
    sv_frame_t f0 = v->getFrameForX(x) - modelStart;
    sv_frame_t f1 = v->getFrameForX(x + 1) - modelStart - 1;

    if (f1 < int(modelStart) || f0 > int(modelEnd)) {
        return false;
    }
      
    // And that range may be drawn from a possibly non-integral
    // range of spectrogram windows:

    int windowIncrement = getWindowIncrement();
    s0 = double(f0) / windowIncrement;
    s1 = double(f1) / windowIncrement;

    return true;
}
 
bool
SpectrogramLayer::getXBinSourceRange(LayerGeometryProvider *v, int x, RealTime &min, RealTime &max) const
{
    auto model = ModelById::getAs<DenseTimeValueModel>(m_model);
    if (!model) return false;

    double s0 = 0, s1 = 0;
    if (!getXBinRange(v, x, s0, s1)) return false;
    
    int s0i = int(s0 + 0.001);
    int s1i = int(s1);

    int windowIncrement = getWindowIncrement();
    int w0 = s0i * windowIncrement - (m_windowSize - windowIncrement)/2;
    int w1 = s1i * windowIncrement + windowIncrement +
        (m_windowSize - windowIncrement)/2 - 1;
    
    min = RealTime::frame2RealTime(w0, model->getSampleRate());
    max = RealTime::frame2RealTime(w1, model->getSampleRate());
    return true;
}

bool
SpectrogramLayer::getYBinSourceRange(LayerGeometryProvider *v, int y, double &freqMin, double &freqMax)
const
{
    auto model = ModelById::getAs<DenseTimeValueModel>(m_model);
    if (!model) return false;

    double q0 = 0, q1 = 0;
    if (!getYBinRange(v, y, q0, q1)) return false;

    int q0i = int(q0 + 0.001);
    int q1i = int(q1);

    sv_samplerate_t sr = model->getSampleRate();

    for (int q = q0i; q <= q1i; ++q) {
        if (q == q0i) freqMin = (sr * q) / getFFTSize();
        if (q == q1i) freqMax = (sr * (q+1)) / getFFTSize();
    }
    return true;
}

bool
SpectrogramLayer::getAdjustedYBinSourceRange(LayerGeometryProvider *v, int x, int y,
                                             double &freqMin, double &freqMax,
                                             double &adjFreqMin, double &adjFreqMax)
const
{
    auto model = ModelById::getAs<DenseTimeValueModel>(m_model);
    if (!model || !model->isOK() || !model->isReady()) {
        return false;
    }

    auto fft = ModelById::getAs<FFTModel>(m_fftModel);
    if (!fft) return false;

    double s0 = 0, s1 = 0;
    if (!getXBinRange(v, x, s0, s1)) return false;

    double q0 = 0, q1 = 0;
    if (!getYBinRange(v, y, q0, q1)) return false;

    int s0i = int(s0 + 0.001);
    int s1i = int(s1);

    int q0i = int(q0 + 0.001);
    int q1i = int(q1);

    sv_samplerate_t sr = model->getSampleRate();

    bool haveAdj = false;

    bool peaksOnly = (m_binDisplay == BinDisplay::PeakBins ||
                      m_binDisplay == BinDisplay::PeakFrequencies);

    for (int q = q0i; q <= q1i; ++q) {

        for (int s = s0i; s <= s1i; ++s) {

            double binfreq = (double(sr) * q) / getFFTSize();
            if (q == q0i) freqMin = binfreq;
            if (q == q1i) freqMax = binfreq;

            if (peaksOnly && !fft->isLocalPeak(s, q)) continue;

            if (!fft->isOverThreshold
                (s, q, float(m_threshold * double(getFFTSize())/2.0))) {
                continue;
            }

            double freq = binfreq;
            
            if (s < int(fft->getWidth()) - 1) {

                fft->estimateStableFrequency(s, q, freq);
            
                if (!haveAdj || freq < adjFreqMin) adjFreqMin = freq;
                if (!haveAdj || freq > adjFreqMax) adjFreqMax = freq;

                haveAdj = true;
            }
        }
    }

    if (!haveAdj) {
        adjFreqMin = adjFreqMax = 0.0;
    }

    return haveAdj;
}
    
bool
SpectrogramLayer::getXYBinSourceRange(LayerGeometryProvider *v, int x, int y,
                                      double &min, double &max,
                                      double &phaseMin, double &phaseMax) const
{
    auto model = ModelById::getAs<DenseTimeValueModel>(m_model);
    if (!model || !model->isOK() || !model->isReady()) {
        return false;
    }

    double q0 = 0, q1 = 0;
    if (!getYBinRange(v, y, q0, q1)) return false;

    double s0 = 0, s1 = 0;
    if (!getXBinRange(v, x, s0, s1)) return false;
    
    int q0i = int(q0 + 0.001);
    int q1i = int(q1);

    int s0i = int(s0 + 0.001);
    int s1i = int(s1);

    bool rv = false;

    auto fft = ModelById::getAs<FFTModel>(m_fftModel);

    if (fft) {

        int cw = fft->getWidth();
        int ch = fft->getHeight();

        min = 0.0;
        max = 0.0;
        phaseMin = 0.0;
        phaseMax = 0.0;
        bool have = false;

        for (int q = q0i; q <= q1i; ++q) {
            for (int s = s0i; s <= s1i; ++s) {
                if (s >= 0 && q >= 0 && s < cw && q < ch) {

                    double value;

                    value = fft->getPhaseAt(s, q);
                    if (!have || value < phaseMin) { phaseMin = value; }
                    if (!have || value > phaseMax) { phaseMax = value; }

                    value = fft->getMagnitudeAt(s, q) / (getFFTSize()/2.0);
                    if (!have || value < min) { min = value; }
                    if (!have || value > max) { max = value; }
                    
                    have = true;
                }       
            }
        }
        
        if (have) {
            rv = true;
        }
    }

    return rv;
}
        
void
SpectrogramLayer::recreateFFTModel()
{
    SVDEBUG << "SpectrogramLayer::recreateFFTModel called" << endl;

    { // scope, avoid hanging on to this pointer
        auto model = ModelById::getAs<DenseTimeValueModel>(m_model);
        if (!model || !model->isOK()) {
            deleteDerivedModels();
            return;
        }
    }
    
    deleteDerivedModels();

    auto newFFTModel = std::make_shared<FFTModel>(m_model,
                                                  m_channel,
                                                  m_windowType,
                                                  m_windowSize,
                                                  getWindowIncrement(),
                                                  getFFTSize());

    if (!newFFTModel->isOK()) {
        QMessageBox::critical
            (nullptr, tr("FFT cache failed"),
             tr("Failed to create the FFT model for this spectrogram.\n"
                "There may be insufficient memory or disc space to continue."));
        return;
    }

    if (m_verticallyFixed) {
        newFFTModel->setMaximumFrequency(getMaxFrequency());
    }
    
    m_fftModel = ModelById::add(newFFTModel);

    bool createWholeCache = false;
    checkCacheSpace(&m_peakCacheDivisor, &createWholeCache);
    
    if (createWholeCache) {

        auto whole = std::make_shared<Dense3DModelPeakCache>(m_fftModel, 1);
        m_wholeCache = ModelById::add(whole);

        auto peaks = std::make_shared<Dense3DModelPeakCache>(m_fftModel,
                                                             m_peakCacheDivisor);
        m_peakCache = ModelById::add(peaks);

    } else {

        auto peaks = std::make_shared<Dense3DModelPeakCache>(m_fftModel,
                                                             m_peakCacheDivisor);
        m_peakCache = ModelById::add(peaks);
    }
}

void
SpectrogramLayer::checkCacheSpace(int *suggestedPeakDivisor,
                                  bool *createWholeCache) const
{
    *suggestedPeakDivisor = 8;
    *createWholeCache = false;

    auto fftModel = ModelById::getAs<FFTModel>(m_fftModel);
    if (!fftModel) return;

    size_t sz =
        size_t(fftModel->getWidth()) *
        size_t(fftModel->getHeight()) *
        sizeof(float);

    try {
        SVDEBUG << "Requesting advice from StorageAdviser on whether to create whole-model cache" << endl;
        // The lower amount here is the amount required for the
        // slightly higher-resolution version of the peak cache
        // without a whole-model cache; the higher amount is that for
        // the whole-model cache. The factors of 1024 are because
        // StorageAdviser rather stupidly works in kilobytes
        StorageAdviser::Recommendation recommendation =
            StorageAdviser::recommend
            (StorageAdviser::Criteria(StorageAdviser::SpeedCritical |
                                      StorageAdviser::PrecisionCritical |
                                      StorageAdviser::FrequentLookupLikely),
             (sz / 8) / 1024, sz / 1024);
        if (recommendation & StorageAdviser::UseDisc) {
            SVDEBUG << "Seems inadvisable to create whole-model cache" << endl;
        } else if (recommendation & StorageAdviser::ConserveSpace) {
            SVDEBUG << "Seems inadvisable to create whole-model cache but acceptable to use the slightly higher-resolution peak cache" << endl;
            *suggestedPeakDivisor = 4;
        } else  {
            SVDEBUG << "Seems fine to create whole-model cache" << endl;
            *createWholeCache = true;
        }
    } catch (const InsufficientDiscSpace &) {
        SVDEBUG << "Seems like a terrible idea to create whole-model cache" << endl;
    }
}

ModelId
SpectrogramLayer::getSliceableModel() const
{
    return m_fftModel;
}

void
SpectrogramLayer::invalidateMagnitudes()
{
#ifdef DEBUG_SPECTROGRAM
    cerr << "SpectrogramLayer::invalidateMagnitudes called" << endl;
#endif
    m_viewMags.clear();
}

void
SpectrogramLayer::setSynchronousPainting(bool synchronous)
{
    m_synchronous = synchronous;
}

Colour3DPlotRenderer *
SpectrogramLayer::getRenderer(LayerGeometryProvider *v) const
{
    int viewId = v->getId();
    
    if (m_renderers.find(viewId) == m_renderers.end()) {

        Colour3DPlotRenderer::Sources sources;
        sources.verticalBinLayer = this;
        sources.fft = m_fftModel;
        sources.source = sources.fft;
        if (!m_peakCache.isNone()) sources.peakCaches.push_back(m_peakCache);
        if (!m_wholeCache.isNone()) sources.peakCaches.push_back(m_wholeCache);

        ColourScale::Parameters cparams;
        cparams.colourMap = m_colourMap;
        cparams.scaleType = m_colourScale;
        cparams.multiple = m_colourScaleMultiple;

        if (m_colourScale != ColourScaleType::Phase) {
            cparams.gain = m_gain;
            cparams.threshold = m_threshold;
        }

        double minValue = 0.0f;
        double maxValue = 1.0f;
        
        if (m_normalizeVisibleArea && m_viewMags[viewId].isSet()) {
            minValue = m_viewMags[viewId].getMin();
            maxValue = m_viewMags[viewId].getMax();
        } else if (m_colourScale == ColourScaleType::Linear &&
                   m_normalization == ColumnNormalization::None) {
            maxValue = 0.1f;
        }

        if (maxValue <= minValue) {
            maxValue = minValue + 0.1f;
        }
        if (maxValue <= m_threshold) {
            maxValue = m_threshold + 0.1f;
        }

        cparams.minValue = minValue;
        cparams.maxValue = maxValue;

        m_lastRenderedMags[viewId] = MagnitudeRange(float(minValue),
                                                    float(maxValue));

        Colour3DPlotRenderer::Parameters params;
        params.colourScale = ColourScale(cparams);
        params.normalization = m_normalization;
        params.binDisplay = m_binDisplay;
        params.binScale = m_binScale;
        params.alwaysOpaque = true;
        params.invertVertical = false;
        params.scaleFactor = 1.0;
        params.colourRotation = m_colourRotation;

        if (m_colourScale != ColourScaleType::Phase &&
            m_normalization != ColumnNormalization::Hybrid) {
            params.scaleFactor *= 2.f / float(getWindowSize());
        }

        Preferences::SpectrogramSmoothing smoothing = 
            Preferences::getInstance()->getSpectrogramSmoothing();
        params.interpolate = 
            (smoothing != Preferences::NoSpectrogramSmoothing);

        m_renderers[viewId] = new Colour3DPlotRenderer(sources, params);

        m_crosshairColour =
            ColourMapper(m_colourMap, m_colourInverted, 1.f, 255.f)
            .getContrastingColour();
    }

    return m_renderers[viewId];
}

void
SpectrogramLayer::paintWithRenderer(LayerGeometryProvider *v, QPainter &paint, QRect rect) const
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

#ifdef DEBUG_SPECTROGRAM_REPAINT
        cerr << "rect width from this paint: " << result.rendered.width()
             << ", mag range in this paint: " << result.range.getMin() << " -> "
             << result.range.getMax() << endl;
#endif
        
        QRect uncached = renderer->getLargestUncachedRect(v);
        if (uncached.width() > 0) {
            v->updatePaintRect(uncached);
        }
    }

    magRange.sample(result.range);

    if (magRange.isSet()) {
        if (m_viewMags[viewId] != magRange) {
            m_viewMags[viewId] = magRange;
#ifdef DEBUG_SPECTROGRAM_REPAINT
            cerr << "mag range in this view has changed: "
                 << magRange.getMin() << " -> " << magRange.getMax() << endl;
#endif
        }
    }

    if (!continuingPaint && m_normalizeVisibleArea &&
        m_viewMags[viewId] != m_lastRenderedMags[viewId]) {
#ifdef DEBUG_SPECTROGRAM_REPAINT
        cerr << "mag range has changed from last rendered range: re-rendering"
             << endl;
#endif
        delete m_renderers[viewId];
        m_renderers.erase(viewId);
        v->updatePaintRect(v->getPaintRect());
    }
}

void
SpectrogramLayer::paint(LayerGeometryProvider *v, QPainter &paint, QRect rect) const
{
    Profiler profiler("SpectrogramLayer::paint", false);

#ifdef DEBUG_SPECTROGRAM_REPAINT
    cerr << "SpectrogramLayer::paint() entering: m_model is " << m_model << ", zoom level is " << v->getZoomLevel() << endl;
    
    cerr << "SpectrogramLayer::paint(): rect is " << rect.x() << "," << rect.y() << " " << rect.width() << "x" << rect.height() << endl;
#endif

    auto model = ModelById::getAs<DenseTimeValueModel>(m_model);
    if (!model || !model->isOK() || !model->isReady()) {
        return;
    }

    paintWithRenderer(v, paint, rect);

    illuminateLocalFeatures(v, paint);
}

void
SpectrogramLayer::illuminateLocalFeatures(LayerGeometryProvider *v, QPainter &paint) const
{
    Profiler profiler("SpectrogramLayer::illuminateLocalFeatures");

    auto model = ModelById::getAs<DenseTimeValueModel>(m_model);
    
    QPoint localPos;
    if (!v->shouldIlluminateLocalFeatures(this, localPos) || !model) {
        return;
    }

#ifdef DEBUG_SPECTROGRAM_REPAINT
    cerr << "SpectrogramLayer: illuminateLocalFeatures("
              << localPos.x() << "," << localPos.y() << ")" << endl;
#endif

    double s0, s1;
    double f0, f1;

    if (getXBinRange(v, localPos.x(), s0, s1) &&
        getYBinSourceRange(v, localPos.y(), f0, f1)) {
        
        int s0i = int(s0 + 0.001);
        int s1i = int(s1);
        
        int x0 = v->getXForFrame(s0i * getWindowIncrement());
        int x1 = v->getXForFrame((s1i + 1) * getWindowIncrement());

        int y1 = int(getYForFrequency(v, f1));
        int y0 = int(getYForFrequency(v, f0));
        
#ifdef DEBUG_SPECTROGRAM_REPAINT
        cerr << "SpectrogramLayer: illuminate "
                  << x0 << "," << y1 << " -> " << x1 << "," << y0 << endl;
#endif
        
        paint.setPen(v->getForeground());

        //!!! should we be using paintCrosshairs for this?

        paint.drawRect(x0, y1, x1 - x0 + 1, y0 - y1 + 1);
    }
}

double
SpectrogramLayer::getYForFrequency(const LayerGeometryProvider *v, double frequency) const
{
    return v->getYForFrequency(frequency,
                               getEffectiveMinFrequency(),
                               getEffectiveMaxFrequency(),
                               m_binScale == BinScale::Log);
}

double
SpectrogramLayer::getFrequencyForY(const LayerGeometryProvider *v, int y) const
{
    return v->getFrequencyForY(y,
                               getEffectiveMinFrequency(),
                               getEffectiveMaxFrequency(),
                               m_binScale == BinScale::Log);
}

int
SpectrogramLayer::getCompletion(LayerGeometryProvider *) const
{
    auto fftModel = ModelById::getAs<FFTModel>(m_fftModel);
    if (!fftModel) return 100;
    int completion = fftModel->getCompletion();
#ifdef DEBUG_SPECTROGRAM_REPAINT
    cerr << "SpectrogramLayer::getCompletion: completion = " << completion << endl;
#endif
    return completion;
}

QString
SpectrogramLayer::getError(LayerGeometryProvider *) const
{
    auto fftModel = ModelById::getAs<FFTModel>(m_fftModel);
    if (!fftModel) return "";
    return fftModel->getError();
}

bool
SpectrogramLayer::getValueExtents(double &min, double &max,
                                  bool &logarithmic, QString &unit) const
{
    auto model = ModelById::getAs<DenseTimeValueModel>(m_model);
    if (!model) return false;

    sv_samplerate_t sr = model->getSampleRate();
    min = double(sr) / getFFTSize();
    max = double(sr) / 2;
    
    logarithmic = (m_binScale == BinScale::Log);
    unit = "Hz";
    return true;
}

bool
SpectrogramLayer::getDisplayExtents(double &min, double &max) const
{
    min = getEffectiveMinFrequency();
    max = getEffectiveMaxFrequency();

//    SVDEBUG << "SpectrogramLayer::getDisplayExtents: " << min << "->" << max << endl;
    return true;
}    

bool
SpectrogramLayer::setDisplayExtents(double min, double max)
{
    auto model = ModelById::getAs<DenseTimeValueModel>(m_model);
    if (!model) return false;

//    SVDEBUG << "SpectrogramLayer::setDisplayExtents: " << min << "->" << max << endl;

    if (min < 0) min = 0;
    if (max > model->getSampleRate()/2.0) max = model->getSampleRate()/2.0;
    
    int minf = int(lrint(min));
    int maxf = int(lrint(max));

    if (m_minFrequency == minf && m_maxFrequency == maxf) return true;

    invalidateRenderers();
    invalidateMagnitudes();

    if (m_verticallyFixed &&
        (m_minFrequency != minf || m_maxFrequency != maxf)) {
        throw std::logic_error("setDisplayExtents called with values differing from the defaults, on SpectrogramLayer with verticallyFixed true");
    }

    m_minFrequency = minf;
    m_maxFrequency = maxf;
    
    emit layerParametersChanged();

    int vs = getCurrentVerticalZoomStep();
    if (vs != m_lastEmittedZoomStep) {
        emit verticalZoomChanged();
        m_lastEmittedZoomStep = vs;
    }

    return true;
}

bool
SpectrogramLayer::getYScaleValue(const LayerGeometryProvider *v, int y,
                                 double &value, QString &unit) const
{
    value = getFrequencyForY(v, y);
    unit = "Hz";
    return true;
}

bool
SpectrogramLayer::snapToFeatureFrame(LayerGeometryProvider *,
                                     sv_frame_t &frame,
                                     int &resolution,
                                     SnapType snap, int) const
{
    resolution = getWindowIncrement();
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
SpectrogramLayer::measureDoubleClick(LayerGeometryProvider *v, QMouseEvent *e)
{
    const Colour3DPlotRenderer *renderer = getRenderer(v);
    if (!renderer) return;

    QRect rect = renderer->findSimilarRegionExtents(e->pos());
    if (rect.isValid()) {
        MeasureRect mr;
        setMeasureRectFromPixrect(v, mr, rect);
        CommandHistory::getInstance()->addCommand
            (new AddMeasurementRectCommand(this, mr));
    }
}

bool
SpectrogramLayer::getCrosshairExtents(LayerGeometryProvider *v, QPainter &paint,
                                      QPoint cursorPos,
                                      vector<QRect> &extents) const
{
    // Qt 5.13 deprecates QFontMetrics::width(), but its suggested
    // replacement (horizontalAdvance) was only added in Qt 5.11
    // which is too new for us
#pragma GCC diagnostic ignored "-Wdeprecated-declarations"

    QRect vertical(cursorPos.x() - 12, 0, 12, v->getPaintHeight());
    extents.push_back(vertical);

    QRect horizontal(0, cursorPos.y(), cursorPos.x(), 1);
    extents.push_back(horizontal);

    int sw = getVerticalScaleWidth(v, m_haveDetailedScale, paint);

    QRect freq(sw, cursorPos.y() - paint.fontMetrics().ascent() - 2,
               paint.fontMetrics().width("123456 Hz") + 2,
               paint.fontMetrics().height());
    extents.push_back(freq);

    QRect pitch(sw, cursorPos.y() + 2,
                paint.fontMetrics().width("C#10+50c") + 2,
                paint.fontMetrics().height());
    extents.push_back(pitch);

    QRect rt(cursorPos.x(),
             v->getPaintHeight() - paint.fontMetrics().height() - 2,
             paint.fontMetrics().width("1234.567 s"),
             paint.fontMetrics().height());
    extents.push_back(rt);

    int w(paint.fontMetrics().width("1234567890") + 2);
    QRect frame(cursorPos.x() - w - 2,
                v->getPaintHeight() - paint.fontMetrics().height() - 2,
                w,
                paint.fontMetrics().height());
    extents.push_back(frame);

    return true;
}

void
SpectrogramLayer::paintCrosshairs(LayerGeometryProvider *v, QPainter &paint,
                                  QPoint cursorPos) const
{
    auto model = ModelById::getAs<DenseTimeValueModel>(m_model);
    if (!model) return;

    paint.save();
    
    int sw = getVerticalScaleWidth(v, m_haveDetailedScale, paint);

    QFont fn = paint.font();
    if (fn.pointSize() > 8) {
        fn.setPointSize(fn.pointSize() - 1);
        paint.setFont(fn);
    }
    paint.setPen(m_crosshairColour);

    paint.drawLine(0, cursorPos.y(), cursorPos.x() - 1, cursorPos.y());
    paint.drawLine(cursorPos.x(), 0, cursorPos.x(), v->getPaintHeight());
    
    double fundamental = getFrequencyForY(v, cursorPos.y());

    PaintAssistant::drawVisibleText
        (v, paint,
         sw + 2,
         cursorPos.y() - 2,
         QString("%1 Hz").arg(fundamental),
         PaintAssistant::OutlinedText);

    if (Pitch::isFrequencyInMidiRange(fundamental)) {
        QString pitchLabel = Pitch::getPitchLabelForFrequency(fundamental);
        PaintAssistant::drawVisibleText
            (v, paint,
             sw + 2,
             cursorPos.y() + paint.fontMetrics().ascent() + 2,
             pitchLabel,
             PaintAssistant::OutlinedText);
    }

    sv_frame_t frame = v->getFrameForX(cursorPos.x());
    RealTime rt = RealTime::frame2RealTime(frame, model->getSampleRate());
    QString rtLabel = QString("%1 s").arg(rt.toText(true).c_str());
    QString frameLabel = QString("%1").arg(frame);
    PaintAssistant::drawVisibleText
        (v, paint,
         cursorPos.x() - paint.fontMetrics().width(frameLabel) - 2,
         v->getPaintHeight() - 2,
         frameLabel,
         PaintAssistant::OutlinedText);
    PaintAssistant::drawVisibleText
        (v, paint,
         cursorPos.x() + 2,
         v->getPaintHeight() - 2,
         rtLabel,
         PaintAssistant::OutlinedText);

    int harmonic = 2;

    while (harmonic < 100) {

        int hy = int(lrint(getYForFrequency(v, fundamental * harmonic)));
        if (hy < 0 || hy > v->getPaintHeight()) break;
        
        int len = 7;

        if (harmonic % 2 == 0) {
            if (harmonic % 4 == 0) {
                len = 12;
            } else {
                len = 10;
            }
        }

        paint.drawLine(cursorPos.x() - len,
                       hy,
                       cursorPos.x(),
                       hy);

        ++harmonic;
    }

    paint.restore();
}

QString
SpectrogramLayer::getFeatureDescription(LayerGeometryProvider *v, QPoint &pos) const
{
    int x = pos.x();
    int y = pos.y();

    auto model = ModelById::getAs<DenseTimeValueModel>(m_model);
    if (!model || !model->isOK()) return "";

    double magMin = 0, magMax = 0;
    double phaseMin = 0, phaseMax = 0;
    double freqMin = 0, freqMax = 0;
    double adjFreqMin = 0, adjFreqMax = 0;
    QString pitchMin, pitchMax;
    RealTime rtMin, rtMax;

    bool haveValues = false;

    if (!getXBinSourceRange(v, x, rtMin, rtMax)) {
        return "";
    }
    if (getXYBinSourceRange(v, x, y, magMin, magMax, phaseMin, phaseMax)) {
        haveValues = true;
    }

    QString adjFreqText = "", adjPitchText = "";

    if (m_binDisplay == BinDisplay::PeakFrequencies) {

        if (!getAdjustedYBinSourceRange(v, x, y, freqMin, freqMax,
                                        adjFreqMin, adjFreqMax)) {
            return "";
        }

        if (adjFreqMin != adjFreqMax) {
            adjFreqText = tr("Peak Frequency:\t%1 - %2 Hz\n")
                .arg(adjFreqMin).arg(adjFreqMax);
        } else {
            adjFreqText = tr("Peak Frequency:\t%1 Hz\n")
                .arg(adjFreqMin);
        }

        QString pmin = Pitch::getPitchLabelForFrequency(adjFreqMin);
        QString pmax = Pitch::getPitchLabelForFrequency(adjFreqMax);

        if (pmin != pmax) {
            adjPitchText = tr("Peak Pitch:\t%3 - %4\n").arg(pmin).arg(pmax);
        } else {
            adjPitchText = tr("Peak Pitch:\t%2\n").arg(pmin);
        }

    } else {
        
        if (!getYBinSourceRange(v, y, freqMin, freqMax)) return "";
    }

    QString text;

    if (rtMin != rtMax) {
        text += tr("Time:\t%1 - %2\n")
            .arg(rtMin.toText(true).c_str())
            .arg(rtMax.toText(true).c_str());
    } else {
        text += tr("Time:\t%1\n")
            .arg(rtMin.toText(true).c_str());
    }

    if (freqMin != freqMax) {
        text += tr("%1Bin Frequency:\t%2 - %3 Hz\n%4Bin Pitch:\t%5 - %6\n")
            .arg(adjFreqText)
            .arg(freqMin)
            .arg(freqMax)
            .arg(adjPitchText)
            .arg(Pitch::getPitchLabelForFrequency(freqMin))
            .arg(Pitch::getPitchLabelForFrequency(freqMax));
    } else {
        text += tr("%1Bin Frequency:\t%2 Hz\n%3Bin Pitch:\t%4\n")
            .arg(adjFreqText)
            .arg(freqMin)
            .arg(adjPitchText)
            .arg(Pitch::getPitchLabelForFrequency(freqMin));
    }   

    if (haveValues) {
        double dbMin = AudioLevel::multiplier_to_dB(magMin);
        double dbMax = AudioLevel::multiplier_to_dB(magMax);
        QString dbMinString;
        QString dbMaxString;
        if (dbMin == AudioLevel::DB_FLOOR) {
            dbMinString = Strings::minus_infinity;
        } else {
            dbMinString = QString("%1").arg(lrint(dbMin));
        }
        if (dbMax == AudioLevel::DB_FLOOR) {
            dbMaxString = Strings::minus_infinity;
        } else {
            dbMaxString = QString("%1").arg(lrint(dbMax));
        }
        if (lrint(dbMin) != lrint(dbMax)) {
            text += tr("dB:\t%1 - %2").arg(dbMinString).arg(dbMaxString);
        } else {
            text += tr("dB:\t%1").arg(dbMinString);
        }
        if (phaseMin != phaseMax) {
            text += tr("\nPhase:\t%1 - %2").arg(phaseMin).arg(phaseMax);
        } else {
            text += tr("\nPhase:\t%1").arg(phaseMin);
        }
    }

    return text;
}

int
SpectrogramLayer::getColourScaleWidth(QPainter &paint) const
{
    int cw;

    cw = paint.fontMetrics().width("-80dB");

    return cw;
}

int
SpectrogramLayer::getVerticalScaleWidth(LayerGeometryProvider *, bool detailed, QPainter &paint) const
{
    auto model = ModelById::getAs<DenseTimeValueModel>(m_model);
    if (!model || !model->isOK()) return 0;

    int cw = 0;
    if (detailed) cw = getColourScaleWidth(paint);

    int tw = paint.fontMetrics().width(QString("%1")
                                     .arg(m_maxFrequency > 0 ?
                                          m_maxFrequency - 1 :
                                          model->getSampleRate() / 2));

    int fw = paint.fontMetrics().width(tr("43Hz"));
    if (tw < fw) tw = fw;

    int tickw = (m_binScale == BinScale::Log ? 10 : 4);
    
    return cw + tickw + tw + 13;
}

void
SpectrogramLayer::paintVerticalScale(LayerGeometryProvider *v, bool detailed,
                                     QPainter &paint, QRect rect) const
{
    auto model = ModelById::getAs<DenseTimeValueModel>(m_model);
    if (!model || !model->isOK()) {
        return;
    }

    Profiler profiler("SpectrogramLayer::paintVerticalScale");

    //!!! cache this?
    
    int h = rect.height(), w = rect.width();
    int textHeight = paint.fontMetrics().height();

    if (detailed && (h > textHeight * 3 + 10)) {
        paintDetailedScale(v, paint, rect);
    }
    m_haveDetailedScale = detailed;

    int tickw = (m_binScale == BinScale::Log ? 10 : 4);
    int pkw = (m_binScale == BinScale::Log ? 10 : 0);

    int bins = getFFTSize() / 2;
    sv_samplerate_t sr = model->getSampleRate();

    if (m_maxFrequency > 0) {
        bins = int((double(m_maxFrequency) * getFFTSize()) / sr + 0.1);
        if (bins > getFFTSize() / 2) bins = getFFTSize() / 2;
    }

    int cw = 0;
    if (detailed) cw = getColourScaleWidth(paint);

    int py = -1;
    int toff = -textHeight + paint.fontMetrics().ascent() + 2;

    paint.drawLine(cw + 7, 0, cw + 7, h);

    int bin = -1;

    for (int y = 0; y < v->getPaintHeight(); ++y) {

        double q0, q1;
        if (!getYBinRange(v, v->getPaintHeight() - y, q0, q1)) continue;

        int vy;

        if (int(q0) > bin) {
            vy = y;
            bin = int(q0);
        } else {
            continue;
        }

        int freq = int((sr * bin) / getFFTSize());

        if (py >= 0 && (vy - py) < textHeight - 1) {
            if (m_binScale == BinScale::Linear) {
                paint.drawLine(w - tickw, h - vy, w, h - vy);
            }
            continue;
        }

        QString text = QString("%1").arg(freq);
        if (bin == 1) text = tr("%1Hz").arg(freq); // bin 0 is DC
        paint.drawLine(cw + 7, h - vy, w - pkw - 1, h - vy);

        if (h - vy - textHeight >= -2) {
            int tx = w - 3 - paint.fontMetrics().width(text) - max(tickw, pkw);
            paint.drawText(tx, h - vy + toff, text);
        }

        py = vy;
    }

    if (m_binScale == BinScale::Log) {

        // piano keyboard

        PianoScale().paintPianoVertical
            (v, paint, QRect(w - pkw - 1, 0, pkw, h),
             getEffectiveMinFrequency(), getEffectiveMaxFrequency());
    }

    m_haveDetailedScale = detailed;
}

void
SpectrogramLayer::paintDetailedScale(LayerGeometryProvider *v,
                                     QPainter &paint, QRect rect) const
{
    // The colour scale

    if (m_colourScale == ColourScaleType::Phase) {
        paintDetailedScalePhase(v, paint, rect);
        return;
    }
    
    int h = rect.height();
    int textHeight = paint.fontMetrics().height();
    int toff = -textHeight + paint.fontMetrics().ascent() + 2;

    int cw = getColourScaleWidth(paint);
    int cbw = paint.fontMetrics().width("dB");

    int topLines = 2;

    int ch = h - textHeight * (topLines + 1) - 8;
//      paint.drawRect(4, textHeight + 4, cw - 1, ch + 1);
    paint.drawRect(4 + cw - cbw, textHeight * topLines + 4, cbw - 1, ch + 1);

    QString top, bottom;
    double min = m_viewMags[v->getId()].getMin();
    double max = m_viewMags[v->getId()].getMax();

    if (min < m_threshold) min = m_threshold;
    if (max <= min) max = min + 0.1;
        
    double dBmin = AudioLevel::multiplier_to_dB(min);
    double dBmax = AudioLevel::multiplier_to_dB(max);

#ifdef DEBUG_SPECTROGRAM_REPAINT
    cerr << "paintVerticalScale: for view id " << v->getId()
         << ": min = " << min << ", max = " << max
         << ", dBmin = " << dBmin << ", dBmax = " << dBmax << endl;
#endif
        
    if (dBmax < -60.f) dBmax = -60.f;
    else top = QString("%1").arg(lrint(dBmax));

    if (dBmin < dBmax - 60.f) dBmin = dBmax - 60.f;
    bottom = QString("%1").arg(lrint(dBmin));

#ifdef DEBUG_SPECTROGRAM_REPAINT
    cerr << "adjusted dB range to min = " << dBmin << ", max = " << dBmax
         << endl;
#endif
        
    paint.drawText((cw + 6 - paint.fontMetrics().width("dBFS")) / 2,
                   2 + textHeight + toff, "dBFS");

    paint.drawText(3 + cw - cbw - paint.fontMetrics().width(top),
                   2 + textHeight * topLines + toff + textHeight/2, top);

    paint.drawText(3 + cw - cbw - paint.fontMetrics().width(bottom),
                   h + toff - 3 - textHeight/2, bottom);

    paint.save();
    paint.setBrush(Qt::NoBrush);

    int lasty = 0;
    int lastdb = 0;

    for (int i = 0; i < ch; ++i) {

        double dBval = dBmin + (((dBmax - dBmin) * i) / (ch - 1));
        int idb = int(dBval);

        double value = AudioLevel::dB_to_multiplier(dBval);
        paint.setPen(getRenderer(v)->getColour(value));

        int y = textHeight * topLines + 4 + ch - i;

        paint.drawLine(5 + cw - cbw, y, cw + 2, y);
        
        if (i == 0) {
            lasty = y;
            lastdb = idb;
        } else if (i < ch - paint.fontMetrics().ascent() &&
                   idb != lastdb &&
                   ((abs(y - lasty) > textHeight && 
                     idb % 10 == 0) ||
                    (abs(y - lasty) > paint.fontMetrics().ascent() && 
                     idb % 5 == 0))) {
            paint.setPen(v->getForeground());
            QString text = QString("%1").arg(idb);
            paint.drawText(3 + cw - cbw - paint.fontMetrics().width(text),
                           y + toff + textHeight/2, text);
            paint.drawLine(5 + cw - cbw, y, 8 + cw - cbw, y);
            lasty = y;
            lastdb = idb;
        }
    }
    paint.restore();
}

void
SpectrogramLayer::paintDetailedScalePhase(LayerGeometryProvider *v,
                                          QPainter &paint, QRect rect) const
{
    // The colour scale in phase mode
    
    int h = rect.height();
    int textHeight = paint.fontMetrics().height();
    int toff = -textHeight + paint.fontMetrics().ascent() + 2;

    int cw = getColourScaleWidth(paint);

    // Phase is not measured in dB of course, but this places the
    // scale at the same position as in the magnitude spectrogram
    int cbw = paint.fontMetrics().width("dB");

    int topLines = 1;

    int ch = h - textHeight * (topLines + 1) - 8;
    paint.drawRect(4 + cw - cbw, textHeight * topLines + 4, cbw - 1, ch + 1);

    QString top = Strings::pi, bottom = Strings::minus_pi, middle = "0";
    
    double min = -M_PI;
    double max =  M_PI;

    paint.drawText(3 + cw - cbw - paint.fontMetrics().width(top),
                   2 + textHeight * topLines + toff + textHeight/2, top);

    paint.drawText(3 + cw - cbw - paint.fontMetrics().width(middle),
                   2 + textHeight * topLines + ch/2 + toff + textHeight/2, middle);

    paint.drawText(3 + cw - cbw - paint.fontMetrics().width(bottom),
                   h + toff - 3 - textHeight/2, bottom);

    paint.save();
    paint.setBrush(Qt::NoBrush);

    for (int i = 0; i < ch; ++i) {
        double val = min + (((max - min) * i) / (ch - 1));
        paint.setPen(getRenderer(v)->getColour(val));
        int y = textHeight * topLines + 4 + ch - i;
        paint.drawLine(5 + cw - cbw, y, cw + 2, y);
    }
    paint.restore();
}

class SpectrogramRangeMapper : public RangeMapper
{
public:
    SpectrogramRangeMapper(sv_samplerate_t sr, int /* fftsize */) :
        m_dist(sr / 2),
        m_s2(sqrt(sqrt(2))) { }
    ~SpectrogramRangeMapper() override { }
    
    int getPositionForValue(double value) const override {

        double dist = m_dist;
    
        int n = 0;

        while (dist > (value + 0.00001) && dist > 0.1) {
            dist /= m_s2;
            ++n;
        }

        return n;
    }
    
    int getPositionForValueUnclamped(double value) const override {
        // We don't really support this
        return getPositionForValue(value);
    }

    double getValueForPosition(int position) const override {

        // Vertical zoom step 0 shows the entire range from DC ->
        // Nyquist frequency.  Step 1 shows 2^(1/4) of the range of
        // step 0, and so on until the visible range is smaller than
        // the frequency step between bins at the current fft size.

        double dist = m_dist;
    
        int n = 0;
        while (n < position) {
            dist /= m_s2;
            ++n;
        }

        return dist;
    }
    
    double getValueForPositionUnclamped(int position) const override {
        // We don't really support this
        return getValueForPosition(position);
    }

    QString getUnit() const override { return "Hz"; }

protected:
    double m_dist;
    double m_s2;
};

int
SpectrogramLayer::getVerticalZoomSteps(int &defaultStep) const
{
    auto model = ModelById::getAs<DenseTimeValueModel>(m_model);
    if (!model) return 0;

    sv_samplerate_t sr = model->getSampleRate();

    SpectrogramRangeMapper mapper(sr, getFFTSize());

//    int maxStep = mapper.getPositionForValue((double(sr) / getFFTSize()) + 0.001);
    int maxStep = mapper.getPositionForValue(0);
    int minStep = mapper.getPositionForValue(double(sr) / 2);

    int initialMax = m_initialMaxFrequency;
    if (initialMax == 0) initialMax = int(sr / 2);

    defaultStep = mapper.getPositionForValue(initialMax) - minStep;

//    SVDEBUG << "SpectrogramLayer::getVerticalZoomSteps: " << maxStep - minStep << " (" << maxStep <<"-" << minStep << "), default is " << defaultStep << " (from initial max freq " << initialMax << ")" << endl;

    return maxStep - minStep;
}

int
SpectrogramLayer::getCurrentVerticalZoomStep() const
{
    auto model = ModelById::getAs<DenseTimeValueModel>(m_model);
    if (!model) return 0;

    double dmin, dmax;
    getDisplayExtents(dmin, dmax);
    
    SpectrogramRangeMapper mapper(model->getSampleRate(), getFFTSize());
    int n = mapper.getPositionForValue(dmax - dmin);
//    SVDEBUG << "SpectrogramLayer::getCurrentVerticalZoomStep: " << n << endl;
    return n;
}

void
SpectrogramLayer::setVerticalZoomStep(int step)
{
    auto model = ModelById::getAs<DenseTimeValueModel>(m_model);
    if (!model) return;

    double dmin = m_minFrequency, dmax = m_maxFrequency;
//    getDisplayExtents(dmin, dmax);

//    cerr << "current range " << dmin << " -> " << dmax << ", range " << dmax-dmin << ", mid " << (dmax + dmin)/2 << endl;
    
    sv_samplerate_t sr = model->getSampleRate();
    SpectrogramRangeMapper mapper(sr, getFFTSize());
    double newdist = mapper.getValueForPosition(step);

    double newmin, newmax;

    if (m_binScale == BinScale::Log) {

        // need to pick newmin and newmax such that
        //
        // (log(newmin) + log(newmax)) / 2 == logmid
        // and
        // newmax - newmin = newdist
        //
        // so log(newmax - newdist) + log(newmax) == 2logmid
        // log(newmax(newmax - newdist)) == 2logmid
        // newmax.newmax - newmax.newdist == exp(2logmid)
        // newmax^2 + (-newdist)newmax + -exp(2logmid) == 0
        // quadratic with a = 1, b = -newdist, c = -exp(2logmid), all known
        // 
        // positive root
        // newmax = (newdist + sqrt(newdist^2 + 4exp(2logmid))) / 2
        //
        // but logmid = (log(dmin) + log(dmax)) / 2
        // so exp(2logmid) = exp(log(dmin) + log(dmax))
        // = exp(log(dmin.dmax))
        // = dmin.dmax
        // so newmax = (newdist + sqrtf(newdist^2 + 4dmin.dmax)) / 2

        newmax = (newdist + sqrt(newdist*newdist + 4*dmin*dmax)) / 2;
        newmin = newmax - newdist;

//        cerr << "newmin = " << newmin << ", newmax = " << newmax << endl;

    } else {
        double dmid = (dmax + dmin) / 2;
        newmin = dmid - newdist / 2;
        newmax = dmid + newdist / 2;
    }

    double mmin, mmax;
    mmin = 0;
    mmax = double(sr) / 2;
    
    if (newmin < mmin) {
        newmax += (mmin - newmin);
        newmin = mmin;
    }
    if (newmax > mmax) {
        newmax = mmax;
    }
    
//    SVDEBUG << "SpectrogramLayer::setVerticalZoomStep: " << step << ": " << newmin << " -> " << newmax << " (range " << newdist << ")" << endl;

    setMinFrequency(int(lrint(newmin)));
    setMaxFrequency(int(lrint(newmax)));
}

RangeMapper *
SpectrogramLayer::getNewVerticalZoomRangeMapper() const
{
    auto model = ModelById::getAs<DenseTimeValueModel>(m_model);
    if (!model) return nullptr;
    return new SpectrogramRangeMapper(model->getSampleRate(), getFFTSize());
}

void
SpectrogramLayer::updateMeasureRectYCoords(LayerGeometryProvider *v, const MeasureRect &r) const
{
    int y0 = 0;
    if (r.startY > 0.0) y0 = int(getYForFrequency(v, r.startY));
    
    int y1 = y0;
    if (r.endY > 0.0) y1 = int(getYForFrequency(v, r.endY));

//    SVDEBUG << "SpectrogramLayer::updateMeasureRectYCoords: start " << r.startY << " -> " << y0 << ", end " << r.endY << " -> " << y1 << endl;

    r.pixrect = QRect(r.pixrect.x(), y0, r.pixrect.width(), y1 - y0);
}

void
SpectrogramLayer::setMeasureRectYCoord(LayerGeometryProvider *v, MeasureRect &r, bool start, int y) const
{
    if (start) {
        r.startY = getFrequencyForY(v, y);
        r.endY = r.startY;
    } else {
        r.endY = getFrequencyForY(v, y);
    }
//    SVDEBUG << "SpectrogramLayer::setMeasureRectYCoord: start " << r.startY << " <- " << y << ", end " << r.endY << " <- " << y << endl;

}

void
SpectrogramLayer::toXml(QTextStream &stream,
                        QString indent, QString extraAttributes) const
{
    QString s;
    
    s += QString("channel=\"%1\" "
                 "windowSize=\"%2\" "
                 "windowHopLevel=\"%3\" "
                 "oversampling=\"%4\" "
                 "gain=\"%5\" "
                 "threshold=\"%6\" ")
        .arg(m_channel)
        .arg(m_windowSize)
        .arg(m_windowHopLevel)
        .arg(m_oversampling)
        .arg(m_gain)
        .arg(m_threshold);

    s += QString("minFrequency=\"%1\" "
                 "maxFrequency=\"%2\" "
                 "colourScale=\"%3\" "
                 "colourRotation=\"%4\" "
                 "frequencyScale=\"%5\" "
                 "binDisplay=\"%6\" ")
        .arg(m_minFrequency)
        .arg(m_maxFrequency)
        .arg(convertFromColourScale(m_colourScale, m_colourScaleMultiple))
        .arg(m_colourRotation)
        .arg(int(m_binScale))
        .arg(int(m_binDisplay));

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
        .arg(m_normalization == ColumnNormalization::Max1 ? "peak" :
             m_normalization == ColumnNormalization::Hybrid ? "hybrid" : "none");

    // Old-style normalization attribute. We *don't* write out
    // normalizeHybrid here because the only release that would accept
    // it (Tony v1.0) has a totally different scale factor for
    // it. We'll just have to accept that session files from Tony
    // v2.0+ will look odd in Tony v1.0
    
    s += QString("normalizeColumns=\"%1\" ")
        .arg(m_normalization == ColumnNormalization::Max1 ? "true" : "false");

    // And this applies to both old- and new-style attributes
    
    s += QString("normalizeVisibleArea=\"%1\" ")
        .arg(m_normalizeVisibleArea ? "true" : "false");
    
    Layer::toXml(stream, indent, extraAttributes + " " + s);
}

void
SpectrogramLayer::setProperties(const QXmlAttributes &attributes)
{
    bool ok = false;

    int channel = attributes.value("channel").toInt(&ok);
    if (ok) setChannel(channel);

    int windowSize = attributes.value("windowSize").toUInt(&ok);
    if (ok) setWindowSize(windowSize);

    int windowHopLevel = attributes.value("windowHopLevel").toUInt(&ok);
    if (ok) setWindowHopLevel(windowHopLevel);
    else {
        int windowOverlap = attributes.value("windowOverlap").toUInt(&ok);
        // a percentage value
        if (ok) {
            if (windowOverlap == 0) setWindowHopLevel(0);
            else if (windowOverlap == 25) setWindowHopLevel(1);
            else if (windowOverlap == 50) setWindowHopLevel(2);
            else if (windowOverlap == 75) setWindowHopLevel(3);
            else if (windowOverlap == 90) setWindowHopLevel(4);
        }
    }

    int oversampling = attributes.value("oversampling").toUInt(&ok);
    if (ok) setOversampling(oversampling);

    float gain = attributes.value("gain").toFloat(&ok);
    if (ok) setGain(gain);

    float threshold = attributes.value("threshold").toFloat(&ok);
    if (ok) setThreshold(threshold);

    int minFrequency = attributes.value("minFrequency").toUInt(&ok);
    if (ok) {
        SVDEBUG << "SpectrogramLayer::setProperties: setting min freq to " << minFrequency << endl;
        setMinFrequency(minFrequency);
    }

    int maxFrequency = attributes.value("maxFrequency").toUInt(&ok);
    if (ok) {
        SVDEBUG << "SpectrogramLayer::setProperties: setting max freq to " << maxFrequency << endl;
        setMaxFrequency(maxFrequency);
    }

    auto colourScale = convertToColourScale
        (attributes.value("colourScale").toInt(&ok));
    if (ok) {
        setColourScale(colourScale.first);
        setColourScaleMultiple(colourScale.second);
    }

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

    int colourRotation = attributes.value("colourRotation").toInt(&ok);
    if (ok) setColourRotation(colourRotation);

    BinScale binScale = (BinScale)
        attributes.value("frequencyScale").toInt(&ok);
    if (ok) setBinScale(binScale);

    BinDisplay binDisplay = (BinDisplay)
        attributes.value("binDisplay").toInt(&ok);
    if (ok) setBinDisplay(binDisplay);

    bool haveNewStyleNormalization = false;
    
    QString columnNormalization = attributes.value("columnNormalization");

    if (columnNormalization != "") {

        haveNewStyleNormalization = true;

        if (columnNormalization == "peak") {
            setNormalization(ColumnNormalization::Max1);
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

        bool normalizeColumns =
            (attributes.value("normalizeColumns").trimmed() == "true");
        if (normalizeColumns) {
            setNormalization(ColumnNormalization::Max1);
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

    if (!haveNewStyleNormalization && m_normalization == ColumnNormalization::Hybrid) {
        // Tony v1.0 is (and hopefully will remain!) the only released
        // SV-a-like to use old-style attributes when saving sessions
        // that ask for hybrid normalization. It saves them with the
        // wrong gain factor, so hack in a fix for that here -- this
        // gives us backward but not forward compatibility.
        setGain(m_gain / float(getFFTSize() / 2));
    }
}
    
