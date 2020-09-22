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

#include "NoteLayer.h"

#include "data/model/Model.h"
#include "base/RealTime.h"
#include "base/Profiler.h"
#include "base/Pitch.h"
#include "base/LogRange.h"
#include "base/RangeMapper.h"
#include "view/View.h"

#include "ColourDatabase.h"
#include "PianoScale.h"
#include "LinearNumericalScale.h"
#include "LogNumericalScale.h"
#include "PaintAssistant.h"

#include "data/model/NoteModel.h"

#include "widgets/ItemEditDialog.h"
#include "widgets/TextAbbrev.h"

#include <QPainter>
#include <QPainterPath>
#include <QMouseEvent>
#include <QTextStream>
#include <QMessageBox>

#include <iostream>
#include <cmath>
#include <utility>

//#define DEBUG_NOTE_LAYER 1

NoteLayer::NoteLayer() :
    SingleColourLayer(),
    m_modelUsesHz(true),
    m_editing(false),
    m_dragPointX(0),
    m_dragPointY(0),
    m_dragStartX(0),
    m_dragStartY(0),
    m_originalPoint(0, 0.0, 0, 1.f, tr("New Point")),
    m_editingPoint(0, 0.0, 0, 1.f, tr("New Point")),
    m_editingCommand(nullptr),
    m_editIsOpen(false),
    m_verticalScale(AutoAlignScale),
    m_scaleMinimum(0),
    m_scaleMaximum(0)
{
    SVDEBUG << "constructed NoteLayer" << endl;
}

int
NoteLayer::getCompletion(LayerGeometryProvider *) const
{
    auto model = ModelById::get(m_model);
    if (model) return model->getCompletion();
    else return 0;
}

void
NoteLayer::setModel(ModelId modelId)
{
    auto newModel = ModelById::getAs<NoteModel>(modelId);
    
    if (!modelId.isNone() && !newModel) {
        throw std::logic_error("Not a NoteModel");
    }
    
    if (m_model == modelId) return;
    m_model = modelId;

    if (newModel) {
        connectSignals(m_model);

        QString unit = newModel->getScaleUnits();
        m_modelUsesHz = (unit.toLower() == "hz");
    }
    
    m_scaleMinimum = 0;
    m_scaleMaximum = 0;

    emit modelReplaced();
}

Layer::PropertyList
NoteLayer::getProperties() const
{
    PropertyList list = SingleColourLayer::getProperties();
    list.push_back("Vertical Scale");
    list.push_back("Scale Units");
    return list;
}

QString
NoteLayer::getPropertyLabel(const PropertyName &name) const
{
    if (name == "Vertical Scale") return tr("Vertical Scale");
    if (name == "Scale Units") return tr("Scale Units");
    return SingleColourLayer::getPropertyLabel(name);
}

Layer::PropertyType
NoteLayer::getPropertyType(const PropertyName &name) const
{
    if (name == "Scale Units") return UnitsProperty;
    if (name == "Vertical Scale") return ValueProperty;
    return SingleColourLayer::getPropertyType(name);
}

QString
NoteLayer::getPropertyGroupName(const PropertyName &name) const
{
    if (name == "Vertical Scale" || name == "Scale Units") {
        return tr("Scale");
    }
    return SingleColourLayer::getPropertyGroupName(name);
}

QString
NoteLayer::getScaleUnits() const
{
    return "Hz";
}

int
NoteLayer::getPropertyRangeAndValue(const PropertyName &name,
                                    int *min, int *max, int *deflt) const
{
    int val = 0;

    if (name == "Vertical Scale") {
        
        if (min) *min = 0;
        if (max) *max = 3;
        if (deflt) *deflt = int(AutoAlignScale);
        
        val = int(m_verticalScale);

    } else if (name == "Scale Units") {

        if (deflt) *deflt = 0;
        auto model = ModelById::getAs<NoteModel>(m_model);
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
NoteLayer::getPropertyValueLabel(const PropertyName &name,
                                 int value) const
{
    if (name == "Vertical Scale") {
        switch (value) {
        default:
        case 0: return tr("Auto-Align");
        case 1: return tr("Linear");
        case 2: return tr("Log");
        case 3: return tr("MIDI Notes");
        }
    }
    return SingleColourLayer::getPropertyValueLabel(name, value);
}

void
NoteLayer::setProperty(const PropertyName &name, int value)
{
    if (name == "Vertical Scale") {
        setVerticalScale(VerticalScale(value));
    } else if (name == "Scale Units") {
        auto model = ModelById::getAs<NoteModel>(m_model);
        if (model) {
            QString unit = UnitDatabase::getInstance()->getUnitById(value);
            model->setScaleUnits(unit);
            m_modelUsesHz = (unit.toLower() == "hz");
            emit modelChanged(m_model);
        }
    } else {
        return SingleColourLayer::setProperty(name, value);
    }
}

void
NoteLayer::setVerticalScale(VerticalScale scale)
{
    if (m_verticalScale == scale) return;
    m_verticalScale = scale;
    emit layerParametersChanged();
}

bool
NoteLayer::isLayerScrollable(const LayerGeometryProvider *v) const
{
    QPoint discard;
    return !v->shouldIlluminateLocalFeatures(this, discard);
}

double
NoteLayer::valueOf(const Event &e) const
{
    return convertValueFromEventValue(e.getValue());
}

Event
NoteLayer::eventWithValue(const Event &e, double value) const
{
    return e.withValue(convertValueToEventValue(value));
}

double
NoteLayer::convertValueFromEventValue(float eventValue) const
{
    if (m_modelUsesHz) {
        return eventValue;
    } else {
        double v = eventValue;
        if (v < 0) v = 0;
        if (v > 127) v = 127;
        int p = int(round(v));
        double c = 100.0 * (v - p);
        return Pitch::getFrequencyForPitch(p, c);
    }
}

float
NoteLayer::convertValueToEventValue(double value) const
{
    if (m_modelUsesHz) {
        return float(value);
    } else {
        float c = 0;
        int p = Pitch::getPitchForFrequency(value, &c);
        return float(p) + c / 100.f;
    }
}

bool
NoteLayer::getValueExtents(double &min, double &max,
                           bool &logarithmic, QString &unit) const
{
    auto model = ModelById::getAs<NoteModel>(m_model);
    if (!model) return false;

    min = convertValueFromEventValue(model->getValueMinimum());
    max = convertValueFromEventValue(model->getValueMaximum());
    min /= 1.06;
    max *= 1.06;
    unit = "Hz";

    if (m_verticalScale != LinearScale) {
        logarithmic = true;
    }

    return true;
}

bool
NoteLayer::getDisplayExtents(double &min, double &max) const
{
    auto model = ModelById::getAs<NoteModel>(m_model);
    if (!model || shouldAutoAlign()) return false;

    if (m_verticalScale == MIDIRangeScale) {
        min = Pitch::getFrequencyForPitch(0);
        max = Pitch::getFrequencyForPitch(127);
        return true;
    }

    if (m_scaleMinimum == m_scaleMaximum) {
        QString unit;
        bool log = false;
        getValueExtents(min, max, log, unit);
    } else {
        min = m_scaleMinimum;
        max = m_scaleMaximum;
    }

#ifdef DEBUG_NOTE_LAYER
    SVCERR << "NoteLayer::getDisplayExtents: min = " << min << ", max = " << max << " (m_scaleMinimum = " << m_scaleMinimum << ", m_scaleMaximum = " << m_scaleMaximum << ")" << endl;
#endif

    return true;
}

bool
NoteLayer::setDisplayExtents(double min, double max)
{
    if (m_model.isNone()) return false;

    if (min == max) {
        if (min == 0.f) {
            max = 1.f;
        } else {
            max = min * 1.0001;
        }
    }

    m_scaleMinimum = min;
    m_scaleMaximum = max;

#ifdef DEBUG_NOTE_LAYER
    SVCERR << "NoteLayer::setDisplayExtents: min = " << min << ", max = " << max << endl;
#endif
    
    emit layerParametersChanged();
    return true;
}

int
NoteLayer::getVerticalZoomSteps(int &defaultStep) const
{
    if (shouldAutoAlign() || m_model.isNone()) return 0;
    defaultStep = 0;
    return 100;
}

int
NoteLayer::getCurrentVerticalZoomStep() const
{
    if (shouldAutoAlign() || m_model.isNone()) return 0;

    RangeMapper *mapper = getNewVerticalZoomRangeMapper();
    if (!mapper) return 0;

    double dmin, dmax;
    getDisplayExtents(dmin, dmax);

    int nr = mapper->getPositionForValue(dmax - dmin);

    delete mapper;

    return 100 - nr;
}

//!!! lots of duplication with TimeValueLayer

void
NoteLayer::setVerticalZoomStep(int step)
{
    if (shouldAutoAlign() || m_model.isNone()) return;

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

//        cerr << "newmin = " << newmin << ", newmax = " << newmax << endl;

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
    
#ifdef DEBUG_NOTE_LAYER
    SVCERR << "NoteLayer::setVerticalZoomStep: " << step << ": " << newmin << " -> " << newmax << " (range " << newdist << ")" << endl;
#endif

    setDisplayExtents(newmin, newmax);
}

RangeMapper *
NoteLayer::getNewVerticalZoomRangeMapper() const
{
    if (m_model.isNone()) return nullptr;
    
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
NoteLayer::getLocalPoints(LayerGeometryProvider *v, int x) const
{
    auto model = ModelById::getAs<NoteModel>(m_model);
    if (!model) return {};
    
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
NoteLayer::getPointToDrag(LayerGeometryProvider *v, int x, int y, Event &point) const
{
    auto model = ModelById::getAs<NoteModel>(m_model);
    if (!model) return false;

    sv_frame_t frame = v->getFrameForX(x);

    EventVector onPoints = model->getEventsCovering(frame);
    if (onPoints.empty()) return false;

    int nearestDistance = -1;
    for (const auto &p: onPoints) {
        int distance = getYForValue(v, valueOf(p)) - y;
        if (distance < 0) distance = -distance;
        if (nearestDistance == -1 || distance < nearestDistance) {
            nearestDistance = distance;
            point = p;
        }
    }

    return true;
}

QString
NoteLayer::getFeatureDescription(LayerGeometryProvider *v, QPoint &pos) const
{
    int x = pos.x();

    auto model = ModelById::getAs<NoteModel>(m_model);
    if (!model || !model->getSampleRate()) return "";

    EventVector points = getLocalPoints(v, x);

    if (points.empty()) {
        if (!model->isReady()) {
            return tr("In progress");
        } else {
            return tr("No local points");
        }
    }

    Event note;
    EventVector::iterator i;

    for (i = points.begin(); i != points.end(); ++i) {

        int y = getYForValue(v, valueOf(*i));
        int h = 3;

        if (model->getValueQuantization() != 0.0) {
            h = y - getYForValue
                (v, convertValueFromEventValue(i->getValue() +
                                               model->getValueQuantization()));
            if (h < 3) h = 3;
        }

        if (pos.y() >= y - h && pos.y() <= y) {
            note = *i;
            break;
        }
    }

    if (i == points.end()) return tr("No local points");

    RealTime rt = RealTime::frame2RealTime(note.getFrame(),
                                           model->getSampleRate());
    RealTime rd = RealTime::frame2RealTime(note.getDuration(),
                                           model->getSampleRate());
    
    QString pitchText;

    if (m_modelUsesHz) {

        float value = note.getValue();
    
        pitchText = tr("%1 Hz (%2, %3)")
            .arg(value)
            .arg(Pitch::getPitchLabelForFrequency(value))
            .arg(Pitch::getPitchForFrequency(value));

    } else {
        
        float eventValue = note.getValue();
        double value = convertValueFromEventValue(eventValue);
        
        int mnote = int(lrint(eventValue));
        int cents = int(lrint((eventValue - float(mnote)) * 100));

        pitchText = tr("%1 (%2, %3 Hz)")
            .arg(Pitch::getPitchLabel(mnote, cents))
            .arg(eventValue)
            .arg(value);
    }

    QString text;

    if (note.getLabel() == "") {
        text = QString(tr("Time:\t%1\nPitch:\t%2\nDuration:\t%3\nNo label"))
            .arg(rt.toText(true).c_str())
            .arg(pitchText)
            .arg(rd.toText(true).c_str());
    } else {
        text = QString(tr("Time:\t%1\nPitch:\t%2\nDuration:\t%3\nLabel:\t%4"))
            .arg(rt.toText(true).c_str())
            .arg(pitchText)
            .arg(rd.toText(true).c_str())
            .arg(note.getLabel());
    }

    pos = QPoint(v->getXForFrame(note.getFrame()),
                 getYForValue(v, valueOf(note)));
    return text;
}

bool
NoteLayer::snapToFeatureFrame(LayerGeometryProvider *v, sv_frame_t &frame,
                              int &resolution,
                              SnapType snap, int ycoord) const
{
    auto model = ModelById::getAs<NoteModel>(m_model);
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

void
NoteLayer::getScaleExtents(LayerGeometryProvider *v, double &min, double &max, bool &log) const
{
    min = 0.0;
    max = 0.0;
    log = false;

    auto model = ModelById::getAs<NoteModel>(m_model);
    if (!model) return;
    
    if (shouldAutoAlign()) {

        if (!v->getVisibleExtentsForUnit("Hz", min, max, log)) {

            QString unit;
            getValueExtents(min, max, log, unit);

#ifdef DEBUG_NOTE_LAYER
            SVCERR << "NoteLayer[" << this << "]::getScaleExtents: min = " << min << ", max = " << max << ", log = " << log << endl;
#endif

        } else if (log) {

            LogRange::mapRange(min, max);

#ifdef DEBUG_NOTE_LAYER
            SVCERR << "NoteLayer[" << this << "]::getScaleExtents: min = " << min << ", max = " << max << ", log = " << log << endl;
#endif
        }

    } else {

        getDisplayExtents(min, max);

        if (m_verticalScale != LinearScale) {
            LogRange::mapRange(min, max);
            log = true;
        }
    }

    if (max == min) max = min + 1.0;
}

int
NoteLayer::getYForValue(LayerGeometryProvider *v, double val) const
{
    double min = 0.0, max = 0.0;
    bool logarithmic = false;
    int h = v->getPaintHeight();

    getScaleExtents(v, min, max, logarithmic);

#ifdef DEBUG_NOTE_LAYER
    SVCERR << "NoteLayer[" << this << "]::getYForValue(" << val << "): min = " << min << ", max = " << max << ", log = " << logarithmic << endl;
#endif

    if (logarithmic) {
        val = LogRange::map(val);
#ifdef DEBUG_NOTE_LAYER
        SVCERR << "logarithmic true, val now = " << val << endl;
#endif
    }

    int y = int(h - ((val - min) * h) / (max - min)) - 1;
#ifdef DEBUG_NOTE_LAYER
    SVCERR << "y = " << y << endl;
#endif
    return y;
}

double
NoteLayer::getValueForY(LayerGeometryProvider *v, int y) const
{
    double min = 0.0, max = 0.0;
    bool logarithmic = false;
    int h = v->getPaintHeight();

    getScaleExtents(v, min, max, logarithmic);

    double val = min + (double(h - y) * double(max - min)) / h;

    if (logarithmic) {
        val = pow(10.0, val);
    }

    return val;
}

bool
NoteLayer::shouldAutoAlign() const
{
    if (m_model.isNone()) return false;
    return (m_verticalScale == AutoAlignScale);
}

void
NoteLayer::paint(LayerGeometryProvider *v, QPainter &paint, QRect rect) const
{
    auto model = ModelById::getAs<NoteModel>(m_model);
    if (!model || !model->isOK()) return;

    sv_samplerate_t sampleRate = model->getSampleRate();
    if (!sampleRate) return;

//    Profiler profiler("NoteLayer::paint", true);

    int x0 = rect.left();
    int x1 = x0 + rect.width();
    
    sv_frame_t frame0 = v->getFrameForX(x0);
    sv_frame_t frame1 = v->getFrameForX(x1);

    EventVector points(model->getEventsSpanning(frame0, frame1 - frame0));
    if (points.empty()) return;

    paint.setPen(getBaseQColor());

    QColor brushColour(getBaseQColor());
    brushColour.setAlpha(80);

//    SVDEBUG << "NoteLayer::paint: resolution is "
//              << model->getResolution() << " frames" << endl;

    double min = convertValueFromEventValue(model->getValueMinimum());
    double max = convertValueFromEventValue(model->getValueMaximum());
    if (max == min) max = min + 1.0;

    QPoint localPos;
    Event illuminatePoint;
    bool shouldIlluminate = false;

    if (m_editing || m_editIsOpen) {
        shouldIlluminate = true;
        illuminatePoint = m_editingPoint;
    } else if (v->shouldIlluminateLocalFeatures(this, localPos)) {
        shouldIlluminate = getPointToDrag(v, localPos.x(), localPos.y(),
                                          illuminatePoint);
    }

    paint.save();
    paint.setRenderHint(QPainter::Antialiasing, false);
    
    for (EventVector::const_iterator i = points.begin();
         i != points.end(); ++i) {

        const Event &p(*i);

        int x = v->getXForFrame(p.getFrame());
        int y = getYForValue(v, valueOf(p));
        int w = v->getXForFrame(p.getFrame() + p.getDuration()) - x;
        int h = 3;
        
        if (model->getValueQuantization() != 0.0) {
            h = y - getYForValue
                (v, convertValueFromEventValue
                 (p.getValue() + model->getValueQuantization()));
            if (h < 3) h = 3;
        }

        if (w < 1) w = 1;
        paint.setPen(getBaseQColor());
        paint.setBrush(brushColour);

        if (shouldIlluminate && illuminatePoint == p) {

            paint.setPen(v->getForeground());
            paint.setBrush(v->getForeground());

    // Qt 5.13 deprecates QFontMetrics::width(), but its suggested
    // replacement (horizontalAdvance) was only added in Qt 5.11
    // which is too new for us
#pragma GCC diagnostic ignored "-Wdeprecated-declarations"

            QString vlabel;
            if (m_modelUsesHz) {
                vlabel = QString("%1%2")
                    .arg(p.getValue())
                    .arg(model->getScaleUnits());
            } else {
                vlabel = QString("%1 %2")
                    .arg(p.getValue())
                    .arg(model->getScaleUnits());
            }
            
            PaintAssistant::drawVisibleText(v, paint, 
                               x - paint.fontMetrics().width(vlabel) - 2,
                               y + paint.fontMetrics().height()/2
                                 - paint.fontMetrics().descent(), 
                               vlabel, PaintAssistant::OutlinedText);

            QString hlabel = RealTime::frame2RealTime
                (p.getFrame(), model->getSampleRate()).toText(true).c_str();
            PaintAssistant::drawVisibleText(v, paint, 
                               x,
                               y - h/2 - paint.fontMetrics().descent() - 2,
                               hlabel, PaintAssistant::OutlinedText);
        }
        
        paint.drawRect(x, y - h/2, w, h);
    }

    paint.restore();
}

int
NoteLayer::getVerticalScaleWidth(LayerGeometryProvider *v, bool, QPainter &paint) const
{
    if (m_model.isNone()) {
        return 0;
    }

    if (shouldAutoAlign() && !valueExtentsMatchMine(v)) {
        return 0;
    }

    if (m_verticalScale != LinearScale) {
        return LogNumericalScale().getWidth(v, paint) + 10; // for piano
    } else {
        return LinearNumericalScale().getWidth(v, paint);
    }
}

void
NoteLayer::paintVerticalScale(LayerGeometryProvider *v, bool, QPainter &paint, QRect) const
{
    auto model = ModelById::getAs<NoteModel>(m_model);
    if (!model || model->isEmpty()) return;

    QString unit;
    double min, max;
    bool logarithmic;

    int w = getVerticalScaleWidth(v, false, paint);
    int h = v->getPaintHeight();

    getScaleExtents(v, min, max, logarithmic);

    if (logarithmic) {
        LogNumericalScale().paintVertical(v, this, paint, 0, min, max);
    } else {
        LinearNumericalScale().paintVertical(v, this, paint, 0, min, max);
    }
    
    if (logarithmic) {
        PianoScale().paintPianoVertical
            (v, paint, QRect(w - 10, 0, 10, h), 
             LogRange::unmap(min), 
             LogRange::unmap(max));
        paint.drawLine(w, 0, w, h);
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
NoteLayer::drawStart(LayerGeometryProvider *v, QMouseEvent *e)
{
//    SVDEBUG << "NoteLayer::drawStart(" << e->x() << "," << e->y() << ")" << endl;

    auto model = ModelById::getAs<NoteModel>(m_model);
    if (!model) return;

    sv_frame_t frame = v->getFrameForX(e->x());
    if (frame < 0) frame = 0;
    frame = frame / model->getResolution() * model->getResolution();

    double value = getValueForY(v, e->y());
    float eventValue = convertValueToEventValue(value);
    eventValue = roundf(eventValue);

    m_editingPoint = Event(frame, eventValue, 0, 0.8f, tr("New Point"));
    m_originalPoint = m_editingPoint;

    if (m_editingCommand) finish(m_editingCommand);
    m_editingCommand = new ChangeEventsCommand(m_model.untyped, tr("Draw Point"));
    m_editingCommand->add(m_editingPoint);

    m_editing = true;
}

void
NoteLayer::drawDrag(LayerGeometryProvider *v, QMouseEvent *e)
{
//    SVDEBUG << "NoteLayer::drawDrag(" << e->x() << "," << e->y() << ")" << endl;

    auto model = ModelById::getAs<NoteModel>(m_model);
    if (!model || !m_editing) return;

    sv_frame_t frame = v->getFrameForX(e->x());
    if (frame < 0) frame = 0;
    frame = frame / model->getResolution() * model->getResolution();

    double newValue = getValueForY(v, e->y());
    float newEventValue = convertValueToEventValue(newValue);
    newEventValue = roundf(newEventValue);

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
        .withDuration(newDuration)
        .withValue(newEventValue);
    m_editingCommand->add(m_editingPoint);
}

void
NoteLayer::drawEnd(LayerGeometryProvider *, QMouseEvent *)
{
//    SVDEBUG << "NoteLayer::drawEnd(" << e->x() << "," << e->y() << ")" << endl;
    auto model = ModelById::getAs<NoteModel>(m_model);
    if (!model || !m_editing) return;
    finish(m_editingCommand);
    m_editingCommand = nullptr;
    m_editing = false;
}

void
NoteLayer::eraseStart(LayerGeometryProvider *v, QMouseEvent *e)
{
    auto model = ModelById::getAs<NoteModel>(m_model);
    if (!model) return;

    if (!getPointToDrag(v, e->x(), e->y(), m_editingPoint)) return;

    if (m_editingCommand) {
        finish(m_editingCommand);
        m_editingCommand = nullptr;
    }

    m_editing = true;
}

void
NoteLayer::eraseDrag(LayerGeometryProvider *, QMouseEvent *)
{
}

void
NoteLayer::eraseEnd(LayerGeometryProvider *v, QMouseEvent *e)
{
    auto model = ModelById::getAs<NoteModel>(m_model);
    if (!model || !m_editing) return;

    m_editing = false;

    Event p(0);
    if (!getPointToDrag(v, e->x(), e->y(), p)) return;
    if (p.getFrame() != m_editingPoint.getFrame() ||
        p.getValue() != m_editingPoint.getValue()) return;

    m_editingCommand = new ChangeEventsCommand(m_model.untyped, tr("Erase Point"));

    m_editingCommand->remove(m_editingPoint);

    finish(m_editingCommand);
    m_editingCommand = nullptr;
    m_editing = false;
}

void
NoteLayer::editStart(LayerGeometryProvider *v, QMouseEvent *e)
{
//    SVDEBUG << "NoteLayer::editStart(" << e->x() << "," << e->y() << ")" << endl;

    auto model = ModelById::getAs<NoteModel>(m_model);
    if (!model) return;

    if (!getPointToDrag(v, e->x(), e->y(), m_editingPoint)) return;
    m_originalPoint = m_editingPoint;

    m_dragPointX = v->getXForFrame(m_editingPoint.getFrame());
    m_dragPointY = getYForValue(v, valueOf(m_editingPoint));

    if (m_editingCommand) {
        finish(m_editingCommand);
        m_editingCommand = nullptr;
    }

    m_editing = true;
    m_dragStartX = e->x();
    m_dragStartY = e->y();
}

void
NoteLayer::editDrag(LayerGeometryProvider *v, QMouseEvent *e)
{
//    SVDEBUG << "NoteLayer::editDrag(" << e->x() << "," << e->y() << ")" << endl;

    auto model = ModelById::getAs<NoteModel>(m_model);
    if (!model || !m_editing) return;

    int xdist = e->x() - m_dragStartX;
    int ydist = e->y() - m_dragStartY;
    int newx = m_dragPointX + xdist;
    int newy = m_dragPointY + ydist;

    sv_frame_t frame = v->getFrameForX(newx);
    if (frame < 0) frame = 0;
    frame = frame / model->getResolution() * model->getResolution();

    double newValue = getValueForY(v, newy);
    float newEventValue = convertValueToEventValue(newValue);
    newEventValue = roundf(newEventValue);

    if (!m_editingCommand) {
        m_editingCommand = new ChangeEventsCommand
            (m_model.untyped, tr("Drag Point"));
    }

    m_editingCommand->remove(m_editingPoint);
    m_editingPoint = m_editingPoint
        .withFrame(frame)
        .withValue(newEventValue);
    m_editingCommand->add(m_editingPoint);
}

void
NoteLayer::editEnd(LayerGeometryProvider *, QMouseEvent *)
{
//    SVDEBUG << "NoteLayer::editEnd(" << e->x() << "," << e->y() << ")" << endl;
    auto model = ModelById::getAs<NoteModel>(m_model);
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
NoteLayer::editOpen(LayerGeometryProvider *v, QMouseEvent *e)
{
    auto model = ModelById::getAs<NoteModel>(m_model);
    if (!model) return false;

    Event note(0);
    if (!getPointToDrag(v, e->x(), e->y(), note)) return false;

    ItemEditDialog *dialog = new ItemEditDialog
        (model->getSampleRate(),
         ItemEditDialog::ShowTime |
         ItemEditDialog::ShowDuration |
         ItemEditDialog::ShowValue |
         ItemEditDialog::ShowLevel |
         ItemEditDialog::ShowText,
         getScaleUnits());

    dialog->setFrameTime(note.getFrame());
    dialog->setValue(note.getValue());
    dialog->setFrameDuration(note.getDuration());
    dialog->setText(note.getLabel());

    m_editingPoint = note;
    m_editIsOpen = true;
    
    if (dialog->exec() == QDialog::Accepted) {

        Event newNote = note
            .withFrame(dialog->getFrameTime())
            .withValue(dialog->getValue())
            .withDuration(dialog->getFrameDuration())
            .withLabel(dialog->getText());
        
        ChangeEventsCommand *command = new ChangeEventsCommand
            (m_model.untyped, tr("Edit Point"));
        command->remove(note);
        command->add(newNote);
        finish(command);
    }

    m_editingPoint = 0;
    m_editIsOpen = false;
        
    delete dialog;
    return true;
}

void
NoteLayer::moveSelection(Selection s, sv_frame_t newStartFrame)
{
    auto model = ModelById::getAs<NoteModel>(m_model);
    if (!model) return;

    ChangeEventsCommand *command =
        new ChangeEventsCommand(m_model.untyped, tr("Drag Selection"));

    EventVector points =
        model->getEventsStartingWithin(s.getStartFrame(), s.getDuration());

    for (Event p: points) {
        command->remove(p);
        Event moved = p.withFrame(p.getFrame() +
                                  newStartFrame - s.getStartFrame());
        command->add(moved);
    }

    finish(command);
}

void
NoteLayer::resizeSelection(Selection s, Selection newSize)
{
    auto model = ModelById::getAs<NoteModel>(m_model);
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
}

void
NoteLayer::deleteSelection(Selection s)
{
    auto model = ModelById::getAs<NoteModel>(m_model);
    if (!model) return;

    ChangeEventsCommand *command =
        new ChangeEventsCommand(m_model.untyped, tr("Delete Selected Points"));

    EventVector points =
        model->getEventsStartingWithin(s.getStartFrame(), s.getDuration());

    for (Event p: points) {
        command->remove(p);
    }

    finish(command);
}    

void
NoteLayer::copy(LayerGeometryProvider *v, Selection s, Clipboard &to)
{
    auto model = ModelById::getAs<NoteModel>(m_model);
    if (!model) return;

    EventVector points =
        model->getEventsStartingWithin(s.getStartFrame(), s.getDuration());

    for (Event p: points) {
        to.addPoint(p.withReferenceFrame(alignToReference(v, p.getFrame())));
    }
}

bool
NoteLayer::paste(LayerGeometryProvider *v, const Clipboard &from,
                 sv_frame_t /* frameOffset */, bool /* interactive */)
{
    auto model = ModelById::getAs<NoteModel>(m_model);
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
    return true;
}

void
NoteLayer::addNoteOn(sv_frame_t frame, int pitch, int velocity)
{
    double value = Pitch::getFrequencyForPitch(pitch);
    float eventValue = convertValueToEventValue(value);
    m_pendingNoteOns.insert(Event(frame, eventValue, 0,
                                  float(velocity) / 127.f, QString()));
}

void
NoteLayer::addNoteOff(sv_frame_t frame, int pitch)
{
    auto model = ModelById::getAs<NoteModel>(m_model);

    for (NoteSet::iterator i = m_pendingNoteOns.begin();
         i != m_pendingNoteOns.end(); ++i) {

        Event p = *i;
        double value = valueOf(p);
        int eventPitch = Pitch::getPitchForFrequency(value);

        if (eventPitch == pitch) {
            m_pendingNoteOns.erase(i);
            Event note = p.withDuration(frame - p.getFrame());
            if (model) {
                ChangeEventsCommand *c = new ChangeEventsCommand
                    (m_model.untyped, tr("Record Note"));
                c->add(note);
                // execute and bundle:
                CommandHistory::getInstance()->addCommand(c, true, true);
            }
            break;
        }
    }
}

void
NoteLayer::abandonNoteOns()
{
    m_pendingNoteOns.clear();
}

int
NoteLayer::getDefaultColourHint(bool darkbg, bool &impose)
{
    impose = false;
    return ColourDatabase::getInstance()->getColourIndex
        (QString(darkbg ? "White" : "Black"));
}

void
NoteLayer::toXml(QTextStream &stream,
                 QString indent, QString extraAttributes) const
{
    SingleColourLayer::toXml(stream, indent, extraAttributes +
                             QString(" verticalScale=\"%1\" scaleMinimum=\"%2\" scaleMaximum=\"%3\" ")
                             .arg(m_verticalScale)
                             .arg(m_scaleMinimum)
                             .arg(m_scaleMaximum));
}

void
NoteLayer::setProperties(const QXmlAttributes &attributes)
{
    SingleColourLayer::setProperties(attributes);

    bool ok, alsoOk;
    VerticalScale scale = (VerticalScale)
        attributes.value("verticalScale").toInt(&ok);
    if (ok) setVerticalScale(scale);

    float min = attributes.value("scaleMinimum").toFloat(&ok);
    float max = attributes.value("scaleMaximum").toFloat(&alsoOk);
    if (ok && alsoOk && min != max) setDisplayExtents(min, max);
}


