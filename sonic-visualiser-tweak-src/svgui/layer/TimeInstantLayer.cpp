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

#include "TimeInstantLayer.h"

#include "data/model/Model.h"
#include "base/RealTime.h"
#include "view/View.h"
#include "base/Profiler.h"
#include "base/Clipboard.h"

#include "ColourDatabase.h"
#include "PaintAssistant.h"

#include "data/model/SparseOneDimensionalModel.h"

#include "widgets/ItemEditDialog.h"
#include "widgets/ListInputDialog.h"

#include <QPainter>
#include <QMouseEvent>
#include <QTextStream>
#include <QMessageBox>

#include <iostream>
#include <cmath>

//#define DEBUG_TIME_INSTANT_LAYER 1

TimeInstantLayer::TimeInstantLayer() :
    SingleColourLayer(),
    m_editing(false),
    m_editingPoint(0, tr("New Point")),
    m_editingCommand(nullptr),
    m_plotStyle(PlotInstants)
{
}

TimeInstantLayer::~TimeInstantLayer()
{
}

int
TimeInstantLayer::getCompletion(LayerGeometryProvider *) const
{
    auto model = ModelById::get(m_model);
    if (model) return model->getCompletion();
    else return 0;
}

void
TimeInstantLayer::setModel(ModelId modelId)
{
    auto newModel = ModelById::getAs<SparseOneDimensionalModel>(modelId);
    if (!modelId.isNone() && !newModel) {
        throw std::logic_error("Not a SparseOneDimensionalModel");
    }
    
    if (m_model == modelId) return;
    m_model = modelId;

    if (newModel) {
        connectSignals(m_model);
        if (newModel->getRDFTypeURI().endsWith("Segment")) {
            setPlotStyle(PlotSegmentation);
        }
    }

    emit modelReplaced();
}

Layer::PropertyList
TimeInstantLayer::getProperties() const
{
    PropertyList list = SingleColourLayer::getProperties();
    list.push_back("Plot Type");
    return list;
}

QString
TimeInstantLayer::getPropertyLabel(const PropertyName &name) const
{
    if (name == "Plot Type") return tr("Plot Type");
    return SingleColourLayer::getPropertyLabel(name);
}

Layer::PropertyType
TimeInstantLayer::getPropertyType(const PropertyName &name) const
{
    if (name == "Plot Type") return ValueProperty;
    return SingleColourLayer::getPropertyType(name);
}

int
TimeInstantLayer::getPropertyRangeAndValue(const PropertyName &name,
                                           int *min, int *max, int *deflt) const
{
    int val = 0;

    if (name == "Plot Type") {
        
        if (min) *min = 0;
        if (max) *max = 1;
        if (deflt) *deflt = 0;
        
        val = int(m_plotStyle);

    } else {
        
        val = SingleColourLayer::getPropertyRangeAndValue(name, min, max, deflt);
    }

    return val;
}

QString
TimeInstantLayer::getPropertyValueLabel(const PropertyName &name,
                                        int value) const
{
    if (name == "Plot Type") {
        switch (value) {
        default:
        case 0: return tr("Instants");
        case 1: return tr("Segmentation");
        }
    }
    return SingleColourLayer::getPropertyValueLabel(name, value);
}

void
TimeInstantLayer::setProperty(const PropertyName &name, int value)
{
    if (name == "Plot Type") {
        setPlotStyle(PlotStyle(value));
    } else {
        SingleColourLayer::setProperty(name, value);
    }
}

void
TimeInstantLayer::setPlotStyle(PlotStyle style)
{
    if (m_plotStyle == style) return;
    m_plotStyle = style;
    emit layerParametersChanged();
}

bool
TimeInstantLayer::needsTextLabelHeight() const
{
    auto model = ModelById::getAs<SparseOneDimensionalModel>(m_model);
    if (model) return model->hasTextLabels();
    else return false;
}

bool
TimeInstantLayer::isLayerScrollable(const LayerGeometryProvider *v) const
{
    QPoint discard;
    return !v->shouldIlluminateLocalFeatures(this, discard);
}

EventVector
TimeInstantLayer::getLocalPoints(LayerGeometryProvider *v, int x) const
{
    auto model = ModelById::getAs<SparseOneDimensionalModel>(m_model);
    if (!model) return {};

    // Return a set of points that all have the same frame number, the
    // nearest to the given x coordinate, and that are within a
    // certain fuzz distance of that x coordinate.

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
TimeInstantLayer::getLabelPreceding(sv_frame_t frame) const
{
    auto model = ModelById::getAs<SparseOneDimensionalModel>(m_model);
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
TimeInstantLayer::getFeatureDescription(LayerGeometryProvider *v, QPoint &pos) const
{
    int x = pos.x();

    auto model = ModelById::getAs<SparseOneDimensionalModel>(m_model);
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
    
    QString text;

    if (points.begin()->getLabel() == "") {
        text = QString(tr("Time:\t%1\nNo label"))
            .arg(rt.toText(true).c_str());
    } else {
        text = QString(tr("Time:\t%1\nLabel:\t%2"))
            .arg(rt.toText(true).c_str())
            .arg(points.begin()->getLabel());
    }

    pos = QPoint(v->getXForFrame(useFrame), pos.y());
    return text;
}

bool
TimeInstantLayer::snapToFeatureFrame(LayerGeometryProvider *v, sv_frame_t &frame,
                                     int &resolution,
                                     SnapType snap, int ycoord) const
{
    auto model = ModelById::getAs<SparseOneDimensionalModel>(m_model);
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
TimeInstantLayer::paint(LayerGeometryProvider *v, QPainter &paint, QRect rect) const
{
    auto model = ModelById::getAs<SparseOneDimensionalModel>(m_model);
    if (!model || !model->isOK()) return;

//    Profiler profiler("TimeInstantLayer::paint", true);

    int x0 = rect.left();
    int x1 = x0 + rect.width();

    sv_frame_t resolution = model->getResolution();
    
    sv_frame_t frame0 = v->getFrameForX(x0) - resolution;
    sv_frame_t frame1 = v->getFrameForX(x1);

#ifdef DEBUG_TIME_INSTANT_LAYER
    SVCERR << "TimeInstantLayer[" << this << "]::paint: x0 = "
           << x0 << ", x1 = " << x1 << ", frame0 = " << frame0
           << ", frame1 = " << frame1 << endl;
#endif
    
    int overspill = 0;
    if (m_plotStyle == PlotSegmentation) {
        // We need to start painting at the prior point, so we can
        // fill in the visible part of its segmentation area
        overspill = 1;
    }
    
    EventVector points(model->getEventsWithin(frame0, frame1 - frame0,
                                              overspill));

#ifdef DEBUG_TIME_INSTANT_LAYER
    SVCERR << "TimeInstantLayer[" << this << "]::paint: have " << points.size()
           << " point(s) with overspill = " << overspill << " from model "
           << getModel() << endl;
#endif

    bool odd = false;
    if (m_plotStyle == PlotSegmentation && !points.empty()) {
        int index = model->getRowForFrame(points.begin()->getFrame());
        odd = ((index % 2) == 1);
    }

    paint.setPen(getBaseQColor());

    QColor brushColour(getBaseQColor());
    brushColour.setAlpha(100);
    paint.setBrush(brushColour);

    QColor oddBrushColour(brushColour);
    if (m_plotStyle == PlotSegmentation) {
        if (getBaseQColor() == Qt::black) {
            oddBrushColour = Qt::gray;
        } else if (getBaseQColor() == Qt::darkRed) {
            oddBrushColour = Qt::red;
        } else if (getBaseQColor() == Qt::darkBlue) {
            oddBrushColour = Qt::blue;
        } else if (getBaseQColor() == Qt::darkGreen) {
            oddBrushColour = Qt::green;
        } else {
            oddBrushColour = oddBrushColour.lighter(150);
        }
        oddBrushColour.setAlpha(100);
    }

    QPoint localPos;
    sv_frame_t illuminateFrame = -1;

    if (v->shouldIlluminateLocalFeatures(this, localPos)) {
        EventVector localPoints = getLocalPoints(v, localPos.x());
        if (!localPoints.empty()) {
            illuminateFrame = localPoints.begin()->getFrame();
        }
    }
        
    int prevX = -1;
    int textY = v->getTextLabelYCoord(this, paint);
    
    for (EventVector::const_iterator i = points.begin();
         i != points.end(); ++i) {

        Event p(*i);
        EventVector::const_iterator j = i;
        ++j;

        int x = v->getXForFrame(p.getFrame());

#ifdef DEBUG_TIME_INSTANT_LAYER
        SVCERR << "point frame = " << p.getFrame() << " -> x = " << x << endl;
#endif
        
        if (x == prevX && m_plotStyle == PlotInstants &&
            p.getFrame() != illuminateFrame) {
#ifdef DEBUG_TIME_INSTANT_LAYER
            SVCERR << "(skipping)" << endl;
#endif
            continue;
        }

        int iw = v->getXForFrame(p.getFrame() + model->getResolution()) - x;
        if (iw < 2) {
            if (iw < 1) {
                iw = 2;
                if (j != points.end()) {
                    int nx = v->getXForFrame(j->getFrame());
                    if (nx < x + 3) iw = 1;
                }
            } else {
                iw = 2;
            }
        }
                
        if (p.getFrame() == illuminateFrame) {
            paint.setPen(getForegroundQColor(v->getView()));
        } else {
            paint.setPen(brushColour);
        }

#ifdef DEBUG_TIME_INSTANT_LAYER
        SVCERR << "m_plotStyle = " << m_plotStyle << ", iw = " << iw << endl;
#endif
        
        if (m_plotStyle == PlotInstants) {
            if (iw > 1) {
                paint.drawRect(x, 0, iw - 1, v->getPaintHeight() - 1);
            } else {
                paint.drawLine(x, 0, x, v->getPaintHeight() - 1);
            }
        } else {

            if (odd) paint.setBrush(oddBrushColour);
            else paint.setBrush(brushColour);
            
            int nx;
            
            if (j != points.end()) {
                Event q(*j);
                nx = v->getXForFrame(q.getFrame());
            } else {
                nx = v->getXForFrame(model->getEndFrame());
            }

            if (nx >= x) {
                
                if (illuminateFrame != p.getFrame() &&
                    (nx < x + 5 || x >= v->getPaintWidth() - 1)) {
                    paint.setPen(Qt::NoPen);
                }

                paint.drawRect(x, -1, nx - x, v->getPaintHeight() + 1);
            }

            odd = !odd;
        }

        paint.setPen(getBaseQColor());
        
        if (p.getLabel() != "") {
            
    // Qt 5.13 deprecates QFontMetrics::width(), but its suggested
    // replacement (horizontalAdvance) was only added in Qt 5.11
    // which is too new for us
#pragma GCC diagnostic ignored "-Wdeprecated-declarations"

            // only draw if there's enough room from here to the next point

            int lw = paint.fontMetrics().width(p.getLabel());
            bool good = true;

            if (j != points.end()) {
                int nx = v->getXForFrame(j->getFrame());
                if (nx >= x && nx - x - iw - 3 <= lw) good = false;
            }

            if (good) {
                PaintAssistant::drawVisibleText(v, paint,
                                                x + iw + 2, textY,
                                                p.getLabel(),
                                                PaintAssistant::OutlinedText);
            }
        }

        prevX = x;
    }
}

void
TimeInstantLayer::drawStart(LayerGeometryProvider *v, QMouseEvent *e)
{
#ifdef DEBUG_TIME_INSTANT_LAYER
    cerr << "TimeInstantLayer::drawStart(" << e->x() << ")" << endl;
#endif

    auto model = ModelById::getAs<SparseOneDimensionalModel>(m_model);
    if (!model) return;

    sv_frame_t frame = v->getFrameForX(e->x());
    if (frame < 0) frame = 0;
    frame = frame / model->getResolution() * model->getResolution();

    m_editingPoint = Event(frame, tr("New Point"));

    if (m_editingCommand) finish(m_editingCommand);
    m_editingCommand = new ChangeEventsCommand(m_model.untyped, tr("Draw Point"));
    m_editingCommand->add(m_editingPoint);

    m_editing = true;
}

void
TimeInstantLayer::drawDrag(LayerGeometryProvider *v, QMouseEvent *e)
{
#ifdef DEBUG_TIME_INSTANT_LAYER
    cerr << "TimeInstantLayer::drawDrag(" << e->x() << ")" << endl;
#endif

    auto model = ModelById::getAs<SparseOneDimensionalModel>(m_model);
    if (!model || !m_editing) return;

    sv_frame_t frame = v->getFrameForX(e->x());
    if (frame < 0) frame = 0;
    frame = frame / model->getResolution() * model->getResolution();
    m_editingCommand->remove(m_editingPoint);
    m_editingPoint = m_editingPoint.withFrame(frame);
    m_editingCommand->add(m_editingPoint);
}

void
TimeInstantLayer::drawEnd(LayerGeometryProvider *, QMouseEvent *
#ifdef DEBUG_TIME_INSTANT_LAYER
                          e
#endif
    )
{
#ifdef DEBUG_TIME_INSTANT_LAYER
    cerr << "TimeInstantLayer::drawEnd(" << e->x() << ")" << endl;
#endif
    auto model = ModelById::getAs<SparseOneDimensionalModel>(m_model);
    if (!model || !m_editing) return;
    QString newName = tr("Add Point at %1 s")
        .arg(RealTime::frame2RealTime(m_editingPoint.getFrame(),
                                      model->getSampleRate())
             .toText(false).c_str());
    m_editingCommand->setName(newName);
    finish(m_editingCommand);
    m_editingCommand = nullptr;
    m_editing = false;
}

void
TimeInstantLayer::eraseStart(LayerGeometryProvider *v, QMouseEvent *e)
{
    auto model = ModelById::getAs<SparseOneDimensionalModel>(m_model);
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
TimeInstantLayer::eraseDrag(LayerGeometryProvider *, QMouseEvent *)
{
}

void
TimeInstantLayer::eraseEnd(LayerGeometryProvider *v, QMouseEvent *e)
{
    auto model = ModelById::getAs<SparseOneDimensionalModel>(m_model);
    if (!model || !m_editing) return;

    m_editing = false;

    EventVector points = getLocalPoints(v, e->x());
    if (points.empty()) return;
    if (points.begin()->getFrame() != m_editingPoint.getFrame()) return;

    m_editingCommand = new ChangeEventsCommand(m_model.untyped, tr("Erase Point"));
    m_editingCommand->remove(m_editingPoint);
    finish(m_editingCommand);
    m_editingCommand = nullptr;
    m_editing = false;
}

void
TimeInstantLayer::editStart(LayerGeometryProvider *v, QMouseEvent *e)
{
#ifdef DEBUG_TIME_INSTANT_LAYER
    cerr << "TimeInstantLayer::editStart(" << e->x() << ")" << endl;
#endif

    auto model = ModelById::getAs<SparseOneDimensionalModel>(m_model);
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
TimeInstantLayer::editDrag(LayerGeometryProvider *v, QMouseEvent *e)
{
#ifdef DEBUG_TIME_INSTANT_LAYER
    cerr << "TimeInstantLayer::editDrag(" << e->x() << ")" << endl;
#endif

    auto model = ModelById::getAs<SparseOneDimensionalModel>(m_model);
    if (!model || !m_editing) return;

    sv_frame_t frame = v->getFrameForX(e->x());
    if (frame < 0) frame = 0;
    frame = frame / model->getResolution() * model->getResolution();

    if (!m_editingCommand) {
        m_editingCommand = new ChangeEventsCommand(m_model.untyped, tr("Drag Point"));
    }

    m_editingCommand->remove(m_editingPoint);
    m_editingPoint = m_editingPoint.withFrame(frame);
    m_editingCommand->add(m_editingPoint);
}

void
TimeInstantLayer::editEnd(LayerGeometryProvider *, QMouseEvent *
#ifdef DEBUG_TIME_INSTANT_LAYER
                          e
#endif
    )
{
#ifdef DEBUG_TIME_INSTANT_LAYER
    cerr << "TimeInstantLayer::editEnd(" << e->x() << ")" << endl;
#endif
    auto model = ModelById::getAs<SparseOneDimensionalModel>(m_model);
    if (!model || !m_editing) return;
    if (m_editingCommand) {
        QString newName = tr("Move Point to %1 s")
            .arg(RealTime::frame2RealTime(m_editingPoint.getFrame(),
                                          model->getSampleRate())
                 .toText(false).c_str());
        m_editingCommand->setName(newName);
        finish(m_editingCommand);
    }
    m_editingCommand = nullptr;
    m_editing = false;
}

bool
TimeInstantLayer::editOpen(LayerGeometryProvider *v, QMouseEvent *e)
{
    auto model = ModelById::getAs<SparseOneDimensionalModel>(m_model);
    if (!model) return false;

    EventVector points = getLocalPoints(v, e->x());
    if (points.empty()) return false;

    Event point = *points.begin();

    ItemEditDialog *dialog = new ItemEditDialog
        (model->getSampleRate(),
         ItemEditDialog::ShowTime |
         ItemEditDialog::ShowText);

    dialog->setFrameTime(point.getFrame());
    dialog->setText(point.getLabel());

    if (dialog->exec() == QDialog::Accepted) {

        Event newPoint = point
            .withFrame(dialog->getFrameTime())
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
TimeInstantLayer::moveSelection(Selection s, sv_frame_t newStartFrame)
{
    auto model = ModelById::getAs<SparseOneDimensionalModel>(m_model);
    if (!model) return;

    ChangeEventsCommand *command =
        new ChangeEventsCommand(m_model.untyped, tr("Drag Selection"));

    EventVector points =
        model->getEventsWithin(s.getStartFrame(), s.getDuration());

    for (auto p: points) {
        Event newPoint = p
            .withFrame(p.getFrame() + newStartFrame - s.getStartFrame());
        command->remove(p);
        command->add(newPoint);
    }

    finish(command);
}

void
TimeInstantLayer::resizeSelection(Selection s, Selection newSize)
{
    auto model = ModelById::getAs<SparseOneDimensionalModel>(m_model);
    if (!model) return;

    ChangeEventsCommand *command =
        new ChangeEventsCommand(m_model.untyped, tr("Resize Selection"));

    EventVector points =
        model->getEventsWithin(s.getStartFrame(), s.getDuration());

    double ratio = double(newSize.getDuration()) / double(s.getDuration());
    double oldStart = double(s.getStartFrame());
    double newStart = double(newSize.getStartFrame());

    for (auto p: points) {

        double newFrame = (double(p.getFrame()) - oldStart) * ratio + newStart;

        Event newPoint = p
            .withFrame(lrint(newFrame));
        command->remove(p);
        command->add(newPoint);
    }

    finish(command);
}

void
TimeInstantLayer::deleteSelection(Selection s)
{
    auto model = ModelById::getAs<SparseOneDimensionalModel>(m_model);
    if (!model) return;

    ChangeEventsCommand *command =
        new ChangeEventsCommand(m_model.untyped, tr("Delete Selection"));

    EventVector points =
        model->getEventsWithin(s.getStartFrame(), s.getDuration());

    for (auto p: points) {
        command->remove(p);
    }

    finish(command);
}

void
TimeInstantLayer::copy(LayerGeometryProvider *v, Selection s, Clipboard &to)
{
    auto model = ModelById::getAs<SparseOneDimensionalModel>(m_model);
    if (!model) return;

    EventVector points =
        model->getEventsWithin(s.getStartFrame(), s.getDuration());

    for (auto p: points) {
        to.addPoint(p.withReferenceFrame(alignToReference(v, p.getFrame())));
    }
}

bool
TimeInstantLayer::paste(LayerGeometryProvider *v, const Clipboard &from,
                        sv_frame_t frameOffset, bool)
{
    auto model = ModelById::getAs<SparseOneDimensionalModel>(m_model);
    if (!model) return false;

    EventVector points = from.getPoints();

    bool realign = false;

    if (clipboardHasDifferentAlignment(v, from)) {

        QMessageBox::StandardButton button =
            QMessageBox::question(v->getView(), tr("Re-align pasted instants?"),
                                  tr("The instants you are pasting came from a layer with different source material from this one.  Do you want to re-align them in time, to match the source material for this layer?"),
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

        if (frameOffset > 0) frame += frameOffset;
        else if (frameOffset < 0) {
            if (frame > -frameOffset) frame += frameOffset;
            else frame = 0;
        }

        Event newPoint = i->withFrame(frame);
        
        if (!i->hasLabel() && i->hasValue()) {
            newPoint = newPoint.withLabel(QString("%1").arg(i->getValue()));
        }
        
        command->add(newPoint);
    }

    finish(command);
    return true;
}

int
TimeInstantLayer::getDefaultColourHint(bool darkbg, bool &impose)
{
    impose = false;
    return ColourDatabase::getInstance()->getColourIndex
        (QString(darkbg ? "Bright Purple" : "Purple"));
}

void
TimeInstantLayer::toXml(QTextStream &stream,
                        QString indent, QString extraAttributes) const
{
    SingleColourLayer::toXml(stream, indent,
                             extraAttributes +
                             QString(" plotStyle=\"%1\"")
                             .arg(m_plotStyle));
}

void
TimeInstantLayer::setProperties(const QXmlAttributes &attributes)
{
    SingleColourLayer::setProperties(attributes);

    bool ok;
    PlotStyle style = (PlotStyle)
        attributes.value("plotStyle").toInt(&ok);
    if (ok) setPlotStyle(style);
}

