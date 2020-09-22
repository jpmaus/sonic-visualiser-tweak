/* -*- c-basic-offset: 4 indent-tabs-mode: nil -*-  vi:set ts=8 sts=4 sw=4: */

/*
    Sonic Visualiser
    An audio file viewer and annotation editor.
    Centre for Digital Music, Queen Mary, University of London.
    
    This program is free software; you can redistribute it and/or
    modify it under the terms of the GNU General Public License as
    published by the Free Software Foundation; either version 2 of the
    License, or (at your option) any later version.  See the file
    COPYING included with this distribution for more information.
*/

#include "BoxLayer.h"

#include "data/model/Model.h"
#include "base/RealTime.h"
#include "base/Profiler.h"
#include "base/LogRange.h"

#include "ColourDatabase.h"
#include "ColourMapper.h"
#include "LinearNumericalScale.h"
#include "LogNumericalScale.h"
#include "PaintAssistant.h"

#include "view/View.h"

#include "data/model/BoxModel.h"

#include "widgets/ItemEditDialog.h"
#include "widgets/TextAbbrev.h"

#include <QPainter>
#include <QPainterPath>
#include <QMouseEvent>
#include <QTextStream>
#include <QMessageBox>

#include <iostream>
#include <cmath>

BoxLayer::BoxLayer() :
    SingleColourLayer(),
    m_editing(false),
    m_dragPointX(0),
    m_dragPointY(0),
    m_dragStartX(0),
    m_dragStartY(0),
    m_originalPoint(0, 0.0, 0, tr("New Box")),
    m_editingPoint(0, 0.0, 0, tr("New Box")),
    m_editingCommand(nullptr),
    m_verticalScale(AutoAlignScale)
{
    
}

int
BoxLayer::getCompletion(LayerGeometryProvider *) const
{
    auto model = ModelById::get(m_model);
    if (model) return model->getCompletion();
    else return 0;
}

void
BoxLayer::setModel(ModelId modelId)
{
    auto oldModel = ModelById::getAs<BoxModel>(m_model);
    auto newModel = ModelById::getAs<BoxModel>(modelId);
    
    if (!modelId.isNone() && !newModel) {
        throw std::logic_error("Not a BoxModel");
    }
    
    if (m_model == modelId) return;
    m_model = modelId;

    if (newModel) {
        connectSignals(m_model);
    }
    
    emit modelReplaced();
}

Layer::PropertyList
BoxLayer::getProperties() const
{
    PropertyList list = SingleColourLayer::getProperties();
    list.push_back("Vertical Scale");
    list.push_back("Scale Units");
    return list;
}

QString
BoxLayer::getPropertyLabel(const PropertyName &name) const
{
    if (name == "Vertical Scale") return tr("Vertical Scale");
    if (name == "Scale Units") return tr("Scale Units");
    return SingleColourLayer::getPropertyLabel(name);
}

Layer::PropertyType
BoxLayer::getPropertyType(const PropertyName &name) const
{
    if (name == "Vertical Scale") return ValueProperty;
    if (name == "Scale Units") return UnitsProperty;
    return SingleColourLayer::getPropertyType(name);
}

QString
BoxLayer::getPropertyGroupName(const PropertyName &name) const
{
    if (name == "Vertical Scale" || name == "Scale Units") {
        return tr("Scale");
    }
    return SingleColourLayer::getPropertyGroupName(name);
}

int
BoxLayer::getPropertyRangeAndValue(const PropertyName &name,
                                    int *min, int *max, int *deflt) const
{
    int val = 0;

    if (name == "Vertical Scale") {
        
        if (min) *min = 0;
        if (max) *max = 2;
        if (deflt) *deflt = int(LinearScale);
        
        val = int(m_verticalScale);

    } else if (name == "Scale Units") {

        if (deflt) *deflt = 0;
        auto model = ModelById::getAs<BoxModel>(m_model);
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
BoxLayer::getPropertyValueLabel(const PropertyName &name,
                                   int value) const
{
    if (name == "Vertical Scale") {
        switch (value) {
        default:
        case 0: return tr("Auto-Align");
        case 1: return tr("Linear");
        case 2: return tr("Log");
        }
    }
    return SingleColourLayer::getPropertyValueLabel(name, value);
}

void
BoxLayer::setProperty(const PropertyName &name, int value)
{
    if (name == "Vertical Scale") {
        setVerticalScale(VerticalScale(value));
    } else if (name == "Scale Units") {
        auto model = ModelById::getAs<BoxModel>(m_model);
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
BoxLayer::setVerticalScale(VerticalScale scale)
{
    if (m_verticalScale == scale) return;
    m_verticalScale = scale;
    emit layerParametersChanged();
}

bool
BoxLayer::isLayerScrollable(const LayerGeometryProvider *v) const
{
    QPoint discard;
    return !v->shouldIlluminateLocalFeatures(this, discard);
}

bool
BoxLayer::getValueExtents(double &min, double &max,
                          bool &logarithmic, QString &unit) const
{
    auto model = ModelById::getAs<BoxModel>(m_model);
    if (!model) return false;
    min = model->getValueMinimum();
    max = model->getValueMaximum();
    unit = getScaleUnits();

    if (m_verticalScale == LogScale) logarithmic = true;

    return true;
}

bool
BoxLayer::getDisplayExtents(double &min, double &max) const
{
    auto model = ModelById::getAs<BoxModel>(m_model);
    if (!model || m_verticalScale == AutoAlignScale) return false;

    min = model->getValueMinimum();
    max = model->getValueMaximum();

    return true;
}

bool
BoxLayer::adoptExtents(double min, double max, QString unit)
{
    auto model = ModelById::getAs<BoxModel>(m_model);
    if (!model) return false;

    SVDEBUG << "BoxLayer[" << this << "]::adoptExtents: min " << min
            << ", max " << max << ", unit " << unit << endl;
    
    if (model->getScaleUnits() == "") {
        model->setScaleUnits(unit);
        return true;
    } else {
        return false;
    }
}

bool
BoxLayer::getLocalPoint(LayerGeometryProvider *v, int x, int y,
                        Event &point) const
{
    auto model = ModelById::getAs<BoxModel>(m_model);
    if (!model || !model->isReady()) return false;

    sv_frame_t frame = v->getFrameForX(x);

    EventVector onPoints = model->getEventsCovering(frame);
    if (onPoints.empty()) return false;

    Event bestContaining;
    for (const auto &p: onPoints) {
        auto r = getRange(p);
        if (y > getYForValue(v, r.first) || y < getYForValue(v, r.second)) {
            SVCERR << "inPoints: rejecting " << p.toXmlString() << endl;
            continue;
        }
        SVCERR << "inPoints: looking at " << p.toXmlString() << endl;
        if (bestContaining == Event()) {
            bestContaining = p;
            continue;
        }
        auto br = getRange(bestContaining);
        if (r.first < br.first && r.second > br.second) {
            continue;
        }
        if (r.first > br.first && r.second < br.second) {
            bestContaining = p;
            continue;
        }
        if (p.getFrame() > bestContaining.getFrame() &&
            p.getFrame() + p.getDuration() <
            bestContaining.getFrame() + bestContaining.getDuration()) {
            bestContaining = p;
            continue;
        }
    }

    if (bestContaining != Event()) {
        point = bestContaining;
    } else {
        int nearestDistance = -1;
        for (const auto &p: onPoints) {
            const auto r = getRange(p);
            int distance = std::min
                (getYForValue(v, r.first) - y,
                 getYForValue(v, r.second) - y);
            if (distance < 0) distance = -distance;
            if (nearestDistance == -1 || distance < nearestDistance) {
                nearestDistance = distance;
                point = p;
            }
        }
    }

    return true;
}

QString
BoxLayer::getLabelPreceding(sv_frame_t frame) const
{
    auto model = ModelById::getAs<BoxModel>(m_model);
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
BoxLayer::getFeatureDescription(LayerGeometryProvider *v,
                                QPoint &pos) const
{
    auto model = ModelById::getAs<BoxModel>(m_model);
    if (!model || !model->getSampleRate()) return "";
    
    Event box;
    
    if (!getLocalPoint(v, pos.x(), pos.y(), box)) {
        if (!model->isReady()) {
            return tr("In progress");
        } else {
            return tr("No local points");
        }
    }

    RealTime rt = RealTime::frame2RealTime(box.getFrame(),
                                           model->getSampleRate());
    RealTime rd = RealTime::frame2RealTime(box.getDuration(),
                                           model->getSampleRate());
    
    QString rangeText;
    auto r = getRange(box);
    
    rangeText = tr("%1 %2 - %3 %4")
        .arg(r.first).arg(getScaleUnits())
        .arg(r.second).arg(getScaleUnits());

    QString text;

    if (box.getLabel() == "") {
        text = QString(tr("Time:\t%1\nDuration:\t%2\nValue:\t%3\nNo label"))
            .arg(rt.toText(true).c_str())
            .arg(rd.toText(true).c_str())
            .arg(rangeText);
    } else {
        text = QString(tr("Time:\t%1\nDuration:\t%2\nValue:\t%3\nLabel:\t%4"))
            .arg(rt.toText(true).c_str())
            .arg(rd.toText(true).c_str())
            .arg(rangeText)
            .arg(box.getLabel());
    }

    pos = QPoint(v->getXForFrame(box.getFrame()),
                 getYForValue(v, box.getValue()));
    return text;
}

bool
BoxLayer::snapToFeatureFrame(LayerGeometryProvider *v,
                             sv_frame_t &frame,
                             int &resolution,
                             SnapType snap,
                             int ycoord) const
{
    auto model = ModelById::getAs<BoxModel>(m_model);
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

    Event containing;

    if (getLocalPoint(v, v->getXForFrame(frame), ycoord, containing)) {

        switch (snap) {

        case SnapLeft:
        case SnapNeighbouring:
            frame = containing.getFrame();
            return true;

        case SnapRight:
            frame = containing.getFrame() + containing.getDuration();
            return true;
        }
    }
    
    if (snap == SnapNeighbouring) {
        return false;
    }    

    // We aren't actually contained (in time) by any single event, so
    // seek the next one in the relevant direction

    Event e;
    
    if (snap == SnapLeft) {
        if (model->getNearestEventMatching
            (frame, [](Event) { return true; }, EventSeries::Backward, e)) {

            if (e.getFrame() + e.getDuration() < frame) {
                frame = e.getFrame() + e.getDuration();
            } else {
                frame = e.getFrame();
            }
            return true;
        }
    }
    
    if (snap == SnapRight) {
        if (model->getNearestEventMatching
            (frame, [](Event) { return true; }, EventSeries::Forward, e)) {

            frame = e.getFrame();
            return true;
        }
    }

    return false;
}

QString
BoxLayer::getScaleUnits() const
{
    auto model = ModelById::getAs<BoxModel>(m_model);
    if (model) return model->getScaleUnits();
    else return "";
}

void
BoxLayer::getScaleExtents(LayerGeometryProvider *v,
                                       double &min, double &max,
                                       bool &log) const
{
    min = 0.0;
    max = 0.0;
    log = false;

    auto model = ModelById::getAs<BoxModel>(m_model);
    if (!model) return;

    QString queryUnits;
    queryUnits = getScaleUnits();

    if (m_verticalScale == AutoAlignScale) {

        if (!v->getVisibleExtentsForUnit(queryUnits, min, max, log)) {

            min = model->getValueMinimum();
            max = model->getValueMaximum();

//            cerr << "BoxLayer[" << this << "]::getScaleExtents: min = " << min << ", max = " << max << ", log = " << log << endl;

        } else if (log) {

            LogRange::mapRange(min, max);

//            cerr << "BoxLayer[" << this << "]::getScaleExtents: min = " << min << ", max = " << max << ", log = " << log << endl;

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
BoxLayer::getYForValue(LayerGeometryProvider *v, double val) const
{
    double min = 0.0, max = 0.0;
    bool logarithmic = false;
    int h = v->getPaintHeight();

    getScaleExtents(v, min, max, logarithmic);

//    cerr << "BoxLayer[" << this << "]::getYForValue(" << val << "): min = " << min << ", max = " << max << ", log = " << logarithmic << endl;
//    cerr << "h = " << h << ", margin = " << margin << endl;

    if (logarithmic) {
        val = LogRange::map(val);
    }

    return int(h - ((val - min) * h) / (max - min));
}

double
BoxLayer::getValueForY(LayerGeometryProvider *v, int y) const
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

void
BoxLayer::paint(LayerGeometryProvider *v, QPainter &paint,
                             QRect rect) const
{
    auto model = ModelById::getAs<BoxModel>(m_model);
    if (!model || !model->isOK()) return;

    sv_samplerate_t sampleRate = model->getSampleRate();
    if (!sampleRate) return;

//    Profiler profiler("BoxLayer::paint", true);

    int x0 = rect.left() - 40;
    int x1 = x0 + rect.width() + 80;

    sv_frame_t wholeFrame0 = v->getFrameForX(0);
    sv_frame_t wholeFrame1 = v->getFrameForX(v->getPaintWidth());

    EventVector points(model->getEventsSpanning(wholeFrame0,
                                                wholeFrame1 - wholeFrame0));
    if (points.empty()) return;

    paint.setPen(getBaseQColor());

//    SVDEBUG << "BoxLayer::paint: resolution is "
//              << model->getResolution() << " frames" << endl;

    double min = model->getValueMinimum();
    double max = model->getValueMaximum();
    if (max == min) max = min + 1.0;

    QPoint localPos;
    Event illuminatePoint(0);
    bool shouldIlluminate = false;

    if (v->shouldIlluminateLocalFeatures(this, localPos)) {
        shouldIlluminate = getLocalPoint(v, localPos.x(), localPos.y(),
                                         illuminatePoint);
    }

    paint.save();
    paint.setRenderHint(QPainter::Antialiasing, false);

    QFontMetrics fm = paint.fontMetrics();
        
    for (EventVector::const_iterator i = points.begin();
         i != points.end(); ++i) {

        const Event &p(*i);
        const auto r = getRange(p);

        int x = v->getXForFrame(p.getFrame());
        int w = v->getXForFrame(p.getFrame() + p.getDuration()) - x;
        int y = getYForValue(v, r.first);
        int h = getYForValue(v, r.second) - y;
        int ex = x + w;
        int gap = v->scalePixelSize(2);

        EventVector::const_iterator j = i;
        ++j;

        if (j != points.end()) {
            const Event &q(*j);
            int nx = v->getXForFrame(q.getFrame());
            if (nx < ex) ex = nx;
        }

        if (w < 1) w = 1;

        paint.setPen(getBaseQColor());
        paint.setBrush(Qt::NoBrush);

        if ((shouldIlluminate && illuminatePoint == p) ||
            (m_editing && m_editingPoint == p)) {

            paint.setPen(QPen(getBaseQColor(), v->scalePixelSize(2)));
                
            // Qt 5.13 deprecates QFontMetrics::width(), but its suggested
            // replacement (horizontalAdvance) was only added in Qt 5.11
            // which is too new for us
#pragma GCC diagnostic ignored "-Wdeprecated-declarations"

            if (abs(h) > 2 * fm.height()) {
            
                QString y0label = QString("%1 %2")
                    .arg(r.first)
                    .arg(getScaleUnits());

                QString y1label = QString("%1 %2")
                    .arg(r.second)
                    .arg(getScaleUnits());

                PaintAssistant::drawVisibleText
                    (v, paint, 
                     x - fm.width(y0label) - gap,
                     y - fm.descent(), 
                     y0label, PaintAssistant::OutlinedText);

                PaintAssistant::drawVisibleText
                    (v, paint, 
                     x - fm.width(y1label) - gap,
                     y + h + fm.ascent(), 
                     y1label, PaintAssistant::OutlinedText);

            } else {
            
                QString ylabel = QString("%1 %2 - %3 %4")
                    .arg(r.first)
                    .arg(getScaleUnits())
                    .arg(r.second)
                    .arg(getScaleUnits());

                PaintAssistant::drawVisibleText
                    (v, paint, 
                     x - fm.width(ylabel) - gap,
                     y - fm.descent(), 
                     ylabel, PaintAssistant::OutlinedText);
            }
            
            QString t0label = RealTime::frame2RealTime
                (p.getFrame(), model->getSampleRate()).toText(true).c_str();

            QString t1label = RealTime::frame2RealTime
                (p.getFrame() + p.getDuration(), model->getSampleRate())
                .toText(true).c_str();

            PaintAssistant::drawVisibleText
                (v, paint, x, y + fm.ascent() + gap,
                 t0label, PaintAssistant::OutlinedText);

            if (w > fm.width(t0label) + fm.width(t1label) + gap * 3) {

                PaintAssistant::drawVisibleText
                    (v, paint,
                     x + w - fm.width(t1label),
                     y + fm.ascent() + gap,
                     t1label, PaintAssistant::OutlinedText);

            } else {

                PaintAssistant::drawVisibleText
                    (v, paint,
                     x + w - fm.width(t1label),
                     y + fm.ascent() + fm.height() + gap,
                     t1label, PaintAssistant::OutlinedText);
            }                
        }

        paint.drawRect(x, y, w, h);
    }

    for (EventVector::const_iterator i = points.begin();
         i != points.end(); ++i) {

        const Event &p(*i);

        QString label = p.getLabel();
        if (label == "") continue;

        if (shouldIlluminate && illuminatePoint == p) {
            continue; // already handled this in illumination special case
        }
        
        int x = v->getXForFrame(p.getFrame());
        int w = v->getXForFrame(p.getFrame() + p.getDuration()) - x;
        int y = getYForValue(v, p.getValue());

        int labelWidth = fm.width(label);

        int gap = v->scalePixelSize(2);

        if (x + w < x0 || x - labelWidth - gap > x1) {
            continue;
        }
        
        int labelX, labelY;

        labelX = x - labelWidth - gap;
        labelY = y - fm.descent();

        PaintAssistant::drawVisibleText(v, paint, labelX, labelY, label,
                                        PaintAssistant::OutlinedText);
    }

    paint.restore();
}

int
BoxLayer::getVerticalScaleWidth(LayerGeometryProvider *v,
                                             bool, QPainter &paint) const
{
    auto model = ModelById::getAs<BoxModel>(m_model);
    if (!model || m_verticalScale == AutoAlignScale) {
        return 0;
    } else {
        if (m_verticalScale == LogScale) {
            return LogNumericalScale().getWidth(v, paint);
        } else {
            return LinearNumericalScale().getWidth(v, paint);
        }
    }
}

void
BoxLayer::paintVerticalScale(LayerGeometryProvider *v,
                                          bool, QPainter &paint, QRect) const
{
    auto model = ModelById::getAs<BoxModel>(m_model);
    if (!model || model->isEmpty()) return;

    QString unit;
    double min, max;
    bool logarithmic;

    int w = getVerticalScaleWidth(v, false, paint);

    getScaleExtents(v, min, max, logarithmic);

    if (logarithmic) {
        LogNumericalScale().paintVertical(v, this, paint, 0, min, max);
    } else {
        LinearNumericalScale().paintVertical(v, this, paint, 0, min, max);
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
BoxLayer::drawStart(LayerGeometryProvider *v, QMouseEvent *e)
{
    auto model = ModelById::getAs<BoxModel>(m_model);
    if (!model) return;

    sv_frame_t frame = v->getFrameForX(e->x());
    if (frame < 0) frame = 0;
    frame = frame / model->getResolution() * model->getResolution();

    double value = getValueForY(v, e->y());

    m_editingPoint = Event(frame, float(value), 0, "");
    m_originalPoint = m_editingPoint;

    if (m_editingCommand) finish(m_editingCommand);
    m_editingCommand = new ChangeEventsCommand(m_model.untyped,
                                               tr("Draw Box"));
    m_editingCommand->add(m_editingPoint);

    m_editing = true;
}

void
BoxLayer::drawDrag(LayerGeometryProvider *v, QMouseEvent *e)
{
    auto model = ModelById::getAs<BoxModel>(m_model);
    if (!model || !m_editing) return;

    sv_frame_t dragFrame = v->getFrameForX(e->x());
    if (dragFrame < 0) dragFrame = 0;
    dragFrame = dragFrame / model->getResolution() * model->getResolution();

    sv_frame_t eventFrame = m_originalPoint.getFrame();
    sv_frame_t eventDuration = dragFrame - eventFrame;
    if (eventDuration < 0) {
        eventFrame = eventFrame + eventDuration;
        eventDuration = -eventDuration;
    } else if (eventDuration == 0) {
        eventDuration = model->getResolution();
    }

    double dragValue = getValueForY(v, e->y());

    double eventValue = m_originalPoint.getValue();
    double eventFreqDiff = dragValue - eventValue;
    if (eventFreqDiff < 0) {
        eventValue = eventValue + eventFreqDiff;
        eventFreqDiff = -eventFreqDiff;
    }

    m_editingCommand->remove(m_editingPoint);
    m_editingPoint = m_editingPoint
        .withFrame(eventFrame)
        .withDuration(eventDuration)
        .withValue(float(eventValue))
        .withLevel(float(eventFreqDiff));
    m_editingCommand->add(m_editingPoint);
}

void
BoxLayer::drawEnd(LayerGeometryProvider *, QMouseEvent *)
{
    auto model = ModelById::getAs<BoxModel>(m_model);
    if (!model || !m_editing) return;
    finish(m_editingCommand);
    m_editingCommand = nullptr;
    m_editing = false;
}

void
BoxLayer::eraseStart(LayerGeometryProvider *v, QMouseEvent *e)
{
    auto model = ModelById::getAs<BoxModel>(m_model);
    if (!model) return;

    if (!getLocalPoint(v, e->x(), e->y(), m_editingPoint)) return;

    if (m_editingCommand) {
        finish(m_editingCommand);
        m_editingCommand = nullptr;
    }

    m_editing = true;
}

void
BoxLayer::eraseDrag(LayerGeometryProvider *, QMouseEvent *)
{
}

void
BoxLayer::eraseEnd(LayerGeometryProvider *v, QMouseEvent *e)
{
    auto model = ModelById::getAs<BoxModel>(m_model);
    if (!model || !m_editing) return;

    m_editing = false;

    Event p(0);
    if (!getLocalPoint(v, e->x(), e->y(), p)) return;
    if (p.getFrame() != m_editingPoint.getFrame() ||
        p.getValue() != m_editingPoint.getValue()) return;

    m_editingCommand = new ChangeEventsCommand
        (m_model.untyped, tr("Erase Box"));

    m_editingCommand->remove(m_editingPoint);

    finish(m_editingCommand);
    m_editingCommand = nullptr;
    m_editing = false;
}

void
BoxLayer::editStart(LayerGeometryProvider *v, QMouseEvent *e)
{
    auto model = ModelById::getAs<BoxModel>(m_model);
    if (!model) return;

    if (!getLocalPoint(v, e->x(), e->y(), m_editingPoint)) {
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
}

void
BoxLayer::editDrag(LayerGeometryProvider *v, QMouseEvent *e)
{
    auto model = ModelById::getAs<BoxModel>(m_model);
    if (!model || !m_editing) return;

    int xdist = e->x() - m_dragStartX;
    int ydist = e->y() - m_dragStartY;
    int newx = m_dragPointX + xdist;
    int newy = m_dragPointY + ydist;

    sv_frame_t frame = v->getFrameForX(newx);
    if (frame < 0) frame = 0;
    frame = frame / model->getResolution() * model->getResolution();

    double value = getValueForY(v, newy);

    if (!m_editingCommand) {
        m_editingCommand = new ChangeEventsCommand
            (m_model.untyped,
             tr("Drag Box"));
    }

    m_editingCommand->remove(m_editingPoint);
    m_editingPoint = m_editingPoint
        .withFrame(frame)
        .withValue(float(value));
    m_editingCommand->add(m_editingPoint);
}

void
BoxLayer::editEnd(LayerGeometryProvider *, QMouseEvent *)
{
    auto model = ModelById::getAs<BoxModel>(m_model);
    if (!model || !m_editing) return;

    if (m_editingCommand) {

        QString newName = m_editingCommand->getName();

        if (m_editingPoint.getFrame() != m_originalPoint.getFrame()) {
            if (m_editingPoint.getValue() != m_originalPoint.getValue()) {
                newName = tr("Edit Box");
            } else {
                newName = tr("Relocate Box");
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
BoxLayer::editOpen(LayerGeometryProvider *v, QMouseEvent *e)
{
    auto model = ModelById::getAs<BoxModel>(m_model);
    if (!model) return false;

    Event region(0);
    if (!getLocalPoint(v, e->x(), e->y(), region)) return false;

    ItemEditDialog::LabelOptions labelOptions;
    labelOptions.valueLabel = tr("Minimum Value");
    labelOptions.levelLabel = tr("Value Extent");
    labelOptions.valueUnits = getScaleUnits();
    labelOptions.levelUnits = getScaleUnits();
    
    ItemEditDialog *dialog = new ItemEditDialog
        (model->getSampleRate(),
         ItemEditDialog::ShowTime |
         ItemEditDialog::ShowDuration |
         ItemEditDialog::ShowValue |
         ItemEditDialog::ShowLevel |
         ItemEditDialog::ShowText,
         labelOptions);

    dialog->setFrameTime(region.getFrame());
    dialog->setValue(region.getValue());
    dialog->setLevel(region.getLevel());
    dialog->setFrameDuration(region.getDuration());
    dialog->setText(region.getLabel());

    if (dialog->exec() == QDialog::Accepted) {

        Event newBox = region
            .withFrame(dialog->getFrameTime())
            .withValue(dialog->getValue())
            .withLevel(dialog->getLevel())
            .withDuration(dialog->getFrameDuration())
            .withLabel(dialog->getText());
        
        ChangeEventsCommand *command = new ChangeEventsCommand
            (m_model.untyped, tr("Edit Box"));
        command->remove(region);
        command->add(newBox);
        finish(command);
    }

    delete dialog;
    return true;
}

void
BoxLayer::moveSelection(Selection s, sv_frame_t newStartFrame)
{
    auto model = ModelById::getAs<BoxModel>(m_model);
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
}

void
BoxLayer::resizeSelection(Selection s, Selection newSize)
{
    auto model = ModelById::getAs<BoxModel>(m_model);
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
BoxLayer::deleteSelection(Selection s)
{
    auto model = ModelById::getAs<BoxModel>(m_model);
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
}    

void
BoxLayer::copy(LayerGeometryProvider *v, Selection s, Clipboard &to)
{
    auto model = ModelById::getAs<BoxModel>(m_model);
    if (!model) return;

    EventVector points =
        model->getEventsStartingWithin(s.getStartFrame(), s.getDuration());

    for (Event p: points) {
        to.addPoint(p.withReferenceFrame(alignToReference(v, p.getFrame())));
    }
}

bool
BoxLayer::paste(LayerGeometryProvider *v, const Clipboard &from,
                sv_frame_t /* frameOffset */, bool /* interactive */)
{
    auto model = ModelById::getAs<BoxModel>(m_model);
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
BoxLayer::toXml(QTextStream &stream,
                             QString indent, QString extraAttributes) const
{
    QString s;

    s += QString("verticalScale=\"%1\" ").arg(m_verticalScale);
    
    SingleColourLayer::toXml(stream, indent, extraAttributes + " " + s);
}

void
BoxLayer::setProperties(const QXmlAttributes &attributes)
{
    SingleColourLayer::setProperties(attributes);

    bool ok;
    VerticalScale scale = (VerticalScale)
        attributes.value("verticalScale").toInt(&ok);
    if (ok) setVerticalScale(scale);
}


