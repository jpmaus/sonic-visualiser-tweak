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

#include "TimeValueLayer.h"

#include "data/model/Model.h"
#include "base/RealTime.h"
#include "base/Profiler.h"
#include "base/LogRange.h"
#include "base/RangeMapper.h"
#include "base/Pitch.h"
#include "view/View.h"

#include "data/model/SparseTimeValueModel.h"
#include "data/model/Labeller.h"

#include "widgets/ItemEditDialog.h"
#include "widgets/ListInputDialog.h"
#include "widgets/TextAbbrev.h"

#include "ColourDatabase.h"
#include "ColourMapper.h"
#include "PianoScale.h"
#include "LinearNumericalScale.h"
#include "LogNumericalScale.h"
#include "LinearColourScale.h"
#include "LogColourScale.h"
#include "PaintAssistant.h"

#include <QPainter>
#include <QPainterPath>
#include <QMouseEvent>
#include <QRegExp>
#include <QTextStream>
#include <QMessageBox>
#include <QInputDialog>

#include <iostream>
#include <cmath>

//#define DEBUG_TIME_VALUE_LAYER 1

TimeValueLayer::TimeValueLayer() :
    SingleColourLayer(),
    m_editing(false),
    m_originalPoint(0, 0.0, tr("New Point")),
    m_editingPoint(0, 0.0, tr("New Point")),
    m_editingCommand(nullptr),
    m_colourMap(0),
    m_colourInverted(false),
    m_plotStyle(PlotConnectedPoints),
    m_verticalScale(AutoAlignScale),
    m_drawSegmentDivisions(true),
    m_derivative(false),
    m_scaleMinimum(0),
    m_scaleMaximum(0)
{
    
}

int
TimeValueLayer::getCompletion(LayerGeometryProvider *) const
{
    auto model = ModelById::get(m_model);
    if (model) return model->getCompletion();
    else return 0;
}

void
TimeValueLayer::setModel(ModelId modelId)
{
    auto newModel = ModelById::getAs<SparseTimeValueModel>(modelId);
    
    if (!modelId.isNone() && !newModel) {
        throw std::logic_error("Not a SparseTimeValueModel");
    }
    
    if (m_model == modelId) return;
    m_model = modelId;

    if (newModel) {
        
        connectSignals(m_model);

        m_scaleMinimum = 0;
        m_scaleMaximum = 0;

        if (newModel->getRDFTypeURI().endsWith("Segment")) {
            setPlotStyle(PlotSegmentation);
        }
        if (newModel->getRDFTypeURI().endsWith("Change")) {
            setPlotStyle(PlotSegmentation);
        }
    }
    
    emit modelReplaced();
}

Layer::PropertyList
TimeValueLayer::getProperties() const
{
    PropertyList list = SingleColourLayer::getProperties();
    list.push_back("Plot Type");
    list.push_back("Vertical Scale");
    list.push_back("Scale Units");
    list.push_back("Draw Segment Division Lines");
    list.push_back("Show Derivative");
    return list;
}

QString
TimeValueLayer::getPropertyLabel(const PropertyName &name) const
{
    if (name == "Plot Type") return tr("Plot Type");
    if (name == "Vertical Scale") return tr("Vertical Scale");
    if (name == "Scale Units") return tr("Scale Units");
    if (name == "Draw Segment Division Lines") return tr("Draw Segment Division Lines");
    if (name == "Show Derivative") return tr("Show Derivative");
    return SingleColourLayer::getPropertyLabel(name);
}

QString
TimeValueLayer::getPropertyIconName(const PropertyName &name) const
{
    if (name == "Draw Segment Division Lines") return "lines";
    if (name == "Show Derivative") return "derivative";
    return "";
}

Layer::PropertyType
TimeValueLayer::getPropertyType(const PropertyName &name) const
{
    if (name == "Plot Type") return ValueProperty;
    if (name == "Vertical Scale") return ValueProperty;
    if (name == "Scale Units") return UnitsProperty;
    if (name == "Colour" && m_plotStyle == PlotSegmentation) return ColourMapProperty;
    if (name == "Draw Segment Division Lines") return ToggleProperty;
    if (name == "Show Derivative") return ToggleProperty;
    return SingleColourLayer::getPropertyType(name);
}

QString
TimeValueLayer::getPropertyGroupName(const PropertyName &name) const
{
    if (name == "Vertical Scale" || name == "Scale Units") {
        return tr("Scale");
    }
    if (name == "Plot Type" || name == "Draw Segment Division Lines" ||
        name == "Show Derivative") {
        return tr("Plot Type");
    }
    return SingleColourLayer::getPropertyGroupName(name);
}

bool
TimeValueLayer::needsTextLabelHeight() const
{
    auto model = ModelById::getAs<SparseTimeValueModel>(m_model);
    if (!model) return false;
    return m_plotStyle == PlotSegmentation && model->hasTextLabels();
}

QString
TimeValueLayer::getScaleUnits() const
{
    auto model = ModelById::getAs<SparseTimeValueModel>(m_model);
    if (model) return model->getScaleUnits();
    else return "";
}

int
TimeValueLayer::getPropertyRangeAndValue(const PropertyName &name,
                                         int *min, int *max, int *deflt) const
{
    int val = 0;

    if (name == "Colour" && m_plotStyle == PlotSegmentation) {
            
        if (min) *min = 0;
        if (max) *max = ColourMapper::getColourMapCount() - 1;
        if (deflt) *deflt = 0;
        
        val = m_colourMap;

    } else if (name == "Plot Type") {
        
        if (min) *min = 0;
        if (max) *max = 6;
        if (deflt) *deflt = int(PlotConnectedPoints);
        
        val = int(m_plotStyle);

    } else if (name == "Vertical Scale") {
        
        if (min) *min = 0;
        if (max) *max = 3;
        if (deflt) *deflt = int(AutoAlignScale);
        
        val = int(m_verticalScale);

    } else if (name == "Scale Units") {

        if (deflt) *deflt = 0;
        auto model = ModelById::getAs<SparseTimeValueModel>(m_model);
        if (model) {
            val = UnitDatabase::getInstance()->getUnitId
                (getScaleUnits());
        }

    } else if (name == "Draw Segment Division Lines") {

        if (min) *min = 0;
        if (max) *max = 1;
        if (deflt) *deflt = 1;
        val = (m_drawSegmentDivisions ? 1.0 : 0.0);

    } else if (name == "Show Derivative") {

        if (min) *min = 0;
        if (max) *max = 1;
        if (deflt) *deflt = 0;
        val = (m_derivative ? 1.0 : 0.0);

    } else {
        
        val = SingleColourLayer::getPropertyRangeAndValue(name, min, max, deflt);
    }

    return val;
}

QString
TimeValueLayer::getPropertyValueLabel(const PropertyName &name,
                                    int value) const
{
    if (name == "Colour" && m_plotStyle == PlotSegmentation) {
        return ColourMapper::getColourMapLabel(value);
    } else if (name == "Plot Type") {
        switch (value) {
        default:
        case 0: return tr("Points");
        case 1: return tr("Stems");
        case 2: return tr("Connected Points");
        case 3: return tr("Lines");
        case 4: return tr("Curve");
        case 5: return tr("Segmentation");
        case 6: return tr("Discrete Curves");
        }
    } else if (name == "Vertical Scale") {
        switch (value) {
        default:
        case 0: return tr("Auto-Align");
        case 1: return tr("Linear");
        case 2: return tr("Log");
        case 3: return tr("+/-1");
        }
    }
    return SingleColourLayer::getPropertyValueLabel(name, value);
}

void
TimeValueLayer::setProperty(const PropertyName &name, int value)
{
    if (name == "Colour" && m_plotStyle == PlotSegmentation) {
        setFillColourMap(value);
    } else if (name == "Plot Type") {
        setPlotStyle(PlotStyle(value));
    } else if (name == "Vertical Scale") {
        setVerticalScale(VerticalScale(value));
    } else if (name == "Scale Units") {
        auto model = ModelById::getAs<SparseTimeValueModel>(m_model);
        if (model) {
            model->setScaleUnits
                (UnitDatabase::getInstance()->getUnitById(value));
            emit modelChanged(m_model);
        }
    } else if (name == "Draw Segment Division Lines") {
        setDrawSegmentDivisions(value > 0.5);
    } else if (name == "Show Derivative") {
        setShowDerivative(value > 0.5);
    } else {
        SingleColourLayer::setProperty(name, value);
    }
}

void
TimeValueLayer::setFillColourMap(int map)
{
    if (m_colourMap == map) return;
    m_colourMap = map;
    emit layerParametersChanged();
}

void
TimeValueLayer::setPlotStyle(PlotStyle style)
{
    if (m_plotStyle == style) return;
    bool colourTypeChanged = (style == PlotSegmentation ||
                              m_plotStyle == PlotSegmentation);
    m_plotStyle = style;
    if (colourTypeChanged) {
        emit layerParameterRangesChanged();
    }
    emit layerParametersChanged();
}

void
TimeValueLayer::setVerticalScale(VerticalScale scale)
{
    if (m_verticalScale == scale) return;
    m_verticalScale = scale;
    emit layerParametersChanged();
}

void
TimeValueLayer::setDrawSegmentDivisions(bool draw)
{
    if (m_drawSegmentDivisions == draw) return;
    m_drawSegmentDivisions = draw;
    emit layerParametersChanged();
}

void
TimeValueLayer::setShowDerivative(bool show)
{
    if (m_derivative == show) return;
    m_derivative = show;
    emit layerParametersChanged();
}

bool
TimeValueLayer::isLayerScrollable(const LayerGeometryProvider *v) const
{
    // We don't illuminate sections in the line or curve modes, so
    // they're always scrollable

    if (m_plotStyle == PlotLines ||
        m_plotStyle == PlotCurve ||
        m_plotStyle == PlotDiscreteCurves) return true;

    QPoint discard;
    return !v->shouldIlluminateLocalFeatures(this, discard);
}

bool
TimeValueLayer::getValueExtents(double &min, double &max,
                                bool &logarithmic, QString &unit) const
{
    auto model = ModelById::getAs<SparseTimeValueModel>(m_model);
    if (!model) return false;

    min = model->getValueMinimum();
    max = model->getValueMaximum();

    logarithmic = (m_verticalScale == LogScale);

    unit = getScaleUnits();

    if (m_derivative) {
        max = std::max(fabs(min), fabs(max));
        min = -max;
    }

#ifdef DEBUG_TIME_VALUE_LAYER
    cerr << "TimeValueLayer::getValueExtents: min = " << min << ", max = " << max << endl;
#endif

    if (!shouldAutoAlign() && !logarithmic && !m_derivative) {

        if (max == min) {
            max = max + 0.5;
            min = min - 0.5;
        } else {
            double margin = (max - min) / 10.0;
            max = max + margin;
            min = min - margin;
        }

#ifdef DEBUG_TIME_VALUE_LAYER
        cerr << "TimeValueLayer::getValueExtents: min = " << min << ", max = " << max << " (after adjustment)" << endl;
#endif
    }

    return true;
}

bool
TimeValueLayer::getDisplayExtents(double &min, double &max) const
{
    auto model = ModelById::getAs<SparseTimeValueModel>(m_model);
    if (!model || shouldAutoAlign()) return false;

    if (m_scaleMinimum == m_scaleMaximum) {
        bool log;
        QString unit;
        getValueExtents(min, max, log, unit);
    } else {
        min = m_scaleMinimum;
        max = m_scaleMaximum;
    }

    if (m_derivative) {
        max = std::max(fabs(min), fabs(max));
        min = -max;
    }

#ifdef DEBUG_TIME_VALUE_LAYER
    cerr << "TimeValueLayer::getDisplayExtents: min = " << min << ", max = " << max << endl;
#endif

    return true;
}

bool
TimeValueLayer::setDisplayExtents(double min, double max)
{
    auto model = ModelById::getAs<SparseTimeValueModel>(m_model);
    if (!model) return false;

    if (min == max) {
        if (min == 0.f) {
            max = 1.f;
        } else {
            max = min * 1.0001;
        }
    }

    m_scaleMinimum = min;
    m_scaleMaximum = max;

#ifdef DEBUG_TIME_VALUE_LAYER
    cerr << "TimeValueLayer::setDisplayExtents: min = " << min << ", max = " << max << endl;
#endif
    
    emit layerParametersChanged();
    return true;
}

int
TimeValueLayer::getVerticalZoomSteps(int &defaultStep) const
{
    if (shouldAutoAlign()) return 0;
    auto model = ModelById::getAs<SparseTimeValueModel>(m_model);
    if (!model) return 0;

    defaultStep = 0;
    return 100;
}

int
TimeValueLayer::getCurrentVerticalZoomStep() const
{
    if (shouldAutoAlign()) return 0;
    auto model = ModelById::getAs<SparseTimeValueModel>(m_model);
    if (!model) return 0;

    RangeMapper *mapper = getNewVerticalZoomRangeMapper();
    if (!mapper) return 0;

    double dmin, dmax;
    getDisplayExtents(dmin, dmax);

    int nr = mapper->getPositionForValue(dmax - dmin);

#ifdef DEBUG_TIME_VALUE_LAYER
    cerr << "TimeValueLayer::getCurrentVerticalZoomStep: dmin = " << dmin << ", dmax = " << dmax << ", nr = " << nr << endl;
#endif

    delete mapper;

    return 100 - nr;
}

void
TimeValueLayer::setVerticalZoomStep(int step)
{
    if (shouldAutoAlign()) return;
    auto model = ModelById::getAs<SparseTimeValueModel>(m_model);
    if (!model) return;

    RangeMapper *mapper = getNewVerticalZoomRangeMapper();
    if (!mapper) return;
    
    double min, max;
    bool logarithmic;
    QString unit;
    getValueExtents(min, max, logarithmic, unit);
    
    double dmin, dmax;
    getDisplayExtents(dmin, dmax);

    double newdist = mapper->getValueForPosition(100 - step);

    double newmin, newmax;

    if (logarithmic) {

        // see SpectrogramLayer::setVerticalZoomStep

        newmax = (newdist + sqrt(newdist*newdist + 4*dmin*dmax)) / 2;
        newmin = newmax - newdist;

#ifdef DEBUG_TIME_VALUE_LAYER
        cerr << "newmin = " << newmin << ", newmax = " << newmax << endl;
#endif

    } else {
        double dmid = (dmax + dmin) / 2;
        newmin = dmid - newdist / 2;
        newmax = dmid + newdist / 2;
    }

    if (newmin < min) {
        newmax += (min - newmin);
        newmin = min;
    }
    if (newmax > max) {
        newmax = max;
    }
    
#ifdef DEBUG_TIME_VALUE_LAYER
    cerr << "TimeValueLayer::setVerticalZoomStep: " << step << ": " << newmin << " -> " << newmax << " (range " << newdist << ")" << endl;
#endif

    setDisplayExtents(newmin, newmax);
}

RangeMapper *
TimeValueLayer::getNewVerticalZoomRangeMapper() const
{
    auto model = ModelById::getAs<SparseTimeValueModel>(m_model);
    if (!model) return nullptr;
    
    RangeMapper *mapper;

    double min, max;
    bool logarithmic;
    QString unit;
    getValueExtents(min, max, logarithmic, unit);

    if (min == max) return nullptr;
    
    if (logarithmic) {
        mapper = new LogRangeMapper(0, 100, min, max, unit);
    } else {
        mapper = new LinearRangeMapper(0, 100, min, max, unit);
    }

    return mapper;
}

EventVector
TimeValueLayer::getLocalPoints(LayerGeometryProvider *v, int x) const
{
    auto model = ModelById::getAs<SparseTimeValueModel>(m_model);
    if (!model) return {};

    // Return all points at a frame f, where f is the closest frame to
    // pixel coordinate x whose pixel coordinate is both within a
    // small (but somewhat arbitrary) fuzz distance from x and within
    // the current view. If there is no such frame, return an empty
    // vector.
    
    sv_frame_t frame = v->getFrameForX(x);
    
    EventVector exact = model->getEventsStartingAt(frame);
    if (!exact.empty()) return exact;

    // overspill == 1, so one event either side of the given span
    EventVector neighbouring = model->getEventsWithin
        (frame, model->getResolution(), 1);

    double fuzz = v->scaleSize(2);
    sv_frame_t suitable = 0;
    bool have = false;
    
    for (Event e: neighbouring) {
        sv_frame_t f = e.getFrame();
        if (f < v->getStartFrame() || f > v->getEndFrame()) {
            continue;
        }
        int px = v->getXForFrame(f);
        if ((px > x && px - x > fuzz) || (px < x && x - px > fuzz + 3)) {
            continue;
        }
        if (!have) {
            suitable = f;
            have = true;
        } else if (llabs(frame - f) < llabs(suitable - f)) {
            suitable = f;
        }
    }

    if (have) {
        return model->getEventsStartingAt(suitable);
    } else {
        return {};
    }
}

QString
TimeValueLayer::getLabelPreceding(sv_frame_t frame) const
{
    auto model = ModelById::getAs<SparseTimeValueModel>(m_model);
    if (!model || !model->hasTextLabels()) return "";

    Event e;
    if (model->getNearestEventMatching
        (frame,
         [](Event e) { return e.hasLabel() && e.getLabel() != ""; },
         EventSeries::Backward,
         e)) {
        return e.getLabel();
    }

    return "";
}

QString
TimeValueLayer::getFeatureDescription(LayerGeometryProvider *v, QPoint &pos) const
{
    int x = pos.x();

    auto model = ModelById::getAs<SparseTimeValueModel>(m_model);
    if (!model || !model->getSampleRate()) return "";

    EventVector points = getLocalPoints(v, x);

    if (points.empty()) {
        if (!model->isReady()) {
            return tr("In progress");
        } else {
            return tr("No local points");
        }
    }

    sv_frame_t useFrame = points.begin()->getFrame();

    RealTime rt = RealTime::frame2RealTime(useFrame, model->getSampleRate());
    
    QString valueText;
    float value = points.begin()->getValue();
    QString unit = getScaleUnits();

    if (unit == "Hz") {
        valueText = tr("%1 Hz (%2, %3)")
            .arg(value)
            .arg(Pitch::getPitchLabelForFrequency(value))
            .arg(Pitch::getPitchForFrequency(value));
    } else if (unit != "") {
        valueText = tr("%1 %2").arg(value).arg(unit);
    } else {
        valueText = tr("%1").arg(value);
    }
    
    QString text;

    if (points.begin()->getLabel() == "") {
        text = QString(tr("Time:\t%1\nValue:\t%2\nNo label"))
            .arg(rt.toText(true).c_str())
            .arg(valueText);
    } else {
        text = QString(tr("Time:\t%1\nValue:\t%2\nLabel:\t%4"))
            .arg(rt.toText(true).c_str())
            .arg(valueText)
            .arg(points.begin()->getLabel());
    }

    pos = QPoint(v->getXForFrame(useFrame),
                 getYForValue(v, points.begin()->getValue()));
    return text;
}

bool
TimeValueLayer::snapToFeatureFrame(LayerGeometryProvider *v,
                                   sv_frame_t &frame,
                                   int &resolution,
                                   SnapType snap, int ycoord) const
{
    auto model = ModelById::getAs<SparseTimeValueModel>(m_model);
    if (!model) {
        return Layer::snapToFeatureFrame(v, frame, resolution, snap, ycoord);
    }

    // SnapLeft / SnapRight: return frame of nearest feature in that
    // direction no matter how far away
    //
    // SnapNeighbouring: return frame of feature that would be used in
    // an editing operation, i.e. closest feature in either direction
    // but only if it is "close enough"
    
    resolution = model->getResolution();

    if (snap == SnapNeighbouring) {
        EventVector points = getLocalPoints(v, v->getXForFrame(frame));
        if (points.empty()) return false;
        frame = points.begin()->getFrame();
        return true;
    }

    Event e;
    if (model->getNearestEventMatching
        (frame,
         [](Event) { return true; },
         snap == SnapLeft ? EventSeries::Backward : EventSeries::Forward,
         e)) {
        frame = e.getFrame();
        return true;
    }

    return false;
}

bool
TimeValueLayer::snapToSimilarFeature(LayerGeometryProvider *v,
                                     sv_frame_t &frame,
                                     int &resolution,
                                     SnapType snap) const
{
    auto model = ModelById::getAs<SparseTimeValueModel>(m_model);
    if (!model) {
        return Layer::snapToSimilarFeature(v, frame, resolution, snap);
    }

    // snap is only permitted to be SnapLeft or SnapRight here.
    
    resolution = model->getResolution();

    Event ref;
    Event e;
    float matchvalue;
    bool found;

    found = model->getNearestEventMatching
        (frame, [](Event) { return true; }, EventSeries::Backward, ref);

    if (!found) {
        return false;
    }

    matchvalue = ref.getValue();
    
    found = model->getNearestEventMatching
        (frame,
         [matchvalue](Event e) {
             double epsilon = 0.0001;
             return fabs(e.getValue() - matchvalue) < epsilon;
         },
         snap == SnapLeft ? EventSeries::Backward : EventSeries::Forward,
         e);

    if (!found) {
        return false;
    }

    frame = e.getFrame();
    return true;
}

void
TimeValueLayer::getScaleExtents(LayerGeometryProvider *v, double &min, double &max, bool &log) const
{
    min = 0.0;
    max = 0.0;
    log = false;

    auto model = ModelById::getAs<SparseTimeValueModel>(m_model);
    if (!model) return;

    if (shouldAutoAlign()) {

        if (!v->getVisibleExtentsForUnit(getScaleUnits(), min, max, log)) {
            min = model->getValueMinimum();
            max = model->getValueMaximum();
        } else {
#ifdef DEBUG_TIME_VALUE_LAYER
            SVCERR << "getScaleExtents: view returned min = " << min
                   << ", max = " << max << ", log = " << log << endl;
#endif
            if (log) {
                LogRange::mapRange(min, max);
#ifdef DEBUG_TIME_VALUE_LAYER
                SVCERR << "getScaleExtents: mapped to min = " << min
                       << ", max = " << max << endl;
#endif
            }
        }

    } else if (m_verticalScale == PlusMinusOneScale) {

        min = -1.0;
        max = 1.0;

    } else {

        getDisplayExtents(min, max);
        
        if (m_verticalScale == LogScale) {
            LogRange::mapRange(min, max);
            log = true;
        }
    }

#ifdef DEBUG_TIME_VALUE_LAYER
    cerr << "TimeValueLayer::getScaleExtents: min = " << min << ", max = " << max << endl;
#endif
}

int
TimeValueLayer::getYForValue(LayerGeometryProvider *v, double val) const
{
    double min = 0.0, max = 0.0;
    bool logarithmic = false;
    int h = v->getPaintHeight();

    getScaleExtents(v, min, max, logarithmic);

#ifdef DEBUG_TIME_VALUE_LAYER
    SVCERR << "getYForValue(" << val << "): min " << min << ", max "
           << max << ", log " << logarithmic << endl;
#endif

    if (logarithmic) {
        val = LogRange::map(val);
#ifdef DEBUG_TIME_VALUE_LAYER
        SVCERR << "-> " << val << endl;
#endif
    }

    return int(h - ((val - min) * h) / (max - min));
}

double
TimeValueLayer::getValueForY(LayerGeometryProvider *v, int y) const
{
    double min = 0.0, max = 0.0;
    bool logarithmic = false;
    int h = v->getPaintHeight();

    getScaleExtents(v, min, max, logarithmic);

    double val = min + (double(h - y) * double(max - min)) / h;

    if (logarithmic) {
        val = LogRange::map(val);
    }

    return val;
}

bool
TimeValueLayer::shouldAutoAlign() const
{
    QString unit = getScaleUnits();
    return (m_verticalScale == AutoAlignScale && unit != "");
}

QColor
TimeValueLayer::getColourForValue(LayerGeometryProvider *v, double val) const
{
    double min, max;
    bool log;
    getScaleExtents(v, min, max, log);

    if (min > max) std::swap(min, max);
    if (max == min) max = min + 1;

    if (log) {
        val = LogRange::map(val);
    }

#ifdef DEBUG_TIME_VALUE_LAYER
    cerr << "TimeValueLayer::getColourForValue: min " << min << ", max "
              << max << ", log " << log << ", value " << val << endl;
#endif

    QColor solid = ColourMapper(m_colourMap, m_colourInverted, min, max).map(val);
    return QColor(solid.red(), solid.green(), solid.blue(), 120);
}

int
TimeValueLayer::getDefaultColourHint(bool darkbg, bool &impose)
{
    impose = false;
    return ColourDatabase::getInstance()->getColourIndex
        (QString(darkbg ? "Bright Green" : "Green"));
}

void
TimeValueLayer::paint(LayerGeometryProvider *v, QPainter &paint, QRect rect) const
{
    auto model = ModelById::getAs<SparseTimeValueModel>(m_model);
    if (!model || !model->isOK()) return;

    sv_samplerate_t sampleRate = model->getSampleRate();
    if (!sampleRate) return;

#ifdef DEBUG_TIME_VALUE_LAYER
    SVCERR << "TimeValueLayer[" << this << ", model " << getModel() << "]::paint in " << v->getId() << endl;
#endif
    
    paint.setRenderHint(QPainter::Antialiasing, false);

//    Profiler profiler("TimeValueLayer::paint", true);

    int x0 = rect.left();
    int x1 = x0 + rect.width();
    sv_frame_t frame0 = v->getFrameForX(x0);
    sv_frame_t frame1 = v->getFrameForX(x1);
    if (m_derivative) --frame0;

    EventVector points(model->getEventsWithin(frame0, frame1 - frame0, 1));

#ifdef DEBUG_TIME_VALUE_LAYER
    SVCERR << "TimeValueLayer[" << this << "]::paint in " << v->getId()
           << ": pixel extents " << x0 << " to " << x1 << ", frame extents "
           << frame0 << " to " << frame1 << " yielding " << points.size()
           << " points (of " << model->getAllEvents().size()
           << " from frames " << model->getStartFrame() << " to "
           << model->getEndFrame() << ")" << endl;
#endif
    
    if (points.empty()) return;

    paint.setPen(getBaseQColor());

    QColor brushColour(getBaseQColor());
    brushColour.setAlpha(80);
    paint.setBrush(brushColour);

#ifdef DEBUG_TIME_VALUE_LAYER
    cerr << "TimeValueLayer::paint: resolution is "
         << model->getResolution() << " frames" << endl;
#endif

    double min = model->getValueMinimum();
    double max = model->getValueMaximum();
    if (max == min) max = min + 1.0;

    int origin = int(nearbyint(v->getPaintHeight() -
                               (-min * v->getPaintHeight()) / (max - min)));

    QPoint localPos;
    sv_frame_t illuminateFrame = -1;

    if (v->shouldIlluminateLocalFeatures(this, localPos)) {
        EventVector localPoints = getLocalPoints(v, localPos.x());
#ifdef DEBUG_TIME_VALUE_LAYER
        cerr << "TimeValueLayer: " << localPoints.size() << " local points" << endl;
#endif
        if (!localPoints.empty()) {
            illuminateFrame = localPoints.begin()->getFrame();
        }
    }

    int w =
        v->getXForFrame(frame0 + model->getResolution()) -
        v->getXForFrame(frame0);

    if (m_plotStyle == PlotStems) {
        if (w < 2) w = 2;
    } else {
        if (w < 1) w = 1;
    }

    paint.save();

    QPainterPath path;
    int pointCount = 0;

    int textY = 0;
    if (m_plotStyle == PlotSegmentation) {
        textY = v->getTextLabelYCoord(this, paint);
    } else {
        int originY = getYForValue(v, 0.f);
        if (originY > 0 && originY < v->getPaintHeight()) {
            paint.save();
            paint.setPen(getPartialShades(v)[1]);
            paint.drawLine(x0, originY, x1, originY);
            paint.restore();
        }
    }
    
    sv_frame_t prevFrame = 0;

    for (EventVector::const_iterator i = points.begin();
         i != points.end(); ++i) {

        if (m_derivative && i == points.begin()) continue;

        Event p(*i);

        double value = p.getValue();
        if (m_derivative) {
            EventVector::const_iterator j = i;
            --j;
            value -= j->getValue();
        }

        int x = v->getXForFrame(p.getFrame());
        int y = getYForValue(v, value);

        bool gap = false;
        if (m_plotStyle == PlotDiscreteCurves) { 
            if (value == 0.0) {
                // Treat zeros as gaps
                continue;
            }
            gap = (p.getFrame() > prevFrame &&
                   (p.getFrame() - prevFrame >= model->getResolution() * 2));
        }

        if (m_plotStyle != PlotSegmentation) {
            textY = y - paint.fontMetrics().height()
                      + paint.fontMetrics().ascent() - 1;
            if (textY < paint.fontMetrics().ascent() + 1) {
                textY = paint.fontMetrics().ascent() + 1;
            }
        }

        bool haveNext = false;
        double nvalue = 0.f;
        sv_frame_t nf = v->getModelsEndFrame();
        int nx = v->getXForFrame(nf);
        int ny = y;

        EventVector::const_iterator j = i;
        ++j;

        if (j != points.end()) {
            Event q(*j);
            nvalue = q.getValue();
            if (m_derivative) nvalue -= p.getValue();
            nf = q.getFrame();
            nx = v->getXForFrame(nf);
            ny = getYForValue(v, nvalue);
            haveNext = true;
        }

//        cout << "frame = " << p.getFrame() << ", x = " << x << ", haveNext = " << haveNext 
//                  << ", nx = " << nx << endl;

        QPen pen(getBaseQColor());
        QBrush brush(brushColour);
        
        if (m_plotStyle == PlotDiscreteCurves) {
            pen = QPen(getBaseQColor(), 3);
            brush = QBrush(Qt::NoBrush);
        } else if (m_plotStyle == PlotSegmentation) {
            pen = QPen(getForegroundQColor(v));
            brush = QBrush(getColourForValue(v, value));
        } else if (m_plotStyle == PlotLines ||
                   m_plotStyle == PlotCurve) {
            brush = QBrush(Qt::NoBrush);
        }
        
        paint.setPen(v->scalePen(pen));
        paint.setBrush(brush);
        
        if (m_plotStyle == PlotStems) {
            if (y < origin - 1) {
                paint.drawLine(x + w/2, y + 1, x + w/2, origin);
            } else if (y > origin + 1) {
                paint.drawLine(x + w/2, origin, x + w/2, y - 1);
            }
        }

        bool illuminate = false;

        if (illuminateFrame == p.getFrame()) {

            // not equipped to illuminate the right section in line
            // or curve mode

            if (m_plotStyle != PlotCurve &&
                m_plotStyle != PlotDiscreteCurves &&
                m_plotStyle != PlotLines) {
                illuminate = true;
            }
        }

        if (m_plotStyle != PlotLines &&
            m_plotStyle != PlotCurve &&
            m_plotStyle != PlotDiscreteCurves &&
            m_plotStyle != PlotSegmentation) {
            if (illuminate) {
                paint.save();
                paint.setPen(v->scalePen(getForegroundQColor(v)));
                paint.setBrush(getForegroundQColor(v));
            }
            if (m_plotStyle != PlotStems ||
                w > 1) {
                paint.drawRect(x, y - 1, w, 2);
            }
            if (illuminate) {
                paint.restore();
            }
        }

        if (m_plotStyle == PlotConnectedPoints ||
            m_plotStyle == PlotLines ||
            m_plotStyle == PlotDiscreteCurves ||
            m_plotStyle == PlotCurve) {

            if (haveNext) {

                if (m_plotStyle == PlotConnectedPoints) {
                    
                    paint.save();
                    paint.setPen(v->scalePen(brushColour));
                    paint.drawLine(x + w, y, nx, ny);
                    paint.restore();

                } else if (m_plotStyle == PlotLines) {
                    
                    if (pointCount == 0) {
                        path.moveTo(x + w/2, y);
                    }

//                    paint.drawLine(x + w/2, y, nx + w/2, ny);
                    path.lineTo(nx + w/2, ny);

                } else {

                    double x0 = x + double(w)/2;
                    double x1 = nx + double(w)/2;
                    
                    double y0 = y;
                    double y1 = ny;

                    if (m_plotStyle == PlotDiscreteCurves) {
                        bool nextGap =
                            (nvalue == 0.0) ||
                            (nf - p.getFrame() >= model->getResolution() * 2);
                        if (nextGap) {
                            x1 = x0;
                            y1 = y0;
                        }
                    }

                    if (pointCount == 0 || gap) {
                        path.moveTo((x0 + x1) / 2, (y0 + y1) / 2);
                    }

                    if (nx - x > 5) {
                        path.cubicTo(x0, y0,
                                     x0, y0,
                                     (x0 + x1) / 2, (y0 + y1) / 2);

                        // // or
                        // path.quadTo(x0, y0, (x0 + x1) / 2, (y0 + y1) / 2);

                    } else {
                        path.lineTo(x0, y0);
                        path.lineTo((x0 + x1) / 2, (y0 + y1) / 2);
                    }
                }
            }
        }

        if (m_plotStyle == PlotSegmentation) {

#ifdef DEBUG_TIME_VALUE_LAYER
            cerr << "drawing rect" << endl;
#endif
            
            if (nx <= x) continue;

            paint.setPen(v->scalePen(QPen(getForegroundQColor(v), 2)));

            if (!illuminate) {
                if (!m_drawSegmentDivisions ||
                    nx < x + 5 ||
                    x >= v->getPaintWidth() - 1) {
                    paint.setPen(Qt::NoPen);
                }
            }

            paint.drawRect(x, -1, nx - x, v->getPaintHeight() + 1);
        }

        if (v->shouldShowFeatureLabels()) {

            QString label = p.getLabel();
            bool italic = false;

            if (label == "" &&
                (m_plotStyle == PlotPoints ||
                 m_plotStyle == PlotSegmentation ||
                 m_plotStyle == PlotConnectedPoints)) {
                char lc[20];
                snprintf(lc, 20, "%.3g", p.getValue());
                label = lc;
                italic = true;
            }

    // Qt 5.13 deprecates QFontMetrics::width(), but its suggested
    // replacement (horizontalAdvance) was only added in Qt 5.11
    // which is too new for us
#pragma GCC diagnostic ignored "-Wdeprecated-declarations"

            if (label != "") {
                // Quick test for 20px before we do the slower test using metrics
                bool haveRoom = (nx > x + 20);
                haveRoom = (haveRoom &&
                            (nx > x + 6 + paint.fontMetrics().width(label)));
                if (haveRoom ||
                    (!haveNext &&
                     (pointCount == 0 || !italic))) {
                    PaintAssistant::drawVisibleText
                        (v, paint, x + 5, textY, label,
                         italic ?
                         PaintAssistant::OutlinedItalicText :
                         PaintAssistant::OutlinedText);
                }
            }
        }

        prevFrame = p.getFrame();
        ++pointCount;
    }

    if (m_plotStyle == PlotDiscreteCurves) {
        paint.setRenderHint(QPainter::Antialiasing, true);
        paint.drawPath(path);
    } else if ((m_plotStyle == PlotCurve || m_plotStyle == PlotLines)
               && !path.isEmpty()) {
        paint.setRenderHint(QPainter::Antialiasing, pointCount <= v->getPaintWidth());
        paint.drawPath(path);
    }

    paint.restore();

    // looks like save/restore doesn't deal with this:
    paint.setRenderHint(QPainter::Antialiasing, false);
}

int
TimeValueLayer::getVerticalScaleWidth(LayerGeometryProvider *v, bool, QPainter &paint) const
{
    auto model = ModelById::getAs<SparseTimeValueModel>(m_model);
    if (!model) {
        return 0;
    } else if (shouldAutoAlign() && !valueExtentsMatchMine(v)) {
        return 0;
    } else if (m_plotStyle == PlotSegmentation) {
        if (m_verticalScale == LogScale) {
            return LogColourScale().getWidth(v, paint);
        } else {
            return LinearColourScale().getWidth(v, paint);
        }
    } else {
        if (m_verticalScale == LogScale) {
            return LogNumericalScale().getWidth(v, paint) + 10; // for piano
        } else {
            return LinearNumericalScale().getWidth(v, paint);
        }
    }
}

void
TimeValueLayer::paintVerticalScale(LayerGeometryProvider *v, bool, QPainter &paint, QRect) const
{
    auto model = ModelById::getAs<SparseTimeValueModel>(m_model);
    if (!model || model->isEmpty()) return;

    QString unit;
    double min, max;
    bool logarithmic;

    int w = getVerticalScaleWidth(v, false, paint);
    int h = v->getPaintHeight();

    if (m_plotStyle == PlotSegmentation) {

        getValueExtents(min, max, logarithmic, unit);

        if (logarithmic) {
            LogRange::mapRange(min, max);
            LogColourScale().paintVertical(v, this, paint, 0, min, max);
        } else {
            LinearColourScale().paintVertical(v, this, paint, 0, min, max);
        }

    } else {

        getScaleExtents(v, min, max, logarithmic);

        if (logarithmic) {
            LogNumericalScale().paintVertical(v, this, paint, 0, min, max);
        } else {
            LinearNumericalScale().paintVertical(v, this, paint, 0, min, max);
        }

        if (logarithmic && (getScaleUnits() == "Hz")) {
            PianoScale().paintPianoVertical
                (v, paint, QRect(w - 10, 0, 10, h), 
                 LogRange::unmap(min), 
                 LogRange::unmap(max));
            paint.drawLine(w, 0, w, h);
        }
    }
        
    if (getScaleUnits() != "") {
        int mw = w - 5;
        paint.drawText(5,
                       5 + paint.fontMetrics().ascent(),
                       TextAbbrev::abbreviate(getScaleUnits(),
                                              paint.fontMetrics(),
                                              mw));
    }
}

void
TimeValueLayer::drawStart(LayerGeometryProvider *v, QMouseEvent *e)
{
#ifdef DEBUG_TIME_VALUE_LAYER
    cerr << "TimeValueLayer::drawStart(" << e->x() << "," << e->y() << ")" << endl;
#endif

    auto model = ModelById::getAs<SparseTimeValueModel>(m_model);
    if (!model) return;

    sv_frame_t frame = v->getFrameForX(e->x());
    int resolution = model->getResolution();
    if (frame < 0) frame = 0;
    frame = (frame / resolution) * resolution;

    double value = getValueForY(v, e->y());

    bool havePoint = false;

    EventVector points = getLocalPoints(v, e->x());
    if (!points.empty()) {
        for (EventVector::iterator i = points.begin();
             i != points.end(); ++i) {
            if (((i->getFrame() / resolution) * resolution) != frame) {
#ifdef DEBUG_TIME_VALUE_LAYER
                cerr << "ignoring out-of-range frame at " << i->getFrame() << endl;
#endif
                continue;
            }
            m_editingPoint = *i;
            havePoint = true;
        }
    }

    if (!havePoint) {
        m_editingPoint = Event(frame, float(value), tr("New Point"));
    }

    m_originalPoint = m_editingPoint;

    if (m_editingCommand) finish(m_editingCommand);
    m_editingCommand = new ChangeEventsCommand(m_model.untyped, tr("Draw Point"));
    if (!havePoint) {
        m_editingCommand->add(m_editingPoint);
    }

    m_editing = true;
}

void
TimeValueLayer::drawDrag(LayerGeometryProvider *v, QMouseEvent *e)
{
#ifdef DEBUG_TIME_VALUE_LAYER
    cerr << "TimeValueLayer::drawDrag(" << e->x() << "," << e->y() << ")" << endl;
#endif

    auto model = ModelById::getAs<SparseTimeValueModel>(m_model);
    if (!model || !m_editing) return;

    sv_frame_t frame = v->getFrameForX(e->x());
    int resolution = model->getResolution();
    if (frame < 0) frame = 0;
    frame = (frame / resolution) * resolution;

    double value = getValueForY(v, e->y());

    EventVector points = getLocalPoints(v, e->x());

#ifdef DEBUG_TIME_VALUE_LAYER
    cerr << points.size() << " points" << endl;
#endif

    bool havePoint = false;

    if (!points.empty()) {
        for (EventVector::iterator i = points.begin();
             i != points.end(); ++i) {
            if (i->getFrame() == m_editingPoint.getFrame() &&
                i->getValue() == m_editingPoint.getValue()) {
#ifdef DEBUG_TIME_VALUE_LAYER
                cerr << "ignoring current editing point at " << i->getFrame() << ", " << i->getValue() << endl;
#endif
                continue;
            }
            if (((i->getFrame() / resolution) * resolution) != frame) {
#ifdef DEBUG_TIME_VALUE_LAYER
                cerr << "ignoring out-of-range frame at " << i->getFrame() << endl;
#endif
                continue;
            }
#ifdef DEBUG_TIME_VALUE_LAYER
            cerr << "adjusting to new point at " << i->getFrame() << ", " << i->getValue() << endl;
#endif
            m_editingPoint = *i;
            m_originalPoint = m_editingPoint;
            m_editingCommand->remove(m_editingPoint);
            havePoint = true;
        }
    }

    if (!havePoint) {
        if (frame == m_editingPoint.getFrame()) {
            m_editingCommand->remove(m_editingPoint);
        }
    }

    m_editingPoint = m_editingPoint
        .withFrame(frame)
        .withValue(float(value));
    m_editingCommand->add(m_editingPoint);
}

void
TimeValueLayer::drawEnd(LayerGeometryProvider *, QMouseEvent *)
{
#ifdef DEBUG_TIME_VALUE_LAYER
    cerr << "TimeValueLayer::drawEnd" << endl;
#endif
    auto model = ModelById::getAs<SparseTimeValueModel>(m_model);
    if (!model || !m_editing) return;
    finish(m_editingCommand);
    m_editingCommand = nullptr;
    m_editing = false;
}

void
TimeValueLayer::eraseStart(LayerGeometryProvider *v, QMouseEvent *e)
{
    auto model = ModelById::getAs<SparseTimeValueModel>(m_model);
    if (!model) return;

    EventVector points = getLocalPoints(v, e->x());
    if (points.empty()) return;

    m_editingPoint = *points.begin();

    if (m_editingCommand) {
        finish(m_editingCommand);
        m_editingCommand = nullptr;
    }

    m_editing = true;
}

void
TimeValueLayer::eraseDrag(LayerGeometryProvider *, QMouseEvent *)
{
}

void
TimeValueLayer::eraseEnd(LayerGeometryProvider *v, QMouseEvent *e)
{
    auto model = ModelById::getAs<SparseTimeValueModel>(m_model);
    if (!model || !m_editing) return;

    m_editing = false;

    EventVector points = getLocalPoints(v, e->x());
    if (points.empty()) return;
    if (points.begin()->getFrame() != m_editingPoint.getFrame() ||
        points.begin()->getValue() != m_editingPoint.getValue()) return;

    m_editingCommand = new ChangeEventsCommand(m_model.untyped, tr("Erase Point"));
    m_editingCommand->remove(m_editingPoint);
    finish(m_editingCommand);
    m_editingCommand = nullptr;
    m_editing = false;
}

void
TimeValueLayer::editStart(LayerGeometryProvider *v, QMouseEvent *e)
{
#ifdef DEBUG_TIME_VALUE_LAYER
    cerr << "TimeValueLayer::editStart(" << e->x() << "," << e->y() << ")" << endl;
#endif

    auto model = ModelById::getAs<SparseTimeValueModel>(m_model);
    if (!model) return;

    EventVector points = getLocalPoints(v, e->x());
    if (points.empty()) return;

    m_editingPoint = *points.begin();
    m_originalPoint = m_editingPoint;

    if (m_editingCommand) {
        finish(m_editingCommand);
        m_editingCommand = nullptr;
    }

    m_editing = true;
}

void
TimeValueLayer::editDrag(LayerGeometryProvider *v, QMouseEvent *e)
{
#ifdef DEBUG_TIME_VALUE_LAYER
    cerr << "TimeValueLayer::editDrag(" << e->x() << "," << e->y() << ")" << endl;
#endif

    auto model = ModelById::getAs<SparseTimeValueModel>(m_model);
    if (!model || !m_editing) return;

    sv_frame_t frame = v->getFrameForX(e->x());
    if (frame < 0) frame = 0;
    frame = frame / model->getResolution() * model->getResolution();

    double value = getValueForY(v, e->y());

    if (!m_editingCommand) {
        m_editingCommand = new ChangeEventsCommand(m_model.untyped, tr("Drag Point"));
    }

    m_editingCommand->remove(m_editingPoint);
    m_editingPoint = m_editingPoint
        .withFrame(frame)
        .withValue(float(value));
    m_editingCommand->add(m_editingPoint);
}

void
TimeValueLayer::editEnd(LayerGeometryProvider *, QMouseEvent *)
{
#ifdef DEBUG_TIME_VALUE_LAYER
    cerr << "TimeValueLayer::editEnd" << endl;
#endif
    auto model = ModelById::getAs<SparseTimeValueModel>(m_model);
    if (!model || !m_editing) return;

    if (m_editingCommand) {

        QString newName = m_editingCommand->getName();

        if (m_editingPoint.getFrame() != m_originalPoint.getFrame()) {
            if (m_editingPoint.getValue() != m_originalPoint.getValue()) {
                newName = tr("Edit Point");
            } else {
                newName = tr("Relocate Point");
            }
        } else {
            newName = tr("Change Point Value");
        }

        m_editingCommand->setName(newName);
        finish(m_editingCommand);
    }

    m_editingCommand = nullptr;
    m_editing = false;
}

bool
TimeValueLayer::editOpen(LayerGeometryProvider *v, QMouseEvent *e)
{
    auto model = ModelById::getAs<SparseTimeValueModel>(m_model);
    if (!model) return false;

    EventVector points = getLocalPoints(v, e->x());
    if (points.empty()) return false;

    Event point = *points.begin();

    ItemEditDialog *dialog = new ItemEditDialog
        (model->getSampleRate(),
         ItemEditDialog::ShowTime |
         ItemEditDialog::ShowValue |
         ItemEditDialog::ShowText,
         getScaleUnits());

    dialog->setFrameTime(point.getFrame());
    dialog->setValue(point.getValue());
    dialog->setText(point.getLabel());

    if (dialog->exec() == QDialog::Accepted) {

        Event newPoint = point
            .withFrame(dialog->getFrameTime())
            .withValue(dialog->getValue())
            .withLabel(dialog->getText());
        
        ChangeEventsCommand *command =
            new ChangeEventsCommand(m_model.untyped, tr("Edit Point"));
        command->remove(point);
        command->add(newPoint);
        finish(command);
    }

    delete dialog;
    return true;
}

void
TimeValueLayer::moveSelection(Selection s, sv_frame_t newStartFrame)
{
    auto model = ModelById::getAs<SparseTimeValueModel>(m_model);
    if (!model) return;

    ChangeEventsCommand *command =
        new ChangeEventsCommand(m_model.untyped, tr("Drag Selection"));

    EventVector points =
        model->getEventsWithin(s.getStartFrame(), s.getDuration());

    for (Event p: points) {

        Event newPoint = p.withFrame
            (p.getFrame() + newStartFrame - s.getStartFrame());
        command->remove(p);
        command->add(newPoint);
    }

    finish(command);
}

void
TimeValueLayer::resizeSelection(Selection s, Selection newSize)
{
    auto model = ModelById::getAs<SparseTimeValueModel>(m_model);
    if (!model || !s.getDuration()) return;

    ChangeEventsCommand *command =
        new ChangeEventsCommand(m_model.untyped, tr("Resize Selection"));

    EventVector points =
        model->getEventsWithin(s.getStartFrame(), s.getDuration());

    double ratio = double(newSize.getDuration()) / double(s.getDuration());
    double oldStart = double(s.getStartFrame());
    double newStart = double(newSize.getStartFrame());

    for (Event p: points) {
        
        double newFrame = (double(p.getFrame()) - oldStart) * ratio + newStart;

        Event newPoint = p
            .withFrame(lrint(newFrame));
        command->remove(p);
        command->add(newPoint);
    }

    finish(command);
}

void
TimeValueLayer::deleteSelection(Selection s)
{
    auto model = ModelById::getAs<SparseTimeValueModel>(m_model);
    if (!model) return;

    ChangeEventsCommand *command =
        new ChangeEventsCommand(m_model.untyped, tr("Delete Selected Points"));

    EventVector points =
        model->getEventsWithin(s.getStartFrame(), s.getDuration());

    for (Event p: points) {
        command->remove(p);
    }

    finish(command);
}    

void
TimeValueLayer::copy(LayerGeometryProvider *v, Selection s, Clipboard &to)
{
    auto model = ModelById::getAs<SparseTimeValueModel>(m_model);
    if (!model) return;

    EventVector points =
        model->getEventsWithin(s.getStartFrame(), s.getDuration());

    for (Event p: points) {
        to.addPoint(p.withReferenceFrame(alignToReference(v, p.getFrame())));
    }
}

bool
TimeValueLayer::paste(LayerGeometryProvider *v, const Clipboard &from,
                      sv_frame_t /* frameOffset */, bool interactive)
{
    auto model = ModelById::getAs<SparseTimeValueModel>(m_model);
    if (!model) return false;

    EventVector points = from.getPoints();

    bool realign = false;

    if (clipboardHasDifferentAlignment(v, from)) {

        QMessageBox::StandardButton button =
            QMessageBox::question(v->getView(), tr("Re-align pasted items?"),
                                  tr("The items you are pasting came from a layer with different source material from this one.  Do you want to re-align them in time, to match the source material for this layer?"),
                                  QMessageBox::Yes | QMessageBox::No | QMessageBox::Cancel,
                                  QMessageBox::Yes);

        if (button == QMessageBox::Cancel) {
            return false;
        }

        if (button == QMessageBox::Yes) {
            realign = true;
        }
    }

    ChangeEventsCommand *command =
        new ChangeEventsCommand(m_model.untyped, tr("Paste"));

    enum ValueAvailability {
        UnknownAvailability,
        NoValues,
        SomeValues,
        AllValues
    };

    Labeller::ValueType generation = Labeller::ValueNone;

    bool haveUsableLabels = false;
    Labeller labeller;
    labeller.setSampleRate(model->getSampleRate());

    if (interactive) {

        ValueAvailability availability = UnknownAvailability;

        for (EventVector::const_iterator i = points.begin();
             i != points.end(); ++i) {
        
            if (availability == UnknownAvailability) {
                if (i->hasValue()) availability = AllValues;
                else availability = NoValues;
                continue;
            }

            if (i->hasValue()) {
                if (availability == NoValues) {
                    availability = SomeValues;
                }
            } else {
                if (availability == AllValues) {
                    availability = SomeValues;
                }
            }

            if (!haveUsableLabels) {
                if (i->hasLabel()) {
                    if (i->getLabel().contains(QRegExp("[0-9]"))) {
                        haveUsableLabels = true;
                    }
                }
            }

            if (availability == SomeValues && haveUsableLabels) break;
        }

        if (availability == NoValues || availability == SomeValues) {
            
            QString text;
            if (availability == NoValues) {
                text = tr("The items you are pasting do not have values.\nWhat values do you want to use for these items?");
            } else {
                text = tr("Some of the items you are pasting do not have values.\nWhat values do you want to use for these items?");
            }

            Labeller::TypeNameMap names = labeller.getTypeNames();

            QStringList options;
            std::vector<Labeller::ValueType> genopts;

            for (Labeller::TypeNameMap::const_iterator i = names.begin();
                 i != names.end(); ++i) {
                if (i->first == Labeller::ValueNone) options << tr("Zero for all items");
                else options << i->second;
                genopts.push_back(i->first);
            }

            static int prevSelection = 0;

            bool ok = false;
            QString selected = ListInputDialog::getItem
                (nullptr, tr("Choose value calculation"),
                 text, options, prevSelection, &ok);

            if (!ok) {
                delete command;
                return false;
            }
            int selection = 0;
            generation = Labeller::ValueNone;

            for (QStringList::const_iterator i = options.begin();
                 i != options.end(); ++i) {
                if (selected == *i) {
                    generation = genopts[selection];
                    break;
                }
                ++selection;
            }
            
            labeller.setType(generation);

            if (generation == Labeller::ValueFromCyclicalCounter ||
                generation == Labeller::ValueFromTwoLevelCounter) {
                int cycleSize = QInputDialog::getInt
                    (nullptr, tr("Select cycle size"),
                     tr("Cycle size:"), 4, 2, 16, 1);
                labeller.setCounterCycleSize(cycleSize);
            }

            prevSelection = selection;
        }
    }

    Event prevPoint;

    for (EventVector::const_iterator i = points.begin();
         i != points.end(); ++i) {
        
        sv_frame_t frame = 0;

        if (!realign) {
            
            frame = i->getFrame();

        } else {

            if (i->hasReferenceFrame()) {
                frame = i->getReferenceFrame();
                frame = alignFromReference(v, frame);
            } else {
                frame = i->getFrame();
            }
        }

        Event newPoint = i->withFrame(frame);
        
        if (!i->hasLabel() && i->hasValue()) {
            newPoint = newPoint.withLabel(QString("%1").arg(i->getValue()));
        }

        bool usePrev = false;
        Event formerPrevPoint = prevPoint;

        if (!i->hasValue()) {
#ifdef DEBUG_TIME_VALUE_LAYER
            cerr << "Setting value on point at " << newPoint.getFrame() << " from labeller";
            if (i == points.begin()) {
                cerr << ", no prev point" << endl;
            } else {
                cerr << ", prev point is at " << prevPoint.getFrame() << endl;
            }
#endif

            Labeller::Revaluing valuing = 
                labeller.revalue
                (newPoint, (i == points.begin()) ? nullptr : &prevPoint);
            
#ifdef DEBUG_TIME_VALUE_LAYER
            cerr << "New point value = " << newPoint.getValue() << endl;
#endif
            if (valuing.first == Labeller::AppliesToPreviousEvent) {
                usePrev = true;
                prevPoint = valuing.second;
            } else {
                newPoint = valuing.second;
            }
        }

        if (usePrev) {
            command->remove(formerPrevPoint);
            command->add(prevPoint);
        }

        prevPoint = newPoint;
        command->add(newPoint);
    }

    finish(command);
    return true;
}

void
TimeValueLayer::toXml(QTextStream &stream,
                      QString indent, QString extraAttributes) const
{
    QString s;

    s += QString("plotStyle=\"%1\" "
                 "verticalScale=\"%2\" "
                 "scaleMinimum=\"%3\" "
                 "scaleMaximum=\"%4\" "
                 "drawDivisions=\"%5\" "
                 "derivative=\"%6\" ")
        .arg(m_plotStyle)
        .arg(m_verticalScale)
        .arg(m_scaleMinimum)
        .arg(m_scaleMaximum)
        .arg(m_drawSegmentDivisions ? "true" : "false")
        .arg(m_derivative ? "true" : "false");
    
    // New-style colour map attribute, by string id rather than by
    // number

    s += QString("fillColourMap=\"%1\" ")
        .arg(ColourMapper::getColourMapId(m_colourMap));

    // Old-style colour map attribute

    s += QString("colourMap=\"%1\" ")
        .arg(ColourMapper::getBackwardCompatibilityColourMap(m_colourMap));
    
    SingleColourLayer::toXml(stream, indent, extraAttributes + " " + s);
}

void
TimeValueLayer::setProperties(const QXmlAttributes &attributes)
{
    SingleColourLayer::setProperties(attributes);

    bool ok, alsoOk;

    QString colourMapId = attributes.value("fillColourMap");
    int colourMap = ColourMapper::getColourMapById(colourMapId);
    if (colourMap >= 0) {
        setFillColourMap(colourMap);
    } else {
        colourMap = attributes.value("colourMap").toInt(&ok);
        if (ok && colourMap < ColourMapper::getColourMapCount()) {
            setFillColourMap(colourMap);
        }
    }

    PlotStyle style = (PlotStyle)
        attributes.value("plotStyle").toInt(&ok);
    if (ok) setPlotStyle(style);

    VerticalScale scale = (VerticalScale)
        attributes.value("verticalScale").toInt(&ok);
    if (ok) setVerticalScale(scale);

    bool draw = (attributes.value("drawDivisions").trimmed() == "true");
    setDrawSegmentDivisions(draw);

    bool derivative = (attributes.value("derivative").trimmed() == "true");
    setShowDerivative(derivative);

    float min = attributes.value("scaleMinimum").toFloat(&ok);
    float max = attributes.value("scaleMaximum").toFloat(&alsoOk);
#ifdef DEBUG_TIME_VALUE_LAYER
    cerr << "from properties: min = " << min << ", max = " << max << endl;
#endif
    if (ok && alsoOk && min != max) setDisplayExtents(min, max);
}

