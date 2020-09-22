/* -*- c-basic-offset: 4 indent-tabs-mode: nil -*-  vi:set ts=8 sts=4 sw=4: */

/*
    Sonic Visualiser
    An audio file viewer and annotation editor.
    Centre for Digital Music, Queen Mary, University of London.
    This file copyright 2006-2008 Chris Cannam and QMUL.
    
    This program is free software; you can redistribute it and/or
    modify it under the terms of the GNU General Public License as
    published by the Free Software Foundation; either version 2 of the
    License, or (at your option) any later version.  See the file
    COPYING included with this distribution for more information.
*/

#include "RegionLayer.h"

#include "data/model/Model.h"
#include "base/RealTime.h"
#include "base/Profiler.h"
#include "base/LogRange.h"

#include "ColourDatabase.h"
#include "ColourMapper.h"
#include "LinearNumericalScale.h"
#include "LogNumericalScale.h"
#include "LinearColourScale.h"
#include "LogColourScale.h"
#include "PaintAssistant.h"

#include "view/View.h"

#include "data/model/RegionModel.h"

#include "widgets/ItemEditDialog.h"
#include "widgets/TextAbbrev.h"

#include <QPainter>
#include <QPainterPath>
#include <QMouseEvent>
#include <QTextStream>
#include <QMessageBox>

#include <iostream>
#include <cmath>

RegionLayer::RegionLayer() :
    SingleColourLayer(),
    m_editing(false),
    m_dragPointX(0),
    m_dragPointY(0),
    m_dragStartX(0),
    m_dragStartY(0),
    m_originalPoint(0, 0.0, 0, tr("New Region")),
    m_editingPoint(0, 0.0, 0, tr("New Region")),
    m_editingCommand(nullptr),
    m_verticalScale(EqualSpaced),
    m_colourMap(0),
    m_colourInverted(false),
    m_plotStyle(PlotLines)
{
    
}

int
RegionLayer::getCompletion(LayerGeometryProvider *) const
{
    auto model = ModelById::get(m_model);
    if (model) return model->getCompletion();
    else return 0;
}

void
RegionLayer::setModel(ModelId modelId)
{
    auto oldModel = ModelById::getAs<RegionModel>(m_model);
    auto newModel = ModelById::getAs<RegionModel>(modelId);
    
    if (!modelId.isNone() && !newModel) {
        throw std::logic_error("Not a RegionModel");
    }
    
    if (m_model == modelId) return;
    m_model = modelId;

    if (newModel) {
    
        connectSignals(m_model);

        connect(newModel.get(), SIGNAL(modelChanged(ModelId)),
                this, SLOT(recalcSpacing()));
    
        recalcSpacing();

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
RegionLayer::getProperties() const
{
    PropertyList list = SingleColourLayer::getProperties();
    list.push_back("Vertical Scale");
    list.push_back("Scale Units");
    list.push_back("Plot Type");
    return list;
}

QString
RegionLayer::getPropertyLabel(const PropertyName &name) const
{
    if (name == "Vertical Scale") return tr("Vertical Scale");
    if (name == "Scale Units") return tr("Scale Units");
    if (name == "Plot Type") return tr("Plot Type");
    return SingleColourLayer::getPropertyLabel(name);
}

Layer::PropertyType
RegionLayer::getPropertyType(const PropertyName &name) const
{
    if (name == "Scale Units") return UnitsProperty;
    if (name == "Vertical Scale") return ValueProperty;
    if (name == "Plot Type") return ValueProperty;
    if (name == "Colour" && m_plotStyle == PlotSegmentation) return ValueProperty;
    return SingleColourLayer::getPropertyType(name);
}

QString
RegionLayer::getPropertyGroupName(const PropertyName &name) const
{
    if (name == "Vertical Scale" || name == "Scale Units") {
        return tr("Scale");
    }
    return SingleColourLayer::getPropertyGroupName(name);
}

int
RegionLayer::getPropertyRangeAndValue(const PropertyName &name,
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
        if (max) *max = 1;
        if (deflt) *deflt = 0;
        
        val = int(m_plotStyle);

    } else if (name == "Vertical Scale") {
        
        if (min) *min = 0;
        if (max) *max = 3;
        if (deflt) *deflt = int(EqualSpaced);
        
        val = int(m_verticalScale);

    } else if (name == "Scale Units") {

        if (deflt) *deflt = 0;
        auto model = ModelById::getAs<RegionModel>(m_model);
        if (model) {
            val = UnitDatabase::getInstance()->getUnitId
                (model->getScaleUnits());
        }

    } else {

        val = SingleColourLayer::getPropertyRangeAndValue(name, min, max, deflt);
    }

    return val;
}

QString
RegionLayer::getPropertyValueLabel(const PropertyName &name,
                                   int value) const
{
    if (name == "Colour" && m_plotStyle == PlotSegmentation) {
        return ColourMapper::getColourMapLabel(value);
    } else if (name == "Plot Type") {

        switch (value) {
        default:
        case 0: return tr("Bars");
        case 1: return tr("Segmentation");
        }

    } else if (name == "Vertical Scale") {
        switch (value) {
        default:
        case 0: return tr("Auto-Align");
        case 1: return tr("Equal Spaced");
        case 2: return tr("Linear");
        case 3: return tr("Log");
        }
    }
    return SingleColourLayer::getPropertyValueLabel(name, value);
}

void
RegionLayer::setProperty(const PropertyName &name, int value)
{
    if (name == "Colour" && m_plotStyle == PlotSegmentation) {
        setFillColourMap(value);
    } else if (name == "Plot Type") {
        setPlotStyle(PlotStyle(value));
    } else if (name == "Vertical Scale") {
        setVerticalScale(VerticalScale(value));
    } else if (name == "Scale Units") {
        auto model = ModelById::getAs<RegionModel>(m_model);
        if (model) {
            model->setScaleUnits
                (UnitDatabase::getInstance()->getUnitById(value));
            emit modelChanged(m_model);
        }
    } else {
        return SingleColourLayer::setProperty(name, value);
    }
}

void
RegionLayer::setFillColourMap(int map)
{
    if (m_colourMap == map) return;
    m_colourMap = map;
    emit layerParametersChanged();
}

void
RegionLayer::setPlotStyle(PlotStyle style)
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
RegionLayer::setVerticalScale(VerticalScale scale)
{
    if (m_verticalScale == scale) return;
    m_verticalScale = scale;
    emit layerParametersChanged();
}

bool
RegionLayer::isLayerScrollable(const LayerGeometryProvider *v) const
{
    QPoint discard;
    return !v->shouldIlluminateLocalFeatures(this, discard);
}

void
RegionLayer::recalcSpacing()
{
    m_spacingMap.clear();
    m_distributionMap.clear();

    auto model = ModelById::getAs<RegionModel>(m_model);
    if (!model) return;

//    SVDEBUG << "RegionLayer::recalcSpacing" << endl;

    EventVector allEvents = model->getAllEvents();
    for (const Event &e: allEvents) {
        m_distributionMap[e.getValue()]++;
//        SVDEBUG << "RegionLayer::recalcSpacing: value found: " << e.getValue() << " (now have " << m_distributionMap[e.getValue()] << " of this value)" <<  endl;
    }

    int n = 0;

    for (SpacingMap::const_iterator i = m_distributionMap.begin();
         i != m_distributionMap.end(); ++i) {
        m_spacingMap[i->first] = n++;
//        SVDEBUG << "RegionLayer::recalcSpacing: " << i->first << " -> " << m_spacingMap[i->first] << endl;
    }
}

bool
RegionLayer::getValueExtents(double &min, double &max,
                           bool &logarithmic, QString &unit) const
{
    auto model = ModelById::getAs<RegionModel>(m_model);
    if (!model) return false;
    min = model->getValueMinimum();
    max = model->getValueMaximum();
    unit = getScaleUnits();

    if (m_verticalScale == LogScale) logarithmic = true;

    return true;
}

bool
RegionLayer::getDisplayExtents(double &min, double &max) const
{
    auto model = ModelById::getAs<RegionModel>(m_model);
    if (!model ||
        m_verticalScale == AutoAlignScale ||
        m_verticalScale == EqualSpaced) return false;

    min = model->getValueMinimum();
    max = model->getValueMaximum();

    return true;
}

EventVector
RegionLayer::getLocalPoints(LayerGeometryProvider *v, int x) const
{
    auto model = ModelById::getAs<RegionModel>(m_model);
    if (!model) return EventVector();

    sv_frame_t frame = v->getFrameForX(x);

    EventVector local = model->getEventsCovering(frame);
    if (!local.empty()) return local;

    int fuzz = ViewManager::scalePixelSize(2);
    sv_frame_t start = v->getFrameForX(x - fuzz);
    sv_frame_t end = v->getFrameForX(x + fuzz);

    local = model->getEventsStartingWithin(frame, end - frame);
    if (!local.empty()) return local;

    local = model->getEventsSpanning(start, frame - start);
    if (!local.empty()) return local;

    return {};
}

bool
RegionLayer::getPointToDrag(LayerGeometryProvider *v, int x, int y, Event &point) const
{
    auto model = ModelById::getAs<RegionModel>(m_model);
    if (!model) return false;

    sv_frame_t frame = v->getFrameForX(x);

    EventVector onPoints = model->getEventsCovering(frame);
    if (onPoints.empty()) return false;

    int nearestDistance = -1;
    for (const auto &p: onPoints) {
        int distance = getYForValue(v, p.getValue()) - y;
        if (distance < 0) distance = -distance;
        if (nearestDistance == -1 || distance < nearestDistance) {
            nearestDistance = distance;
            point = p;
        }
    }

    return true;
}

QString
RegionLayer::getLabelPreceding(sv_frame_t frame) const
{
    auto model = ModelById::getAs<RegionModel>(m_model);
    if (!model) return "";
    EventVector points = model->getEventsStartingWithin
        (model->getStartFrame(), frame - model->getStartFrame());
    if (!points.empty()) {
        for (auto i = points.rbegin(); i != points.rend(); ++i) {
            if (i->getLabel() != QString()) {
                return i->getLabel();
            }
        }
    }
    return QString();
}

QString
RegionLayer::getFeatureDescription(LayerGeometryProvider *v, QPoint &pos) const
{
    int x = pos.x();

    auto model = ModelById::getAs<RegionModel>(m_model);
    if (!model || !model->getSampleRate()) return "";

    EventVector points = getLocalPoints(v, x);

    if (points.empty()) {
        if (!model->isReady()) {
            return tr("In progress");
        } else {
            return tr("No local points");
        }
    }

    Event region;
    EventVector::iterator i;

    //!!! harmonise with whatever decision is made about point y
    //!!! coords in paint method

    for (i = points.begin(); i != points.end(); ++i) {

        int y = getYForValue(v, i->getValue());
        int h = 3;

        if (model->getValueQuantization() != 0.0) {
            h = y - getYForValue
                (v, i->getValue() + model->getValueQuantization());
            if (h < 3) h = 3;
        }

        if (pos.y() >= y - h && pos.y() <= y) {
            region = *i;
            break;
        }
    }

    if (i == points.end()) return tr("No local points");

    RealTime rt = RealTime::frame2RealTime(region.getFrame(),
                                           model->getSampleRate());
    RealTime rd = RealTime::frame2RealTime(region.getDuration(),
                                           model->getSampleRate());
    
    QString valueText;

    valueText = tr("%1 %2").arg(region.getValue()).arg(getScaleUnits());

    QString text;

    if (region.getLabel() == "") {
        text = QString(tr("Time:\t%1\nValue:\t%2\nDuration:\t%3\nNo label"))
            .arg(rt.toText(true).c_str())
            .arg(valueText)
            .arg(rd.toText(true).c_str());
    } else {
        text = QString(tr("Time:\t%1\nValue:\t%2\nDuration:\t%3\nLabel:\t%4"))
            .arg(rt.toText(true).c_str())
            .arg(valueText)
            .arg(rd.toText(true).c_str())
            .arg(region.getLabel());
    }

    pos = QPoint(v->getXForFrame(region.getFrame()),
                 getYForValue(v, region.getValue()));
    return text;
}

bool
RegionLayer::snapToFeatureFrame(LayerGeometryProvider *v, sv_frame_t &frame,
                                int &resolution,
                                SnapType snap, int ycoord) const
{
    auto model = ModelById::getAs<RegionModel>(m_model);
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

    // Normally we snap to the start frame of whichever event we
    // find. However here, for SnapRight only, if the end frame of
    // whichever event we would have snapped to had we been snapping
    // left is closer than the start frame of the next event to the
    // right, then we snap to that frame instead. Clear?
    
    Event left;
    bool haveLeft = false;
    if (model->getNearestEventMatching
        (frame, [](Event) { return true; }, EventSeries::Backward, left)) {
        haveLeft = true;
    }

    if (snap == SnapLeft) {
        frame = left.getFrame();
        return haveLeft;
    }

    Event right;
    bool haveRight = false;
    if (model->getNearestEventMatching
        (frame, [](Event) { return true; }, EventSeries::Forward, right)) {
        haveRight = true;
    }

    if (haveLeft) {
        sv_frame_t leftEnd = left.getFrame() + left.getDuration();
        if (leftEnd > frame) {
            if (haveRight) {
                if (leftEnd - frame < right.getFrame() - frame) {
                    frame = leftEnd;
                } else {
                    frame = right.getFrame();
                }
            } else {
                frame = leftEnd;
            }
            return true;
        }
    }

    if (haveRight) {
        frame = right.getFrame();
        return true;
    }

    return false;
}

bool
RegionLayer::snapToSimilarFeature(LayerGeometryProvider *v, sv_frame_t &frame,
                                  int &resolution,
                                  SnapType snap) const
{
    auto model = ModelById::getAs<RegionModel>(m_model);
    if (!model) {
        return Layer::snapToSimilarFeature(v, frame, resolution, snap);
    }

    // snap is only permitted to be SnapLeft or SnapRight here.  We
    // don't do the same trick as in snapToFeatureFrame, of snapping
    // to the end of a feature sometimes.
    
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

QString
RegionLayer::getScaleUnits() const
{
    auto model = ModelById::getAs<RegionModel>(m_model);
    if (model) return model->getScaleUnits();
    else return "";
}

void
RegionLayer::getScaleExtents(LayerGeometryProvider *v, double &min, double &max, bool &log) const
{
    min = 0.0;
    max = 0.0;
    log = false;

    auto model = ModelById::getAs<RegionModel>(m_model);
    if (!model) return;

    QString queryUnits;
    queryUnits = getScaleUnits();

    if (m_verticalScale == AutoAlignScale) {

        if (!v->getVisibleExtentsForUnit(queryUnits, min, max, log)) {

            min = model->getValueMinimum();
            max = model->getValueMaximum();

//            cerr << "RegionLayer[" << this << "]::getScaleExtents: min = " << min << ", max = " << max << ", log = " << log << endl;

        } else if (log) {

            LogRange::mapRange(min, max);

//            cerr << "RegionLayer[" << this << "]::getScaleExtents: min = " << min << ", max = " << max << ", log = " << log << endl;

        }

    } else if (m_verticalScale == EqualSpaced) {

        if (!m_spacingMap.empty()) {
            SpacingMap::const_iterator i = m_spacingMap.begin();
            min = i->second;
            i = m_spacingMap.end();
            --i;
            max = i->second;
//            cerr << "RegionLayer[" << this << "]::getScaleExtents: equal spaced; min = " << min << ", max = " << max << ", log = " << log << endl;
        }

    } else {

        min = model->getValueMinimum();
        max = model->getValueMaximum();

        if (m_verticalScale == LogScale) {
            LogRange::mapRange(min, max);
            log = true;
        }
    }

    if (max == min) max = min + 1.0;
}

int
RegionLayer::spacingIndexToY(LayerGeometryProvider *v, int i) const
{
    int h = v->getPaintHeight();
    int n = int(m_spacingMap.size());
    // this maps from i (spacing of the value from the spacing
    // map) and n (number of region types) to y
    int y = h - (((h * i) / n) + (h / (2 * n)));
    return y;
}

double
RegionLayer::yToSpacingIndex(LayerGeometryProvider *v, int y) const
{
    // we return an inexact result here (double rather than int)
    int h = v->getPaintHeight();
    int n = int(m_spacingMap.size());
    // from y = h - ((h * i) / n) + (h / (2 * n)) as above (vh taking place of i)
    double vh = double(2*h*n - h - 2*n*y) / double(2*h);
    return vh;
}

int
RegionLayer::getYForValue(LayerGeometryProvider *v, double val) const
{
    double min = 0.0, max = 0.0;
    bool logarithmic = false;
    int h = v->getPaintHeight();

    if (m_verticalScale == EqualSpaced) {

        if (m_spacingMap.empty()) return h/2;
        
        SpacingMap::const_iterator i = m_spacingMap.lower_bound(val);
        //!!! what now, if i->first != v?

        int y = spacingIndexToY(v, i->second);

//        SVDEBUG << "RegionLayer::getYForValue: value " << val << " -> i->second " << i->second << " -> y " << y << endl;
        return y;


    } else {

        getScaleExtents(v, min, max, logarithmic);

//    cerr << "RegionLayer[" << this << "]::getYForValue(" << val << "): min = " << min << ", max = " << max << ", log = " << logarithmic << endl;
//    cerr << "h = " << h << ", margin = " << margin << endl;

        if (logarithmic) {
            val = LogRange::map(val);
        }

        return int(h - ((val - min) * h) / (max - min));
    }
}

double
RegionLayer::getValueForY(LayerGeometryProvider *v, int y) const
{
    return getValueForY(v, y, -1);
}

double
RegionLayer::getValueForY(LayerGeometryProvider *v, int y, int avoid) const
{
    double min = 0.0, max = 0.0;
    bool logarithmic = false;
    int h = v->getPaintHeight();

    if (m_verticalScale == EqualSpaced) {

        // if we're equal spaced, we probably want to snap to the
        // nearest item when close to it, and give some notification
        // that we're doing so

        if (m_spacingMap.empty()) return 1.f;

        // n is the number of distinct regions.  if we are close to
        // one of the m/n divisions in the y scale, we should snap to
        // the value of the mth region.

        double vh = yToSpacingIndex(v, y);

        // spacings in the map are integral, so find the closest one,
        // map it back to its y coordinate, and see how far we are
        // from it

        int n = int(m_spacingMap.size());
        int ivh = int(lrint(vh));
        if (ivh < 0) ivh = 0;
        if (ivh > n-1) ivh = n-1;
        int iy = spacingIndexToY(v, ivh);

        int dist = iy - y;
        int gap = h / n; // between region lines

//        cerr << "getValueForY: y = " << y << ", vh = " << vh << ", ivh = " << ivh << " of " << n << ", iy = " << iy << ", dist = " << dist << ", gap = " << gap << endl;

        SpacingMap::const_iterator i = m_spacingMap.begin();
        while (i != m_spacingMap.end()) {
            if (i->second == ivh) break;
            ++i;
        }
        if (i == m_spacingMap.end()) i = m_spacingMap.begin();

//        cerr << "nearest existing value = " << i->first << " at " << iy << endl;

        double val = 0;

//        cerr << "note: avoid = " << avoid << ", i->second = " << i->second << endl;

        if (dist < -gap/3 &&
            ((avoid == -1) ||
             (avoid != i->second && avoid != i->second - 1))) {
            // bisect gap to prior
            if (i == m_spacingMap.begin()) {
                val = i->first - 1.f;
//                cerr << "extended down to " << val << endl;
            } else {
                SpacingMap::const_iterator j = i;
                --j;
                val = (i->first + j->first) / 2;
//                cerr << "bisected down to " << val << endl;
            }
        } else if (dist > gap/3 &&
                   ((avoid == -1) ||
                    (avoid != i->second && avoid != i->second + 1))) {
            // bisect gap to following
            SpacingMap::const_iterator j = i;
            ++j;
            if (j == m_spacingMap.end()) {
                val = i->first + 1.f;
//                cerr << "extended up to " << val << endl;
            } else {
                val = (i->first + j->first) / 2;
//                cerr << "bisected up to " << val << endl;
            }
        } else {
            // snap
            val = i->first;
//            cerr << "snapped to " << val << endl;
        }            

        return val;

    } else {

        getScaleExtents(v, min, max, logarithmic);

        double val = min + (double(h - y) * double(max - min)) / h;

        if (logarithmic) {
            val = pow(10.0, val);
        }

        return val;
    }
}

QColor
RegionLayer::getColourForValue(LayerGeometryProvider *v, double val) const
{
    double min, max;
    bool log;
    getScaleExtents(v, min, max, log);

    if (min > max) std::swap(min, max);
    if (max == min) max = min + 1;

    if (log) {
        LogRange::mapRange(min, max);
        val = LogRange::map(val);
    }

//    SVDEBUG << "RegionLayer::getColourForValue: min " << min << ", max "
//              << max << ", log " << log << ", value " << val << endl;

    QColor solid = ColourMapper(m_colourMap, m_colourInverted, min, max).map(val);
    return QColor(solid.red(), solid.green(), solid.blue(), 120);
}

int
RegionLayer::getDefaultColourHint(bool darkbg, bool &impose)
{
    impose = false;
    return ColourDatabase::getInstance()->getColourIndex
        (QString(darkbg ? "Bright Blue" : "Blue"));
}

void
RegionLayer::paint(LayerGeometryProvider *v, QPainter &paint, QRect rect) const
{
    auto model = ModelById::getAs<RegionModel>(m_model);
    if (!model || !model->isOK()) return;

    sv_samplerate_t sampleRate = model->getSampleRate();
    if (!sampleRate) return;

//    Profiler profiler("RegionLayer::paint", true);

    int x0 = rect.left() - 40;
    int x1 = x0 + rect.width() + 80;

    sv_frame_t wholeFrame0 = v->getFrameForX(0);
    sv_frame_t wholeFrame1 = v->getFrameForX(v->getPaintWidth());

    EventVector points(model->getEventsSpanning(wholeFrame0,
                                                  wholeFrame1 - wholeFrame0));
    if (points.empty()) return;

    paint.setPen(getBaseQColor());

    QColor brushColour(getBaseQColor());
    brushColour.setAlpha(80);

//    SVDEBUG << "RegionLayer::paint: resolution is "
//              << model->getResolution() << " frames" << endl;

    double min = model->getValueMinimum();
    double max = model->getValueMaximum();
    if (max == min) max = min + 1.0;

    QPoint localPos;
    Event illuminatePoint(0);
    bool shouldIlluminate = false;

    if (v->shouldIlluminateLocalFeatures(this, localPos)) {
        shouldIlluminate = getPointToDrag(v, localPos.x(), localPos.y(),
                                          illuminatePoint);
    }

    paint.save();
    paint.setRenderHint(QPainter::Antialiasing, false);
    
    //!!! point y coords if model does not haveDistinctValues() should
    //!!! be assigned to avoid overlaps

    //!!! if it does have distinct values, we should still ensure y
    //!!! coord is never completely flat on the top or bottom

    int fontHeight = paint.fontMetrics().height();

    for (EventVector::const_iterator i = points.begin();
         i != points.end(); ++i) {

        const Event &p(*i);

        int x = v->getXForFrame(p.getFrame());
        int w = v->getXForFrame(p.getFrame() + p.getDuration()) - x;
        int y = getYForValue(v, p.getValue());
        int h = 9;
        int ex = x + w;

        int gap = v->scalePixelSize(2);
        
        EventVector::const_iterator j = i;
        ++j;

        if (j != points.end()) {
            const Event &q(*j);
            int nx = v->getXForFrame(q.getFrame());
            if (nx < ex) ex = nx;
        }

        if (model->getValueQuantization() != 0.0) {
            h = y - getYForValue
                (v, p.getValue() + model->getValueQuantization());
            if (h < 3) h = 3;
        }

        if (w < 1) w = 1;

        if (m_plotStyle == PlotSegmentation) {
            paint.setPen(getForegroundQColor(v->getView()));
            paint.setBrush(getColourForValue(v, p.getValue()));
        } else {
            paint.setPen(getBaseQColor());
            paint.setBrush(brushColour);
        }

        if (m_plotStyle == PlotSegmentation) {

            if (ex <= x) continue;

            if (!shouldIlluminate || illuminatePoint != p) {

                paint.setPen(QPen(getForegroundQColor(v->getView()), 1));
                paint.drawLine(x, 0, x, v->getPaintHeight());
                paint.setPen(Qt::NoPen);

            } else {
                paint.setPen(QPen(getForegroundQColor(v->getView()), 2));
            }

            paint.drawRect(x, -1, ex - x, v->getPaintHeight() + gap);

        } else {

            if (shouldIlluminate && illuminatePoint == p) {

                paint.setPen(v->getForeground());
                paint.setBrush(v->getForeground());

                // Qt 5.13 deprecates QFontMetrics::width(), but its suggested
                // replacement (horizontalAdvance) was only added in Qt 5.11
                // which is too new for us
#pragma GCC diagnostic ignored "-Wdeprecated-declarations"

                QString vlabel =
                    QString("%1%2").arg(p.getValue()).arg(getScaleUnits());
                PaintAssistant::drawVisibleText(v, paint, 
                                   x - paint.fontMetrics().width(vlabel) - gap,
                                   y + paint.fontMetrics().height()/2
                                   - paint.fontMetrics().descent(), 
                                   vlabel, PaintAssistant::OutlinedText);
                
                QString hlabel = RealTime::frame2RealTime
                    (p.getFrame(), model->getSampleRate()).toText(true).c_str();
                PaintAssistant::drawVisibleText(v, paint, 
                                   x,
                                   y - h/2 - paint.fontMetrics().descent() - gap,
                                   hlabel, PaintAssistant::OutlinedText);
            }
            
            paint.drawLine(x, y-1, x + w, y-1);
            paint.drawLine(x, y+1, x + w, y+1);
            paint.drawLine(x, y - h/2, x, y + h/2);
            paint.drawLine(x+w, y - h/2, x + w, y + h/2);
        }
    }

    int nextLabelMinX = -100;
    int lastLabelY = 0;

    for (EventVector::const_iterator i = points.begin();
         i != points.end(); ++i) {

        const Event &p(*i);

        int x = v->getXForFrame(p.getFrame());
        int w = v->getXForFrame(p.getFrame() + p.getDuration()) - x;
        int y = getYForValue(v, p.getValue());

        QString label = p.getLabel();
        if (label == "") {
            label = QString("%1%2").arg(p.getValue()).arg(getScaleUnits());
        }
        int labelWidth = paint.fontMetrics().width(label);

        int gap = v->scalePixelSize(2);

        if (m_plotStyle == PlotSegmentation) {
            if ((x + w < x0 && x + labelWidth + gap < x0) || x > x1) {
                continue;
            }
        } else {
            if (x + w < x0 || x - labelWidth - gap > x1) {
                continue;
            }
        }
        
        bool illuminated = false;

        if (m_plotStyle != PlotSegmentation) {
            if (shouldIlluminate && illuminatePoint == p) {
                illuminated = true;
            }
        }

        if (!illuminated) {

            int labelX, labelY;

            if (m_plotStyle != PlotSegmentation) {
                labelX = x - labelWidth - gap;
                labelY = y + paint.fontMetrics().height()/2 
                    - paint.fontMetrics().descent();
            } else {
                labelX = x + 5;
                labelY = v->getTextLabelYCoord(this, paint);
                if (labelX < nextLabelMinX) {
                    if (lastLabelY < v->getPaintHeight()/2) {
                        labelY = lastLabelY + fontHeight;
                    }
                }
                lastLabelY = labelY;
                nextLabelMinX = labelX + labelWidth;
            }

            PaintAssistant::drawVisibleText(v, paint, labelX, labelY, label,
                                            PaintAssistant::OutlinedText);
        }
    }

    paint.restore();
}

int
RegionLayer::getVerticalScaleWidth(LayerGeometryProvider *v, bool, QPainter &paint) const
{
    auto model = ModelById::getAs<RegionModel>(m_model);
    if (!model || 
        m_verticalScale == AutoAlignScale || 
        m_verticalScale == EqualSpaced) {
        return 0;
    } else if (m_plotStyle == PlotSegmentation) {
        if (m_verticalScale == LogScale) {
            return LogColourScale().getWidth(v, paint);
        } else {
            return LinearColourScale().getWidth(v, paint);
        }
    } else {
        if (m_verticalScale == LogScale) {
            return LogNumericalScale().getWidth(v, paint);
        } else {
            return LinearNumericalScale().getWidth(v, paint);
        }
    }
}

void
RegionLayer::paintVerticalScale(LayerGeometryProvider *v, bool, QPainter &paint, QRect) const
{
    auto model = ModelById::getAs<RegionModel>(m_model);
    if (!model || model->isEmpty()) return;

    QString unit;
    double min, max;
    bool logarithmic;

    int w = getVerticalScaleWidth(v, false, paint);

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
RegionLayer::drawStart(LayerGeometryProvider *v, QMouseEvent *e)
{
    auto model = ModelById::getAs<RegionModel>(m_model);
    if (!model) return;

    sv_frame_t frame = v->getFrameForX(e->x());
    if (frame < 0) frame = 0;
    frame = frame / model->getResolution() * model->getResolution();

    double value = getValueForY(v, e->y());

    m_editingPoint = Event(frame, float(value), 0, "");
    m_originalPoint = m_editingPoint;

    if (m_editingCommand) finish(m_editingCommand);
    m_editingCommand = new ChangeEventsCommand(m_model.untyped, tr("Draw Region"));
    m_editingCommand->add(m_editingPoint);

    recalcSpacing();

    m_editing = true;
}

void
RegionLayer::drawDrag(LayerGeometryProvider *v, QMouseEvent *e)
{
    auto model = ModelById::getAs<RegionModel>(m_model);
    if (!model || !m_editing) return;

    sv_frame_t frame = v->getFrameForX(e->x());
    if (frame < 0) frame = 0;
    frame = frame / model->getResolution() * model->getResolution();

    double newValue = m_editingPoint.getValue();
    if (m_verticalScale != EqualSpaced) newValue = getValueForY(v, e->y());

    sv_frame_t newFrame = m_editingPoint.getFrame();
    sv_frame_t newDuration = frame - newFrame;
    if (newDuration < 0) {
        newFrame = frame;
        newDuration = -newDuration;
    } else if (newDuration == 0) {
        newDuration = 1;
    }

    m_editingCommand->remove(m_editingPoint);
    m_editingPoint = m_editingPoint
        .withFrame(newFrame)
        .withValue(float(newValue))
        .withDuration(newDuration);
    m_editingCommand->add(m_editingPoint);

    recalcSpacing();
}

void
RegionLayer::drawEnd(LayerGeometryProvider *, QMouseEvent *)
{
    auto model = ModelById::getAs<RegionModel>(m_model);
    if (!model || !m_editing) return;
    finish(m_editingCommand);
    m_editingCommand = nullptr;
    m_editing = false;

    recalcSpacing();
}

void
RegionLayer::eraseStart(LayerGeometryProvider *v, QMouseEvent *e)
{
    auto model = ModelById::getAs<RegionModel>(m_model);
    if (!model) return;

    if (!getPointToDrag(v, e->x(), e->y(), m_editingPoint)) return;

    if (m_editingCommand) {
        finish(m_editingCommand);
        m_editingCommand = nullptr;
    }

    m_editing = true;
    recalcSpacing();
}

void
RegionLayer::eraseDrag(LayerGeometryProvider *, QMouseEvent *)
{
}

void
RegionLayer::eraseEnd(LayerGeometryProvider *v, QMouseEvent *e)
{
    auto model = ModelById::getAs<RegionModel>(m_model);
    if (!model || !m_editing) return;

    m_editing = false;

    Event p(0);
    if (!getPointToDrag(v, e->x(), e->y(), p)) return;
    if (p.getFrame() != m_editingPoint.getFrame() ||
        p.getValue() != m_editingPoint.getValue()) return;

    m_editingCommand = new ChangeEventsCommand
        (m_model.untyped, tr("Erase Region"));

    m_editingCommand->remove(m_editingPoint);

    finish(m_editingCommand);
    m_editingCommand = nullptr;
    m_editing = false;
    recalcSpacing();
}

void
RegionLayer::editStart(LayerGeometryProvider *v, QMouseEvent *e)
{
    auto model = ModelById::getAs<RegionModel>(m_model);
    if (!model) return;

    if (!getPointToDrag(v, e->x(), e->y(), m_editingPoint)) {
        return;
    }

    m_dragPointX = v->getXForFrame(m_editingPoint.getFrame());
    m_dragPointY = getYForValue(v, m_editingPoint.getValue());

    m_originalPoint = m_editingPoint;

    if (m_editingCommand) {
        finish(m_editingCommand);
        m_editingCommand = nullptr;
    }

    m_editing = true;
    m_dragStartX = e->x();
    m_dragStartY = e->y();
    recalcSpacing();
}

void
RegionLayer::editDrag(LayerGeometryProvider *v, QMouseEvent *e)
{
    auto model = ModelById::getAs<RegionModel>(m_model);
    if (!model || !m_editing) return;

    int xdist = e->x() - m_dragStartX;
    int ydist = e->y() - m_dragStartY;
    int newx = m_dragPointX + xdist;
    int newy = m_dragPointY + ydist;

    sv_frame_t frame = v->getFrameForX(newx);
    if (frame < 0) frame = 0;
    frame = frame / model->getResolution() * model->getResolution();

    // Do not bisect between two values, if one of those values is
    // that of the point we're actually moving ...
    int avoid = m_spacingMap[m_editingPoint.getValue()];

    // ... unless there are other points with the same value
    if (m_distributionMap[m_editingPoint.getValue()] > 1) avoid = -1;

    double value = getValueForY(v, newy, avoid);

    if (!m_editingCommand) {
        m_editingCommand = new ChangeEventsCommand(m_model.untyped,
                                                      tr("Drag Region"));
    }

    m_editingCommand->remove(m_editingPoint);
    m_editingPoint = m_editingPoint
        .withFrame(frame)
        .withValue(float(value));
    m_editingCommand->add(m_editingPoint);
    recalcSpacing();
}

void
RegionLayer::editEnd(LayerGeometryProvider *, QMouseEvent *)
{
    auto model = ModelById::getAs<RegionModel>(m_model);
    if (!model || !m_editing) return;

    if (m_editingCommand) {

        QString newName = m_editingCommand->getName();

        if (m_editingPoint.getFrame() != m_originalPoint.getFrame()) {
            if (m_editingPoint.getValue() != m_originalPoint.getValue()) {
                newName = tr("Edit Region");
            } else {
                newName = tr("Relocate Region");
            }
        } else {
            newName = tr("Change Point Value");
        }

        m_editingCommand->setName(newName);
        finish(m_editingCommand);
    }

    m_editingCommand = nullptr;
    m_editing = false;
    recalcSpacing();
}

bool
RegionLayer::editOpen(LayerGeometryProvider *v, QMouseEvent *e)
{
    auto model = ModelById::getAs<RegionModel>(m_model);
    if (!model) return false;

    Event region(0);
    if (!getPointToDrag(v, e->x(), e->y(), region)) return false;

    ItemEditDialog *dialog = new ItemEditDialog
        (model->getSampleRate(),
         ItemEditDialog::ShowTime |
         ItemEditDialog::ShowDuration |
         ItemEditDialog::ShowValue |
         ItemEditDialog::ShowText,
         getScaleUnits());

    dialog->setFrameTime(region.getFrame());
    dialog->setValue(region.getValue());
    dialog->setFrameDuration(region.getDuration());
    dialog->setText(region.getLabel());

    if (dialog->exec() == QDialog::Accepted) {

        Event newRegion = region
            .withFrame(dialog->getFrameTime())
            .withValue(dialog->getValue())
            .withDuration(dialog->getFrameDuration())
            .withLabel(dialog->getText());
        
        ChangeEventsCommand *command = new ChangeEventsCommand
            (m_model.untyped, tr("Edit Region"));
        command->remove(region);
        command->add(newRegion);
        finish(command);
    }

    delete dialog;
    recalcSpacing();
    return true;
}

void
RegionLayer::moveSelection(Selection s, sv_frame_t newStartFrame)
{
    auto model = ModelById::getAs<RegionModel>(m_model);
    if (!model) return;

    ChangeEventsCommand *command =
        new ChangeEventsCommand(m_model.untyped, tr("Drag Selection"));

    EventVector points =
        model->getEventsStartingWithin(s.getStartFrame(), s.getDuration());

    for (EventVector::iterator i = points.begin();
         i != points.end(); ++i) {

        Event newPoint = (*i)
            .withFrame(i->getFrame() + newStartFrame - s.getStartFrame());
        command->remove(*i);
        command->add(newPoint);
    }

    finish(command);
    recalcSpacing();
}

void
RegionLayer::resizeSelection(Selection s, Selection newSize)
{
    auto model = ModelById::getAs<RegionModel>(m_model);
    if (!model || !s.getDuration()) return;

    ChangeEventsCommand *command =
        new ChangeEventsCommand(m_model.untyped, tr("Resize Selection"));

    EventVector points =
        model->getEventsStartingWithin(s.getStartFrame(), s.getDuration());

    double ratio = double(newSize.getDuration()) / double(s.getDuration());
    double oldStart = double(s.getStartFrame());
    double newStart = double(newSize.getStartFrame());
    
    for (Event p: points) {

        double newFrame = (double(p.getFrame()) - oldStart) * ratio + newStart;
        double newDuration = double(p.getDuration()) * ratio;

        Event newPoint = p
            .withFrame(lrint(newFrame))
            .withDuration(lrint(newDuration));
        command->remove(p);
        command->add(newPoint);
    }

    finish(command);
    recalcSpacing();
}

void
RegionLayer::deleteSelection(Selection s)
{
    auto model = ModelById::getAs<RegionModel>(m_model);
    if (!model) return;

    ChangeEventsCommand *command =
        new ChangeEventsCommand(m_model.untyped, tr("Delete Selected Points"));

    EventVector points =
        model->getEventsStartingWithin(s.getStartFrame(), s.getDuration());

    for (EventVector::iterator i = points.begin();
         i != points.end(); ++i) {

        if (s.contains(i->getFrame())) {
            command->remove(*i);
        }
    }

    finish(command);
    recalcSpacing();
}    

void
RegionLayer::copy(LayerGeometryProvider *v, Selection s, Clipboard &to)
{
    auto model = ModelById::getAs<RegionModel>(m_model);
    if (!model) return;

    EventVector points =
        model->getEventsStartingWithin(s.getStartFrame(), s.getDuration());

    for (Event p: points) {
        to.addPoint(p.withReferenceFrame(alignToReference(v, p.getFrame())));
    }
}

bool
RegionLayer::paste(LayerGeometryProvider *v, const Clipboard &from,
                   sv_frame_t /* frameOffset */, bool /* interactive */)
{
    auto model = ModelById::getAs<RegionModel>(m_model);
    if (!model) return false;

    const EventVector &points = from.getPoints();

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

        Event p = i->withFrame(frame);

        Event newPoint = p;
        if (!p.hasValue()) {
            newPoint = newPoint.withValue((model->getValueMinimum() +
                                           model->getValueMaximum()) / 2);
        }
        if (!p.hasDuration()) {
            sv_frame_t nextFrame = frame;
            EventVector::const_iterator j = i;
            for (; j != points.end(); ++j) {
                if (j != i) break;
            }
            if (j != points.end()) {
                nextFrame = j->getFrame();
            }
            if (nextFrame == frame) {
                newPoint = newPoint.withDuration(model->getResolution());
            } else {
                newPoint = newPoint.withDuration(nextFrame - frame);
            }
        }
        
        command->add(newPoint);
    }

    finish(command);
    recalcSpacing();
    return true;
}

void
RegionLayer::toXml(QTextStream &stream,
                 QString indent, QString extraAttributes) const
{
    QString s;

    s += QString("verticalScale=\"%1\" "
                 "plotStyle=\"%2\" ")
        .arg(m_verticalScale)
        .arg(m_plotStyle);

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
RegionLayer::setProperties(const QXmlAttributes &attributes)
{
    SingleColourLayer::setProperties(attributes);

    bool ok;
    VerticalScale scale = (VerticalScale)
        attributes.value("verticalScale").toInt(&ok);
    if (ok) setVerticalScale(scale);
    PlotStyle style = (PlotStyle)
        attributes.value("plotStyle").toInt(&ok);
    if (ok) setPlotStyle(style);
    
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
}


