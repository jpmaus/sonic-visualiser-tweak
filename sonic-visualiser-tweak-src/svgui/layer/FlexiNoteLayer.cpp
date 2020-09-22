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

#include "FlexiNoteLayer.h"

#include "data/model/Model.h"
#include "data/model/SparseTimeValueModel.h"
#include "base/RealTime.h"
#include "base/Profiler.h"
#include "base/Pitch.h"
#include "base/LogRange.h"
#include "base/RangeMapper.h"

#include "ColourDatabase.h"
#include "LayerGeometryProvider.h"
#include "PianoScale.h"
#include "LinearNumericalScale.h"
#include "LogNumericalScale.h"
#include "PaintAssistant.h"

#include "data/model/NoteModel.h"

#include "view/View.h"

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
#include <limits> // GF: included to compile std::numerical_limits on linux
#include <vector>

#define NOTE_HEIGHT 16

FlexiNoteLayer::FlexiNoteLayer() :
    SingleColourLayer(),
    m_editing(false),
    m_intelligentActions(true),
    m_dragPointX(0),
    m_dragPointY(0),
    m_dragStartX(0),
    m_dragStartY(0),
    m_originalPoint(0, 0.0, 0, 1.f, tr("New Point")),
    m_editingPoint(0, 0.0, 0, 1.f, tr("New Point")),
    m_greatestLeftNeighbourFrame(0),
    m_smallestRightNeighbourFrame(0),
    m_editingCommand(nullptr),
    m_verticalScale(AutoAlignScale),
    m_editMode(DragNote),
    m_scaleMinimum(34), 
    m_scaleMaximum(77)
{
}

void
FlexiNoteLayer::setModel(ModelId modelId) 
{
    auto newModel = ModelById::getAs<NoteModel>(modelId);
    
    if (!modelId.isNone() && !newModel) {
        throw std::logic_error("Not a NoteModel");
    }
    
    if (m_model == modelId) return;
    m_model = modelId;

    if (newModel) {
        connectSignals(m_model);
    }

    emit modelReplaced();
}

Layer::PropertyList
FlexiNoteLayer::getProperties() const
{
    PropertyList list = SingleColourLayer::getProperties();
    list.push_back("Vertical Scale");
    list.push_back("Scale Units");
    return list;
}

QString
FlexiNoteLayer::getPropertyLabel(const PropertyName &name) const
{
    if (name == "Vertical Scale") return tr("Vertical Scale");
    if (name == "Scale Units") return tr("Scale Units");
    return SingleColourLayer::getPropertyLabel(name);
}

Layer::PropertyType
FlexiNoteLayer::getPropertyType(const PropertyName &name) const
{
    if (name == "Scale Units") return UnitsProperty;
    if (name == "Vertical Scale") return ValueProperty;
    return SingleColourLayer::getPropertyType(name);
}

QString
FlexiNoteLayer::getPropertyGroupName(const PropertyName &name) const
{
    if (name == "Vertical Scale" || name == "Scale Units") {
        return tr("Scale");
    }
    return SingleColourLayer::getPropertyGroupName(name);
}

QString
FlexiNoteLayer::getScaleUnits() const
{
    auto model = ModelById::getAs<NoteModel>(m_model);
    if (model) return model->getScaleUnits();
    else return "";
}

int
FlexiNoteLayer::getPropertyRangeAndValue(const PropertyName &name,
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
                (getScaleUnits());
        }

    } else {

        val = SingleColourLayer::getPropertyRangeAndValue(name, min, max, deflt);
    }

    return val;
}

QString
FlexiNoteLayer::getPropertyValueLabel(const PropertyName &name,
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
FlexiNoteLayer::setProperty(const PropertyName &name, int value)
{
    if (name == "Vertical Scale") {
        setVerticalScale(VerticalScale(value));
    } else if (name == "Scale Units") {
        auto model = ModelById::getAs<NoteModel>(m_model);
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
FlexiNoteLayer::setVerticalScale(VerticalScale scale)
{
    if (m_verticalScale == scale) return;
    m_verticalScale = scale;
    emit layerParametersChanged();
}

bool
FlexiNoteLayer::isLayerScrollable(const LayerGeometryProvider *v) const
{
    QPoint discard;
    return !v->shouldIlluminateLocalFeatures(this, discard);
}

bool
FlexiNoteLayer::shouldConvertMIDIToHz() const
{
    QString unit = getScaleUnits();
    return (unit != "Hz");
//    if (unit == "" ||
//        unit.startsWith("MIDI") ||
//        unit.startsWith("midi")) return true;
//    return false;
}

int
FlexiNoteLayer::getCompletion(LayerGeometryProvider *) const
{
    auto model = ModelById::get(m_model);
    if (model) return model->getCompletion();
    else return 0;
}

bool
FlexiNoteLayer::getValueExtents(double &min, double &max,
                                bool &logarithmic, QString &unit) const
{
    auto model = ModelById::getAs<NoteModel>(m_model);
    if (!model) return false;
    min = model->getValueMinimum();
    max = model->getValueMaximum();

    if (shouldConvertMIDIToHz()) {
        unit = "Hz";
        min = Pitch::getFrequencyForPitch(int(lrint(min)));
        max = Pitch::getFrequencyForPitch(int(lrint(max + 1)));
    } else unit = getScaleUnits();

    if (m_verticalScale == MIDIRangeScale ||
        m_verticalScale == LogScale) logarithmic = true;

    return true;
}

bool
FlexiNoteLayer::getDisplayExtents(double &min, double &max) const
{
    auto model = ModelById::getAs<NoteModel>(m_model);
    if (!model || shouldAutoAlign()) {
//        std::cerr << "No model or shouldAutoAlign()" << std::endl;
        return false;
    }

    if (m_verticalScale == MIDIRangeScale) {
        min = Pitch::getFrequencyForPitch(0);
        max = Pitch::getFrequencyForPitch(127);
        return true;
    }

    if (m_scaleMinimum == m_scaleMaximum) {
        min = model->getValueMinimum();
        max = model->getValueMaximum();
    } else {
        min = m_scaleMinimum;
        max = m_scaleMaximum;
    }

    if (shouldConvertMIDIToHz()) {
        min = Pitch::getFrequencyForPitch(int(lrint(min)));
        max = Pitch::getFrequencyForPitch(int(lrint(max + 1)));
    }

#ifdef DEBUG_NOTE_LAYER
    cerr << "NoteLayer::getDisplayExtents: min = " << min << ", max = " << max << " (m_scaleMinimum = " << m_scaleMinimum << ", m_scaleMaximum = " << m_scaleMaximum << ")" << endl;
#endif

    return true;
}

bool
FlexiNoteLayer::setDisplayExtents(double min, double max)
{
    auto model = ModelById::getAs<NoteModel>(m_model);
    if (!model) return false;

    if (min == max) {
        if (min == 0.f) {
            max = 1.f;
        } else {
            max = min * 1.0001f;
        }
    }

    m_scaleMinimum = min;
    m_scaleMaximum = max;

#ifdef DEBUG_NOTE_LAYER
    cerr << "FlexiNoteLayer::setDisplayExtents: min = " << min << ", max = " << max << endl;
#endif
    
    emit layerParametersChanged();
    return true;
}

int
FlexiNoteLayer::getVerticalZoomSteps(int &defaultStep) const
{
    if (shouldAutoAlign()) return 0;
    auto model = ModelById::getAs<NoteModel>(m_model);
    if (!model) return 0;

    defaultStep = 0;
    return 100;
}

int
FlexiNoteLayer::getCurrentVerticalZoomStep() const
{
    if (shouldAutoAlign()) return 0;
    auto model = ModelById::getAs<NoteModel>(m_model);
    if (!model) return 0;

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
FlexiNoteLayer::setVerticalZoomStep(int step)
{
    if (shouldAutoAlign()) return;
    auto model = ModelById::getAs<NoteModel>(m_model);
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
    cerr << "FlexiNoteLayer::setVerticalZoomStep: " << step << ": " << newmin << " -> " << newmax << " (range " << newdist << ")" << endl;
#endif

    setDisplayExtents(newmin, newmax);
}

RangeMapper *
FlexiNoteLayer::getNewVerticalZoomRangeMapper() const
{
    auto model = ModelById::getAs<NoteModel>(m_model);
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
FlexiNoteLayer::getLocalPoints(LayerGeometryProvider *v, int x) const
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
FlexiNoteLayer::getPointToDrag(LayerGeometryProvider *v, int x, int y, Event &point) const
{
    auto model = ModelById::getAs<NoteModel>(m_model);
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

bool
FlexiNoteLayer::getNoteToEdit(LayerGeometryProvider *v, int x, int y, Event &point) const
{
    // GF: find the note that is closest to the cursor
    auto model = ModelById::getAs<NoteModel>(m_model);
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
FlexiNoteLayer::getFeatureDescription(LayerGeometryProvider *v, QPoint &pos) const
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

    Event note(0);
    EventVector::iterator i;

    for (i = points.begin(); i != points.end(); ++i) {

        int y = getYForValue(v, i->getValue());
        int h = NOTE_HEIGHT; // GF: larger notes

        if (model->getValueQuantization() != 0.0) {
            h = y - getYForValue
                (v, i->getValue() + model->getValueQuantization());
            if (h < NOTE_HEIGHT) h = NOTE_HEIGHT;
        }

        // GF: this is not quite correct
        if (pos.y() >= y - 4 && pos.y() <= y + h) {
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

    if (shouldConvertMIDIToHz()) {

        int mnote = int(lrint(note.getValue()));
        int cents = int(lrint((note.getValue() - double(mnote)) * 100));
        double freq = Pitch::getFrequencyForPitch(mnote, cents);
        pitchText = tr("%1 (%2, %3 Hz)")
            .arg(Pitch::getPitchLabel(mnote, cents))
            .arg(mnote)
            .arg(freq);

    } else if (getScaleUnits() == "Hz") {

        pitchText = tr("%1 Hz (%2, %3)")
            .arg(note.getValue())
            .arg(Pitch::getPitchLabelForFrequency(note.getValue()))
            .arg(Pitch::getPitchForFrequency(note.getValue()));

    } else {
        pitchText = tr("%1 %2")
            .arg(note.getValue()).arg(getScaleUnits());
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
                 getYForValue(v, note.getValue()));
    return text;
}

bool
FlexiNoteLayer::snapToFeatureFrame(LayerGeometryProvider *v, sv_frame_t &frame,
                                   int &resolution,
                                   SnapType snap, int ycoord) const
{
    auto model = ModelById::getAs<NoteModel>(m_model);
    if (!model) {
        return Layer::snapToFeatureFrame(v, frame, resolution, snap, ycoord);
    }

    resolution = model->getResolution();
    EventVector points;

    if (snap == SnapNeighbouring) {
    
        points = getLocalPoints(v, v->getXForFrame(frame));
        if (points.empty()) return false;
        frame = points.begin()->getFrame();
        return true;
    }    

    points = model->getEventsCovering(frame);
    sv_frame_t snapped = frame;
    bool found = false;

    for (EventVector::const_iterator i = points.begin();
         i != points.end(); ++i) {

        if (snap == SnapRight) {

            if (i->getFrame() > frame) {
                snapped = i->getFrame();
                found = true;
                break;
            } else if (i->getFrame() + i->getDuration() >= frame) {
                snapped = i->getFrame() + i->getDuration();
                found = true;
                break;
            }

        } else if (snap == SnapLeft) {

            if (i->getFrame() <= frame) {
                snapped = i->getFrame();
                found = true; // don't break, as the next may be better
            } else {
                break;
            }

        } else { // nearest

            EventVector::const_iterator j = i;
            ++j;

            if (j == points.end()) {

                snapped = i->getFrame();
                found = true;
                break;

            } else if (j->getFrame() >= frame) {

                if (j->getFrame() - frame < frame - i->getFrame()) {
                    snapped = j->getFrame();
                } else {
                    snapped = i->getFrame();
                }
                found = true;
                break;
            }
        }
    }

    cerr << "snapToFeatureFrame: frame " << frame << " -> snapped " << snapped << ", found = " << found << endl;

    frame = snapped;
    return found;
}

void
FlexiNoteLayer::getScaleExtents(LayerGeometryProvider *v, double &min, double &max, bool &log) const
{
    min = 0.0;
    max = 0.0;
    log = false;

    QString queryUnits;
    if (shouldConvertMIDIToHz()) queryUnits = "Hz";
    else queryUnits = getScaleUnits();

    if (shouldAutoAlign()) {

        if (!v->getVisibleExtentsForUnit(queryUnits, min, max, log)) {

            auto model = ModelById::getAs<NoteModel>(m_model);
            min = model->getValueMinimum();
            max = model->getValueMaximum();

            if (shouldConvertMIDIToHz()) {
                min = Pitch::getFrequencyForPitch(int(lrint(min)));
                max = Pitch::getFrequencyForPitch(int(lrint(max + 1)));
            }

#ifdef DEBUG_NOTE_LAYER
            cerr << "FlexiNoteLayer[" << this << "]::getScaleExtents: min = " << min << ", max = " << max << ", log = " << log << endl;
#endif

        } else if (log) {

            LogRange::mapRange(min, max);

#ifdef DEBUG_NOTE_LAYER
            cerr << "FlexiNoteLayer[" << this << "]::getScaleExtents: min = " << min << ", max = " << max << ", log = " << log << endl;
#endif
        }

    } else {

        getDisplayExtents(min, max);

        if (m_verticalScale == MIDIRangeScale) {
            min = Pitch::getFrequencyForPitch(0);
            max = Pitch::getFrequencyForPitch(70);
        } else if (shouldConvertMIDIToHz()) {
            min = Pitch::getFrequencyForPitch(int(lrint(min)));
            max = Pitch::getFrequencyForPitch(int(lrint(max + 1)));
        }

        if (m_verticalScale == LogScale || m_verticalScale == MIDIRangeScale) {
            LogRange::mapRange(min, max);
            log = true;
        }
    }

    if (max == min) max = min + 1.0;
}

int
FlexiNoteLayer::getYForValue(LayerGeometryProvider *v, double val) const
{
    double min = 0.0, max = 0.0;
    bool logarithmic = false;
    int h = v->getPaintHeight();

    getScaleExtents(v, min, max, logarithmic);

#ifdef DEBUG_NOTE_LAYER
    cerr << "FlexiNoteLayer[" << this << "]::getYForValue(" << val << "): min = " << min << ", max = " << max << ", log = " << logarithmic << endl;
#endif

    if (shouldConvertMIDIToHz()) {
        val = Pitch::getFrequencyForPitch(int(lrint(val)),
                                          int(lrint((val - floor(val)) * 100.0)));
#ifdef DEBUG_NOTE_LAYER
        cerr << "shouldConvertMIDIToHz true, val now = " << val << endl;
#endif
    }

    if (logarithmic) {
        val = LogRange::map(val);
#ifdef DEBUG_NOTE_LAYER
        cerr << "logarithmic true, val now = " << val << endl;
#endif
    }

    int y = int(h - ((val - min) * h) / (max - min)) - 1;
#ifdef DEBUG_NOTE_LAYER
    cerr << "y = " << y << endl;
#endif
    return y;
}

double
FlexiNoteLayer::getValueForY(LayerGeometryProvider *v, int y) const
{
    double min = 0.0, max = 0.0;
    bool logarithmic = false;
    int h = v->getPaintHeight();

    getScaleExtents(v, min, max, logarithmic);

    double val = min + (double(h - y) * double(max - min)) / h;

    if (logarithmic) {
        val = pow(10.f, val);
    }

    if (shouldConvertMIDIToHz()) {
        val = Pitch::getPitchForFrequency(val);
    }

    return val;
}

bool
FlexiNoteLayer::shouldAutoAlign() const
{
    return (m_verticalScale == AutoAlignScale);
}

void
FlexiNoteLayer::paint(LayerGeometryProvider *v, QPainter &paint, QRect rect) const
{
    auto model = ModelById::getAs<NoteModel>(m_model);
    if (!model || !model->isOK()) return;

    sv_samplerate_t sampleRate = model->getSampleRate();
    if (!sampleRate) return;

//    Profiler profiler("FlexiNoteLayer::paint", true);

    int x0 = rect.left(), x1 = rect.right();
    sv_frame_t frame0 = v->getFrameForX(x0);
    sv_frame_t frame1 = v->getFrameForX(x1);

    EventVector points(model->getEventsSpanning(frame0, frame1 - frame0));
    if (points.empty()) return;

    paint.setPen(getBaseQColor());

    QColor brushColour(getBaseQColor());
    brushColour.setAlpha(80);

//    SVDEBUG << "FlexiNoteLayer::paint: resolution is "
//        << model->getResolution() << " frames" << endl;

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

    int noteNumber = -1;

    for (EventVector::const_iterator i = points.begin();
         i != points.end(); ++i) {

        const Event &p(*i);

        if (noteNumber < 0) {
            noteNumber = model->getIndexForEvent(p);
        } else {
            noteNumber ++;
        }

        int x = v->getXForFrame(p.getFrame());
        int y = getYForValue(v, p.getValue());
        int w = v->getXForFrame(p.getFrame() + p.getDuration()) - x;
        int h = NOTE_HEIGHT; //GF: larger notes
    
        if (model->getValueQuantization() != 0.0) {
            h = y - getYForValue(v, p.getValue() + model->getValueQuantization());
            if (h < NOTE_HEIGHT) h = NOTE_HEIGHT; //GF: larger notes
        }

        if (w < 1) w = 1;
        paint.setPen(getBaseQColor());
        paint.setBrush(brushColour);

        if (shouldIlluminate && illuminatePoint == p) {

            paint.drawLine(x, -1, x, v->getPaintHeight() + 1);
            paint.drawLine(x+w, -1, x+w, v->getPaintHeight() + 1);
        
            paint.setPen(v->getForeground());
        
            QString vlabel = tr("freq: %1%2")
                .arg(p.getValue()).arg(model->getScaleUnits());
            PaintAssistant::drawVisibleText
                (v, paint, 
                 x,
                 y - h/2 - 2 - paint.fontMetrics().height()
                 - paint.fontMetrics().descent(), 
                 vlabel, PaintAssistant::OutlinedText);

            QString hlabel = tr("dur: %1")
                .arg(RealTime::frame2RealTime
                     (p.getDuration(), model->getSampleRate()).toText(true)
                     .c_str());
            PaintAssistant::drawVisibleText
                (v, paint, 
                 x,
                 y - h/2 - paint.fontMetrics().descent() - 2,
                 hlabel, PaintAssistant::OutlinedText);

            QString llabel = QString("%1").arg(p.getLabel());
            PaintAssistant::drawVisibleText
                (v, paint, 
                 x,
                 y + h + 2 + paint.fontMetrics().descent(),
                 llabel, PaintAssistant::OutlinedText);

            QString nlabel = QString("%1").arg(noteNumber);
            PaintAssistant::drawVisibleText
                (v, paint, 
                 x + paint.fontMetrics().averageCharWidth() / 2,
                 y + h/2 - paint.fontMetrics().descent(),
                 nlabel, PaintAssistant::OutlinedText);
        }
    
        paint.drawRect(x, y - h/2, w, h);
    }

    paint.restore();
}

int
FlexiNoteLayer::getVerticalScaleWidth(LayerGeometryProvider *v, bool, QPainter &paint) const
{
    if (shouldAutoAlign()) {
        return 0;
    } else  {
        if (m_verticalScale == LogScale || m_verticalScale == MIDIRangeScale) {
            return LogNumericalScale().getWidth(v, paint) + 10; // for piano
        } else {
            return LinearNumericalScale().getWidth(v, paint);
        }
    }
}

void
FlexiNoteLayer::paintVerticalScale(LayerGeometryProvider *v, bool, QPainter &paint, QRect) const
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
    
    if (logarithmic && (getScaleUnits() == "Hz")) {
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
FlexiNoteLayer::drawStart(LayerGeometryProvider *v, QMouseEvent *e)
{
//    SVDEBUG << "FlexiNoteLayer::drawStart(" << e->x() << "," << e->y() << ")" << endl;

    auto model = ModelById::getAs<NoteModel>(m_model);
    if (!model) return;

    sv_frame_t frame = v->getFrameForX(e->x());
    if (frame < 0) frame = 0;
    frame = frame / model->getResolution() * model->getResolution();

    double value = getValueForY(v, e->y());

    m_editingPoint = Event(frame, float(value), 0, 0.8f, tr("New Point"));
    m_originalPoint = m_editingPoint;

    if (m_editingCommand) finish(m_editingCommand);
    m_editingCommand = new ChangeEventsCommand(m_model.untyped, tr("Draw Point"));
    m_editingCommand->add(m_editingPoint);

    m_editing = true;
}

void
FlexiNoteLayer::drawDrag(LayerGeometryProvider *v, QMouseEvent *e)
{
//    SVDEBUG << "FlexiNoteLayer::drawDrag(" << e->x() << "," << e->y() << ")" << endl;

    auto model = ModelById::getAs<NoteModel>(m_model);
    if (!model || !m_editing) return;

    sv_frame_t frame = v->getFrameForX(e->x());
    if (frame < 0) frame = 0;
    frame = frame / model->getResolution() * model->getResolution();

    double newValue = getValueForY(v, e->y());

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
}

void
FlexiNoteLayer::drawEnd(LayerGeometryProvider *, QMouseEvent *)
{
//    SVDEBUG << "FlexiNoteLayer::drawEnd(" << e->x() << "," << e->y() << ")" << endl;
    auto model = ModelById::getAs<NoteModel>(m_model);
    if (!model || !m_editing) return;
    finish(m_editingCommand);
    m_editingCommand = nullptr;
    m_editing = false;
}

void
FlexiNoteLayer::eraseStart(LayerGeometryProvider *v, QMouseEvent *e)
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
FlexiNoteLayer::eraseDrag(LayerGeometryProvider *, QMouseEvent *)
{
}

void
FlexiNoteLayer::eraseEnd(LayerGeometryProvider *v, QMouseEvent *e)
{
    if (!m_editing) return;
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
FlexiNoteLayer::editStart(LayerGeometryProvider *v, QMouseEvent *e)
{
//    SVDEBUG << "FlexiNoteLayer::editStart(" << e->x() << "," << e->y() << ")" << endl;
    std::cerr << "FlexiNoteLayer::editStart(" << e->x() << "," << e->y() << ")" << std::endl;

    auto model = ModelById::getAs<NoteModel>(m_model);
    if (!model) return;

    if (!getPointToDrag(v, e->x(), e->y(), m_editingPoint)) return;
    m_originalPoint = m_editingPoint;
    
    if (m_editMode == RightBoundary) {
        m_dragPointX = v->getXForFrame
            (m_editingPoint.getFrame() + m_editingPoint.getDuration());
    } else {
        m_dragPointX = v->getXForFrame
            (m_editingPoint.getFrame());
    }
    m_dragPointY = getYForValue(v, m_editingPoint.getValue());

    if (m_editingCommand) {
        finish(m_editingCommand);
        m_editingCommand = nullptr;
    }

    m_editing = true;
    m_dragStartX = e->x();
    m_dragStartY = e->y();
    
    sv_frame_t onset = m_originalPoint.getFrame();
    sv_frame_t offset =
        m_originalPoint.getFrame() +
        m_originalPoint.getDuration() - 1;
    
    m_greatestLeftNeighbourFrame = -1;
    m_smallestRightNeighbourFrame = std::numeric_limits<int>::max();

    EventVector allEvents = model->getAllEvents();
    
    for (auto currentNote: allEvents) {
        
        // left boundary
        if (currentNote.getFrame() + currentNote.getDuration() - 1 < onset) {
            m_greatestLeftNeighbourFrame =
                currentNote.getFrame() + currentNote.getDuration() - 1;
        }
        
        // right boundary
        if (currentNote.getFrame() > offset) {
            m_smallestRightNeighbourFrame = currentNote.getFrame();
            break;
        }
    }

    std::cerr << "editStart: mode is " << m_editMode << ", note frame: " << onset << ", left boundary: " << m_greatestLeftNeighbourFrame << ", right boundary: " << m_smallestRightNeighbourFrame << std::endl;
}

void
FlexiNoteLayer::editDrag(LayerGeometryProvider *v, QMouseEvent *e)
{
//    SVDEBUG << "FlexiNoteLayer::editDrag(" << e->x() << "," << e->y() << ")" << endl;
    std::cerr << "FlexiNoteLayer::editDrag(" << e->x() << "," << e->y() << ")" << std::endl;

    auto model = ModelById::getAs<NoteModel>(m_model);
    if (!model || !m_editing) return;

    int xdist = e->x() - m_dragStartX;
    int ydist = e->y() - m_dragStartY;
    int newx = m_dragPointX + xdist;
    int newy = m_dragPointY + ydist;

    sv_frame_t dragFrame = v->getFrameForX(newx);
    if (dragFrame < 0) dragFrame = 0;
    dragFrame = dragFrame / model->getResolution() * model->getResolution();
    
    double value = getValueForY(v, newy);

    if (!m_editingCommand) {
        m_editingCommand =
            new ChangeEventsCommand(m_model.untyped, tr("Drag Point"));
    }
    m_editingCommand->remove(m_editingPoint);

    std::cerr << "edit mode: " << m_editMode << " intelligent actions = "
              << m_intelligentActions << std::endl;
    
    switch (m_editMode) {
        
    case LeftBoundary : {
        // left 
        if (m_intelligentActions &&
            dragFrame <= m_greatestLeftNeighbourFrame) {
            dragFrame = m_greatestLeftNeighbourFrame + 1;
        }
        // right
        if (m_intelligentActions &&
            dragFrame >= m_originalPoint.getFrame() + m_originalPoint.getDuration()) {
            dragFrame = m_originalPoint.getFrame() + m_originalPoint.getDuration() - 1;
        }
        m_editingPoint = m_editingPoint
            .withFrame(dragFrame)
            .withDuration(m_originalPoint.getFrame() -
                          dragFrame + m_originalPoint.getDuration());
        break;
    }
        
    case RightBoundary : {
        // left
        if (m_intelligentActions &&
            dragFrame <= m_greatestLeftNeighbourFrame) {
            dragFrame = m_greatestLeftNeighbourFrame + 1;
        }
        if (m_intelligentActions &&
            dragFrame >= m_smallestRightNeighbourFrame) {
            dragFrame = m_smallestRightNeighbourFrame - 1;
        }
        m_editingPoint = m_editingPoint
            .withDuration(dragFrame - m_originalPoint.getFrame() + 1);
        break;
    }
        
    case DragNote : {
        // left
        if (m_intelligentActions &&
            dragFrame <= m_greatestLeftNeighbourFrame) {
            dragFrame = m_greatestLeftNeighbourFrame + 1;
        }
        // right
        if (m_intelligentActions &&
            dragFrame + m_originalPoint.getDuration() >= m_smallestRightNeighbourFrame) {
            dragFrame = m_smallestRightNeighbourFrame - m_originalPoint.getDuration();
        }
        
        m_editingPoint = m_editingPoint
            .withFrame(dragFrame)
            .withValue(float(value));

        // Re-analyse region within +/- 1 semitone of the dragged value
        float cents = 0;
        int midiPitch = Pitch::getPitchForFrequency(m_editingPoint.getValue(), &cents);
        double lower = Pitch::getFrequencyForPitch(midiPitch - 1, cents);
        double higher = Pitch::getFrequencyForPitch(midiPitch + 1, cents);
        
        emit reAnalyseRegion(m_editingPoint.getFrame(),
                             m_editingPoint.getFrame() +
                             m_editingPoint.getDuration(),
                             float(lower), float(higher));
        break;
    }
        
    case SplitNote: // nothing
        break;
    }

    m_editingCommand->add(m_editingPoint);

    std::cerr << "added new point(" << m_editingPoint.getFrame() << "," << m_editingPoint.getDuration() << ")" << std::endl;
}

void
FlexiNoteLayer::editEnd(LayerGeometryProvider *v, QMouseEvent *e)
{
    std::cerr << "FlexiNoteLayer::editEnd("
              << e->x() << "," << e->y() << ")" << std::endl;
    
    auto model = ModelById::getAs<NoteModel>(m_model);
    if (!model || !m_editing) return;

    if (m_editingCommand) {

        QString newName = m_editingCommand->getName();

        if (m_editMode == DragNote) {
            //!!! command nesting is wrong?
            emit materialiseReAnalysis();
        }

        m_editingCommand->remove(m_editingPoint);
        updateNoteValueFromPitchCurve(v, m_editingPoint);
        m_editingCommand->add(m_editingPoint);

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

void
FlexiNoteLayer::splitStart(LayerGeometryProvider *v, QMouseEvent *e)
{
    auto model = ModelById::getAs<NoteModel>(m_model);
    if (!model) return;

    // GF: note splitting starts (!! remove printing soon)
    std::cerr << "splitStart (n.b. editStart will be called later, if the user drags the mouse)" << std::endl;

    if (!getPointToDrag(v, e->x(), e->y(), m_editingPoint)) return;
    // m_originalPoint = m_editingPoint;
    // 
    // m_dragPointX = v->getXForFrame(m_editingPoint.getFrame());
    // m_dragPointY = getYForValue(v, m_editingPoint.getValue());

    if (m_editingCommand) {
        finish(m_editingCommand);
        m_editingCommand = nullptr;
    }

    m_editing = true;
    m_dragStartX = e->x();
    m_dragStartY = e->y();
}

void
FlexiNoteLayer::splitEnd(LayerGeometryProvider *v, QMouseEvent *e)
{
    auto model = ModelById::getAs<NoteModel>(m_model);
    // GF: note splitting ends. (!! remove printing soon)
    std::cerr << "splitEnd" << std::endl;
    if (!model || !m_editing || m_editMode != SplitNote) return;

    int xdist = e->x() - m_dragStartX;
    int ydist = e->y() - m_dragStartY;
    if (xdist != 0 || ydist != 0) { 
        std::cerr << "mouse moved" << std::endl;    
        return; 
    }

    sv_frame_t frame = v->getFrameForX(e->x());

    splitNotesAt(v, frame, e);
}

void
FlexiNoteLayer::splitNotesAt(LayerGeometryProvider *v, sv_frame_t frame)
{
    splitNotesAt(v, frame, nullptr);
}

void
FlexiNoteLayer::splitNotesAt(LayerGeometryProvider *v, sv_frame_t frame, QMouseEvent *e)
{
    auto model = ModelById::getAs<NoteModel>(m_model);
    if (!model) return;
    
    EventVector onPoints = model->getEventsCovering(frame);
    if (onPoints.empty()) return;
    
    Event note(*onPoints.begin());

    auto command = new ChangeEventsCommand(m_model.untyped, tr("Edit Point"));
    command->remove(note);

    if (!e || !(e->modifiers() & Qt::ShiftModifier)) {

        int gap = 0; // MM: I prefer a gap of 0, but we can decide later
    
        Event newNote1(note.getFrame(), note.getValue(), 
                       frame - note.getFrame() - gap, 
                       note.getLevel(), note.getLabel());
    
        Event newNote2(frame, note.getValue(), 
                       note.getDuration() - newNote1.getDuration(), 
                       note.getLevel(), note.getLabel());
                       
        if (m_intelligentActions) {
            if (updateNoteValueFromPitchCurve(v, newNote1)) {
                command->add(newNote1);
            }
            if (updateNoteValueFromPitchCurve(v, newNote2)) {
                command->add(newNote2);
            }
        } else {
            command->add(newNote1);
            command->add(newNote2);
        }
    }

    finish(command);
}

void
FlexiNoteLayer::addNote(LayerGeometryProvider *v, QMouseEvent *e)
{
    auto model = ModelById::getAs<NoteModel>(m_model);
    std::cerr << "addNote" << std::endl;
    if (!model) return;

    sv_frame_t duration = 10000;
    
    sv_frame_t frame = v->getFrameForX(e->x());
    double value = getValueForY(v, e->y());
    
    EventVector noteList = model->getAllEvents();

    if (m_intelligentActions) {
        sv_frame_t smallestRightNeighbourFrame = 0;
        for (EventVector::const_iterator i = noteList.begin();
             i != noteList.end(); ++i) {
            Event currentNote = *i;
            if (currentNote.getFrame() > frame) {
                smallestRightNeighbourFrame = currentNote.getFrame();
                break;
            }
        }
        if (smallestRightNeighbourFrame > 0) {
            duration = std::min(smallestRightNeighbourFrame - frame + 1, duration);
            duration = (duration > 0) ? duration : 0;
        }
    }

    if (!m_intelligentActions || 
        (model->getEventsCovering(frame).empty() && duration > 0)) {
        Event newNote(frame, float(value), duration, 100.f, tr("new note"));
        auto command = new ChangeEventsCommand(m_model.untyped, tr("Add Point"));
        command->add(newNote);
        finish(command);
    }
}

ModelId
FlexiNoteLayer::getAssociatedPitchModel(LayerGeometryProvider *v) const
{
    // Better than we used to do, but still not very satisfactory

//    cerr << "FlexiNoteLayer::getAssociatedPitchModel()" << endl;

    for (int i = 0; i < v->getView()->getLayerCount(); ++i) {
        Layer *layer = v->getView()->getLayer(i);
        if (layer &&
            layer->getLayerPresentationName() != "candidate") {
//            cerr << "FlexiNoteLayer::getAssociatedPitchModel: looks like our layer is " << layer << endl;
            auto modelId = layer->getModel();
            auto model = ModelById::getAs<SparseTimeValueModel>(modelId);
            if (model && model->getScaleUnits() == "Hz") {
//                cerr << "FlexiNoteLayer::getAssociatedPitchModel: it's good, returning " << model << endl;
                return modelId;
            }
        }
    }
//    cerr << "FlexiNoteLayer::getAssociatedPitchModel: failed to find a model" << endl;
    return {};
}

void
FlexiNoteLayer::snapSelectedNotesToPitchTrack(LayerGeometryProvider *v, Selection s)
{
    auto model = ModelById::getAs<NoteModel>(m_model);
    if (!model) return;

    EventVector points =
        model->getEventsStartingWithin(s.getStartFrame(), s.getDuration());

    auto command = new ChangeEventsCommand(m_model.untyped, tr("Snap Notes"));

    cerr << "snapSelectedNotesToPitchTrack: selection is from " << s.getStartFrame() << " to " << s.getEndFrame() << endl;

    for (EventVector::iterator i = points.begin();
         i != points.end(); ++i) {

        Event note(*i);

        cerr << "snapSelectedNotesToPitchTrack: looking at note from " << note.getFrame() << " to " << note.getFrame() + note.getDuration() << endl;

        if (!s.contains(note.getFrame()) &&
            !s.contains(note.getFrame() + note.getDuration() - 1)) {
            continue;
        }

        cerr << "snapSelectedNotesToPitchTrack: making new note" << endl;
        Event newNote(note);

        command->remove(note);

        if (updateNoteValueFromPitchCurve(v, newNote)) {
            command->add(newNote);
        }
    }
    
    finish(command);
}

void
FlexiNoteLayer::mergeNotes(LayerGeometryProvider *v, Selection s, bool inclusive)
{
    auto model = ModelById::getAs<NoteModel>(m_model);
    if (!model) return;
    
    EventVector points;
    if (inclusive) {
        points = model->getEventsSpanning(s.getStartFrame(), s.getDuration());
    } else {
        points = model->getEventsWithin(s.getStartFrame(), s.getDuration());
    }
        
    EventVector::iterator i = points.begin();
    if (i == points.end()) return;

    auto command = new ChangeEventsCommand(m_model.untyped, tr("Merge Notes"));

    Event newNote(*i);

    while (i != points.end()) {

        if (inclusive) {
            if (i->getFrame() >= s.getEndFrame()) break;
        } else {
            if (i->getFrame() + i->getDuration() > s.getEndFrame()) break;
        }

        newNote = newNote.withDuration
            (i->getFrame() + i->getDuration() - newNote.getFrame());
        command->remove(*i);

        ++i;
    }

    updateNoteValueFromPitchCurve(v, newNote);
    command->add(newNote);
    finish(command);
}

bool
FlexiNoteLayer::updateNoteValueFromPitchCurve(LayerGeometryProvider *v, Event &note) const
{
    ModelId modelId = getAssociatedPitchModel(v);
    auto model = ModelById::getAs<SparseTimeValueModel>(modelId);
    if (!model) return false;
        
    std::cerr << model->getTypeName() << std::endl;

    EventVector dataPoints =
        model->getEventsWithin(note.getFrame(), note.getDuration());
   
    std::cerr << "frame " << note.getFrame() << ": " << dataPoints.size() << " candidate points" << std::endl;
   
    if (dataPoints.empty()) return false;

    std::vector<double> pitchValues;
   
    for (EventVector::const_iterator i =
             dataPoints.begin(); i != dataPoints.end(); ++i) {
        pitchValues.push_back(i->getValue());
    }
        
    if (pitchValues.empty()) return false;

    sort(pitchValues.begin(), pitchValues.end());
    int size = int(pitchValues.size());
    double median;

    if (size % 2 == 0) {
        median = (pitchValues[size/2 - 1] + pitchValues[size/2]) / 2;
    } else {
        median = pitchValues[size/2];
    }

    std::cerr << "updateNoteValueFromPitchCurve: corrected from " << note.getValue() << " to median " << median << std::endl;
    
    note = note.withValue(float(median));

    return true;
}

void 
FlexiNoteLayer::mouseMoveEvent(LayerGeometryProvider *v, QMouseEvent *e)
{
    // GF: context sensitive cursors
    // v->getView()->setCursor(Qt::ArrowCursor);
    Event note(0);
    if (!getNoteToEdit(v, e->x(), e->y(), note)) { 
        // v->getView()->setCursor(Qt::UpArrowCursor);
        return; 
    }

    bool closeToLeft = false, closeToRight = false,
        closeToTop = false, closeToBottom = false;
    getRelativeMousePosition(v, note, e->x(), e->y(),
                             closeToLeft, closeToRight,
                             closeToTop, closeToBottom);
    
    if (closeToLeft) {
        v->getView()->setCursor(Qt::SizeHorCursor);
        m_editMode = LeftBoundary;
        cerr << "edit mode -> LeftBoundary" << endl;
    } else if (closeToRight) {
        v->getView()->setCursor(Qt::SizeHorCursor);
        m_editMode = RightBoundary;
        cerr << "edit mode -> RightBoundary" << endl;
    } else if (closeToTop) {
        v->getView()->setCursor(Qt::CrossCursor);
        m_editMode = DragNote;
        cerr << "edit mode -> DragNote" << endl;
    } else if (closeToBottom) {
        v->getView()->setCursor(Qt::UpArrowCursor);
        m_editMode = SplitNote;
        cerr << "edit mode -> SplitNote" << endl;
    } else {
        v->getView()->setCursor(Qt::ArrowCursor);
    }
}

void
FlexiNoteLayer::getRelativeMousePosition(LayerGeometryProvider *v, Event &note, int x, int y, bool &closeToLeft, bool &closeToRight, bool &closeToTop, bool &closeToBottom) const
{
    // GF: TODO: consolidate the tolerance values

    int ctol = 0;
    int noteStartX = v->getXForFrame(note.getFrame());
    int noteEndX = v->getXForFrame(note.getFrame() + note.getDuration());
    int noteValueY = getYForValue(v,note.getValue());
    int noteStartY = noteValueY - (NOTE_HEIGHT / 2);
    int noteEndY = noteValueY + (NOTE_HEIGHT / 2);
    
    bool closeToNote = false;
    
    if (y >= noteStartY-ctol && y <= noteEndY+ctol && x >= noteStartX-ctol && x <= noteEndX+ctol) closeToNote = true;
    if (!closeToNote) return;
    
    int tol = NOTE_HEIGHT / 2;
    
    if (x >= noteStartX - tol && x <= noteStartX + tol) closeToLeft = true;
    if (x >= noteEndX - tol && x <= noteEndX + tol) closeToRight = true;
    if (y >= noteStartY - tol && y <= noteStartY + tol) closeToTop = true;
    if (y >= noteEndY - tol && y <= noteEndY + tol) closeToBottom = true;

//    cerr << "FlexiNoteLayer::getRelativeMousePosition: close to: left " << closeToLeft << " right " << closeToRight << " top " << closeToTop << " bottom " << closeToBottom << endl;
}


bool
FlexiNoteLayer::editOpen(LayerGeometryProvider *v, QMouseEvent *e)
{
    std::cerr << "Opening note editor dialog" << std::endl;
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

    if (dialog->exec() == QDialog::Accepted) {

        Event newNote = note
            .withFrame(dialog->getFrameTime())
            .withValue(dialog->getValue())
            .withDuration(dialog->getFrameDuration())
            .withLabel(dialog->getText());
        
        auto command = new ChangeEventsCommand(m_model.untyped, tr("Edit Point"));
        command->remove(note);
        command->add(newNote);
        finish(command);
    }

    delete dialog;
    return true;
}

void
FlexiNoteLayer::moveSelection(Selection s, sv_frame_t newStartFrame)
{
    auto model = ModelById::getAs<NoteModel>(m_model);
    if (!model) return;
    
    auto command = new ChangeEventsCommand(m_model.untyped, tr("Drag Selection"));

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
FlexiNoteLayer::resizeSelection(Selection s, Selection newSize)
{
    auto model = ModelById::getAs<NoteModel>(m_model);
    if (!model || !s.getDuration()) return;

    auto command = new ChangeEventsCommand(m_model.untyped, tr("Resize Selection"));

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
FlexiNoteLayer::deleteSelection(Selection s)
{
    auto model = ModelById::getAs<NoteModel>(m_model);
    if (!model) return;

    auto command =
        new ChangeEventsCommand(m_model.untyped, tr("Delete Selected Points"));

    EventVector points =
        model->getEventsStartingWithin(s.getStartFrame(), s.getDuration());

    for (Event p: points) {
        command->remove(p);
    }

    finish(command);
}    

void
FlexiNoteLayer::deleteSelectionInclusive(Selection s)
{
    auto model = ModelById::getAs<NoteModel>(m_model);
    if (!model) return;

    auto command =
        new ChangeEventsCommand(m_model.untyped, tr("Delete Selected Points"));

    EventVector points =
        model->getEventsSpanning(s.getStartFrame(), s.getDuration());

    for (Event p: points) {
        command->remove(p);
    }

    finish(command);
}

void
FlexiNoteLayer::copy(LayerGeometryProvider *v, Selection s, Clipboard &to)
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
FlexiNoteLayer::paste(LayerGeometryProvider *v, const Clipboard &from,
                      sv_frame_t /*frameOffset */, bool /* interactive */)
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

    auto command = new ChangeEventsCommand(m_model.untyped, tr("Paste"));

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
FlexiNoteLayer::addNoteOn(sv_frame_t frame, int pitch, int velocity)
{
    m_pendingNoteOns.insert(Event(frame, float(pitch), 0,
                                  float(velocity / 127.0), ""));
}

void
FlexiNoteLayer::addNoteOff(sv_frame_t frame, int pitch)
{
    for (NoteSet::iterator i = m_pendingNoteOns.begin();
         i != m_pendingNoteOns.end(); ++i) {

        Event p = *i;

        if (lrintf(p.getValue()) == pitch) {
            m_pendingNoteOns.erase(i);
            Event note = p.withDuration(frame - p.getFrame());
            auto c = new ChangeEventsCommand
                (m_model.untyped, tr("Record Note"));
            c->add(note);
            // execute and bundle:
            CommandHistory::getInstance()->addCommand(c, true, true);
            break;
        }
    }
}

void
FlexiNoteLayer::abandonNoteOns()
{
    m_pendingNoteOns.clear();
}

int
FlexiNoteLayer::getDefaultColourHint(bool darkbg, bool &impose)
{
    impose = false;
    return ColourDatabase::getInstance()->getColourIndex
        (QString(darkbg ? "White" : "Black"));
}

void
FlexiNoteLayer::toXml(QTextStream &stream,
                      QString indent, QString extraAttributes) const
{
    SingleColourLayer::toXml(stream, indent, extraAttributes +
                             QString(" verticalScale=\"%1\" scaleMinimum=\"%2\" scaleMaximum=\"%3\" ")
                             .arg(m_verticalScale)
                             .arg(m_scaleMinimum)
                             .arg(m_scaleMaximum));
}

void
FlexiNoteLayer::setProperties(const QXmlAttributes &attributes)
{
    SingleColourLayer::setProperties(attributes);

    bool ok;
    VerticalScale scale = (VerticalScale)
        attributes.value("verticalScale").toInt(&ok);
    if (ok) setVerticalScale(scale);
}

void
FlexiNoteLayer::setVerticalRangeToNoteRange(LayerGeometryProvider *v)
{
    auto model = ModelById::getAs<NoteModel>(m_model);
    if (!model) return;
    
    double minf = std::numeric_limits<double>::max();
    double maxf = 0;
    bool hasNotes = 0;
    EventVector allPoints = model->getAllEvents();
    for (EventVector::const_iterator i = allPoints.begin();
         i != allPoints.end(); ++i) {
        hasNotes = 1;
        Event note = *i;
        if (note.getValue() < minf) minf = note.getValue();
        if (note.getValue() > maxf) maxf = note.getValue();
    }
    
    std::cerr << "min frequency:" << minf << ", max frequency: " << maxf << std::endl;
    
    if (hasNotes) {
        v->getView()->getLayer(1)->setDisplayExtents(minf*0.66,maxf*1.5); 
        // MM: this is a hack because we rely on 
        // * this layer being automatically aligned to layer 1
        // * layer one is a log frequency layer.
    }
}


