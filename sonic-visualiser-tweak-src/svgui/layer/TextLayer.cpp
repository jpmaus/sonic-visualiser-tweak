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

#include "TextLayer.h"

#include "data/model/Model.h"
#include "base/RealTime.h"
#include "base/Profiler.h"
#include "ColourDatabase.h"
#include "view/View.h"

#include "data/model/TextModel.h"

#include <QPainter>
#include <QMouseEvent>
#include <QInputDialog>
#include <QTextStream>
#include <QMessageBox>

#include <iostream>
#include <cmath>

TextLayer::TextLayer() :
    SingleColourLayer(),
    m_editing(false),
    m_originalPoint(0, 0.0, tr("Empty Label")),
    m_editingPoint(0, 0.0, tr("Empty Label")),
    m_editingCommand(nullptr)
{
    
}

int
TextLayer::getCompletion(LayerGeometryProvider *) const
{
    auto model = ModelById::get(m_model);
    if (model) return model->getCompletion();
    else return 0;
}

void
TextLayer::setModel(ModelId modelId)
{
    auto newModel = ModelById::getAs<TextModel>(modelId);
    
    if (!modelId.isNone() && !newModel) {
        throw std::logic_error("Not a TextModel");
    }
    
    if (m_model == modelId) return;
    m_model = modelId;

    if (newModel) {
        connectSignals(m_model);
    }

    emit modelReplaced();
}

Layer::PropertyList
TextLayer::getProperties() const
{
    PropertyList list = SingleColourLayer::getProperties();
    return list;
}

QString
TextLayer::getPropertyLabel(const PropertyName &name) const
{
    return SingleColourLayer::getPropertyLabel(name);
}

Layer::PropertyType
TextLayer::getPropertyType(const PropertyName &name) const
{
    return SingleColourLayer::getPropertyType(name);
}

int
TextLayer::getPropertyRangeAndValue(const PropertyName &name,
                                    int *min, int *max, int *deflt) const
{
    return SingleColourLayer::getPropertyRangeAndValue(name, min, max, deflt);
}

QString
TextLayer::getPropertyValueLabel(const PropertyName &name,
                                 int value) const
{
    return SingleColourLayer::getPropertyValueLabel(name, value);
}

void
TextLayer::setProperty(const PropertyName &name, int value)
{
    SingleColourLayer::setProperty(name, value);
}

bool
TextLayer::getValueExtents(double &, double &, bool &, QString &) const
{
    return false;
}

bool
TextLayer::isLayerScrollable(const LayerGeometryProvider *v) const
{
    QPoint discard;
    return !v->shouldIlluminateLocalFeatures(this, discard);
}

EventVector
TextLayer::getLocalPoints(LayerGeometryProvider *v, int x, int y) const
{
    auto model = ModelById::getAs<TextModel>(m_model);
    if (!model) return {};

    int overlap = ViewManager::scalePixelSize(150);
    
    sv_frame_t frame0 = v->getFrameForX(-overlap);
    sv_frame_t frame1 = v->getFrameForX(v->getPaintWidth() + overlap);
    
    EventVector points(model->getEventsSpanning(frame0, frame1 - frame0));

    EventVector rv;
    QFontMetrics metrics = QFontMetrics(QFont());

    for (EventVector::iterator i = points.begin(); i != points.end(); ++i) {

        Event p(*i);

        int px = v->getXForFrame(p.getFrame());
        int py = getYForHeight(v, p.getValue());

        QString label = p.getLabel();
        if (label == "") {
            label = tr("<no text>");
        }

        QRect rect = metrics.boundingRect
            (QRect(0, 0, 150, 200),
             Qt::AlignLeft | Qt::AlignTop | Qt::TextWordWrap, label);

        if (py + rect.height() > v->getPaintHeight()) {
            if (rect.height() > v->getPaintHeight()) py = 0;
            else py = v->getPaintHeight() - rect.height() - 1;
        }

        if (x >= px && x < px + rect.width() &&
            y >= py && y < py + rect.height()) {
            rv.push_back(p);
        }
    }

    return rv;
}

bool
TextLayer::getPointToDrag(LayerGeometryProvider *v, int x, int y, Event &p) const
{
    auto model = ModelById::getAs<TextModel>(m_model);
    if (!model) return false;

    sv_frame_t a = v->getFrameForX(x - ViewManager::scalePixelSize(120));
    sv_frame_t b = v->getFrameForX(x + ViewManager::scalePixelSize(10));
    EventVector onPoints = model->getEventsWithin(a, b);
    if (onPoints.empty()) return false;

    double nearestDistance = -1;

    for (EventVector::const_iterator i = onPoints.begin();
         i != onPoints.end(); ++i) {

        double yd = getYForHeight(v, i->getValue()) - y;
        double xd = v->getXForFrame(i->getFrame()) - x;
        double distance = sqrt(yd*yd + xd*xd);

        if (nearestDistance == -1 || distance < nearestDistance) {
            nearestDistance = distance;
            p = *i;
        }
    }

    return true;
}

QString
TextLayer::getFeatureDescription(LayerGeometryProvider *v, QPoint &pos) const
{
    int x = pos.x();

    auto model = ModelById::getAs<TextModel>(m_model);
    if (!model || !model->getSampleRate()) return "";

    EventVector points = getLocalPoints(v, x, pos.y());

    if (points.empty()) {
        if (!model->isReady()) {
            return tr("In progress");
        } else {
            return "";
        }
    }

    sv_frame_t useFrame = points.begin()->getFrame();

    RealTime rt = RealTime::frame2RealTime(useFrame, model->getSampleRate());
    
    QString text;

    if (points.begin()->getLabel() == "") {
        text = QString(tr("Time:\t%1\nHeight:\t%2\nLabel:\t%3"))
            .arg(rt.toText(true).c_str())
            .arg(points.begin()->getValue())
            .arg(points.begin()->getLabel());
    }

    pos = QPoint(v->getXForFrame(useFrame),
                 getYForHeight(v, points.begin()->getValue()));
    return text;
}


//!!! too much overlap with TimeValueLayer/TimeInstantLayer

bool
TextLayer::snapToFeatureFrame(LayerGeometryProvider *v, sv_frame_t &frame,
                              int &resolution,
                              SnapType snap, int ycoord) const
{
    auto model = ModelById::getAs<TextModel>(m_model);
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
        EventVector points = getLocalPoints(v, v->getXForFrame(frame), -1);
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

int
TextLayer::getYForHeight(LayerGeometryProvider *v, double height) const
{
    int h = v->getPaintHeight();
    return h - int(height * h);
}

double
TextLayer::getHeightForY(LayerGeometryProvider *v, int y) const
{
    int h = v->getPaintHeight();
    return double(h - y) / h;
}

void
TextLayer::paint(LayerGeometryProvider *v, QPainter &paint, QRect rect) const
{
    auto model = ModelById::getAs<TextModel>(m_model);
    if (!model || !model->isOK()) return;

    sv_samplerate_t sampleRate = model->getSampleRate();
    if (!sampleRate) return;

//    Profiler profiler("TextLayer::paint", true);

    int x0 = rect.left();
    int x1 = x0 + rect.width();
    
    int overlap = ViewManager::scalePixelSize(150);
    sv_frame_t frame0 = v->getFrameForX(x0 - overlap);
    sv_frame_t frame1 = v->getFrameForX(x1 + overlap);

    EventVector points(model->getEventsWithin(frame0, frame1 - frame0, 2));
    if (points.empty()) return;

    QColor brushColour(getBaseQColor());

    int h, s, val;
    brushColour.getHsv(&h, &s, &val);
    brushColour.setHsv(h, s, 255, 100);

    QColor penColour;
    penColour = v->getForeground();

//    SVDEBUG << "TextLayer::paint: resolution is "
//              << model->getResolution() << " frames" << endl;

    QPoint localPos;
    Event illuminatePoint(0);
    bool shouldIlluminate = false;

    if (v->shouldIlluminateLocalFeatures(this, localPos)) {
        shouldIlluminate = getPointToDrag(v, localPos.x(), localPos.y(),
                                          illuminatePoint);
    }

    int boxMaxWidth = 150;
    int boxMaxHeight = 200;

    paint.save();
    paint.setClipRect(rect.x(), 0, rect.width() + boxMaxWidth, v->getPaintHeight());
    
    for (EventVector::const_iterator i = points.begin();
         i != points.end(); ++i) {

        Event p(*i);

        int x = v->getXForFrame(p.getFrame());
        int y = getYForHeight(v, p.getValue());

        if (!shouldIlluminate || illuminatePoint != p) {
            paint.setPen(penColour);
            paint.setBrush(brushColour);
        } else {
            paint.setBrush(penColour);
            paint.setPen(v->getBackground());
        }

        QString label = p.getLabel();
        if (label == "") {
            label = tr("<no text>");
        }

        QRect boxRect = paint.fontMetrics().boundingRect
            (QRect(0, 0, boxMaxWidth, boxMaxHeight),
             Qt::AlignLeft | Qt::AlignTop | Qt::TextWordWrap, label);

        QRect textRect = QRect(3, 2, boxRect.width(), boxRect.height());
        boxRect = QRect(0, 0, boxRect.width() + 6, boxRect.height() + 2);

        if (y + boxRect.height() > v->getPaintHeight()) {
            if (boxRect.height() > v->getPaintHeight()) y = 0;
            else y = v->getPaintHeight() - boxRect.height() - 1;
        }

        boxRect = QRect(x, y, boxRect.width(), boxRect.height());
        textRect = QRect(x + 3, y + 2, textRect.width(), textRect.height());

//        boxRect = QRect(x, y, boxRect.width(), boxRect.height());
//        textRect = QRect(x + 3, y + 2, textRect.width(), textRect.height());

        paint.setRenderHint(QPainter::Antialiasing, false);
        paint.drawRect(boxRect);

        paint.setRenderHint(QPainter::Antialiasing, true);
        paint.drawText(textRect,
                       Qt::AlignLeft | Qt::AlignTop | Qt::TextWordWrap,
                       label);

///        if (p.getLabel() != "") {
///            paint.drawText(x + 5, y - paint.fontMetrics().height() + paint.fontMetrics().ascent(), p.getLabel());
///        }
    }

    paint.restore();

    // looks like save/restore doesn't deal with this:
    paint.setRenderHint(QPainter::Antialiasing, false);
}

void
TextLayer::drawStart(LayerGeometryProvider *v, QMouseEvent *e)
{
//    SVDEBUG << "TextLayer::drawStart(" << e->x() << "," << e->y() << ")" << endl;

    auto model = ModelById::getAs<TextModel>(m_model);
    if (!model) {
        SVDEBUG << "TextLayer::drawStart: no model" << endl;
        return;
    }

    sv_frame_t frame = v->getFrameForX(e->x());
    if (frame < 0) frame = 0;
    frame = frame / model->getResolution() * model->getResolution();

    double height = getHeightForY(v, e->y());

    m_editingPoint = Event(frame, float(height), "");
    m_originalPoint = m_editingPoint;

    if (m_editingCommand) finish(m_editingCommand);
    m_editingCommand = new ChangeEventsCommand(m_model.untyped, "Add Label");
    m_editingCommand->add(m_editingPoint);

    m_editing = true;
}

void
TextLayer::drawDrag(LayerGeometryProvider *v, QMouseEvent *e)
{
//    SVDEBUG << "TextLayer::drawDrag(" << e->x() << "," << e->y() << ")" << endl;

    auto model = ModelById::getAs<TextModel>(m_model);
    if (!model || !m_editing) return;

    sv_frame_t frame = v->getFrameForX(e->x());
    if (frame < 0) frame = 0;
    frame = frame / model->getResolution() * model->getResolution();

    double height = getHeightForY(v, e->y());

    m_editingCommand->remove(m_editingPoint);
    m_editingPoint = m_editingPoint
        .withFrame(frame)
        .withValue(float(height));
    m_editingCommand->add(m_editingPoint);
}

void
TextLayer::drawEnd(LayerGeometryProvider *v, QMouseEvent *)
{
//    SVDEBUG << "TextLayer::drawEnd(" << e->x() << "," << e->y() << ")" << endl;
    auto model = ModelById::getAs<TextModel>(m_model);
    if (!model || !m_editing) return;

    bool ok = false;
    QString label = QInputDialog::getText(v->getView(), tr("Enter label"),
                                          tr("Please enter a new label:"),
                                          QLineEdit::Normal, "", &ok);

    m_editingCommand->remove(m_editingPoint);
    
    if (ok) {
        m_editingPoint = m_editingPoint
            .withLabel(label);
        m_editingCommand->add(m_editingPoint);
    }

    finish(m_editingCommand);
    m_editingCommand = nullptr;
    m_editing = false;
}

void
TextLayer::eraseStart(LayerGeometryProvider *v, QMouseEvent *e)
{
    auto model = ModelById::getAs<TextModel>(m_model);
    if (!model) return;

    if (!getPointToDrag(v, e->x(), e->y(), m_editingPoint)) return;

    if (m_editingCommand) {
        finish(m_editingCommand);
        m_editingCommand = nullptr;
    }

    m_editing = true;
}

void
TextLayer::eraseDrag(LayerGeometryProvider *, QMouseEvent *)
{
}

void
TextLayer::eraseEnd(LayerGeometryProvider *v, QMouseEvent *e)
{
    auto model = ModelById::getAs<TextModel>(m_model);
    if (!model || !m_editing) return;

    m_editing = false;

    Event p;
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
TextLayer::editStart(LayerGeometryProvider *v, QMouseEvent *e)
{
//    SVDEBUG << "TextLayer::editStart(" << e->x() << "," << e->y() << ")" << endl;

    auto model = ModelById::getAs<TextModel>(m_model);
    if (!model) return;

    if (!getPointToDrag(v, e->x(), e->y(), m_editingPoint)) {
        return;
    }

    m_editOrigin = e->pos();
    m_originalPoint = m_editingPoint;

    if (m_editingCommand) {
        finish(m_editingCommand);
        m_editingCommand = nullptr;
    }

    m_editing = true;
}

void
TextLayer::editDrag(LayerGeometryProvider *v, QMouseEvent *e)
{
    auto model = ModelById::getAs<TextModel>(m_model);
    if (!model || !m_editing) return;

    sv_frame_t frameDiff =
        v->getFrameForX(e->x()) - v->getFrameForX(m_editOrigin.x());
    double heightDiff =
        getHeightForY(v, e->y()) - getHeightForY(v, m_editOrigin.y());

    sv_frame_t frame = m_originalPoint.getFrame() + frameDiff;
    double height = m_originalPoint.getValue() + heightDiff;

    if (frame < 0) frame = 0;
    frame = (frame / model->getResolution()) * model->getResolution();

    if (!m_editingCommand) {
        m_editingCommand = new ChangeEventsCommand(m_model.untyped, tr("Drag Label"));
    }

    m_editingCommand->remove(m_editingPoint);
    m_editingPoint = m_editingPoint
        .withFrame(frame)
        .withValue(float(height));
    m_editingCommand->add(m_editingPoint);
}

void
TextLayer::editEnd(LayerGeometryProvider *, QMouseEvent *)
{
//    SVDEBUG << "TextLayer::editEnd(" << e->x() << "," << e->y() << ")" << endl;
    auto model = ModelById::getAs<TextModel>(m_model);
    if (!model || !m_editing) return;

    if (m_editingCommand) {

        QString newName = m_editingCommand->getName();

        if (m_editingPoint.getFrame() != m_originalPoint.getFrame()) {
            if (m_editingPoint.getValue() != m_originalPoint.getValue()) {
                newName = tr("Move Label");
            } else {
                newName = tr("Move Label Horizontally");
            }
        } else {
            newName = tr("Move Label Vertically");
        }

        m_editingCommand->setName(newName);
        finish(m_editingCommand);
    }
    
    m_editingCommand = nullptr;
    m_editing = false;
}

bool
TextLayer::editOpen(LayerGeometryProvider *v, QMouseEvent *e)
{
    auto model = ModelById::getAs<TextModel>(m_model);
    if (!model) return false;

    Event text;
    if (!getPointToDrag(v, e->x(), e->y(), text)) return false;

    QString label = text.getLabel();

    bool ok = false;
    label = QInputDialog::getText(v->getView(), tr("Enter label"),
                                  tr("Please enter a new label:"),
                                  QLineEdit::Normal, label, &ok);
    if (ok && label != text.getLabel()) {
        ChangeEventsCommand *command =
            new ChangeEventsCommand(m_model.untyped, tr("Re-Label Point"));
        command->remove(text);
        command->add(text.withLabel(label));
        finish(command);
    }

    return true;
}    

void
TextLayer::moveSelection(Selection s, sv_frame_t newStartFrame)
{
    auto model = ModelById::getAs<TextModel>(m_model);
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
TextLayer::resizeSelection(Selection s, Selection newSize)
{
    auto model = ModelById::getAs<TextModel>(m_model);
    if (!model) return;

    ChangeEventsCommand *command =
        new ChangeEventsCommand(m_model.untyped, tr("Resize Selection"));

    EventVector points =
        model->getEventsStartingWithin(s.getStartFrame(), s.getDuration());

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
TextLayer::deleteSelection(Selection s)
{
    auto model = ModelById::getAs<TextModel>(m_model);
    if (!model) return;

    ChangeEventsCommand *command =
        new ChangeEventsCommand(m_model.untyped, tr("Delete Selection"));

    EventVector points =
        model->getEventsStartingWithin(s.getStartFrame(), s.getDuration());

    for (Event p: points) {
        command->remove(p);
    }

    finish(command);
}

void
TextLayer::copy(LayerGeometryProvider *v, Selection s, Clipboard &to)
{
    auto model = ModelById::getAs<TextModel>(m_model);
    if (!model) return;

    EventVector points =
        model->getEventsStartingWithin(s.getStartFrame(), s.getDuration());

    for (Event p: points) {
        to.addPoint(p.withReferenceFrame(alignToReference(v, p.getFrame())));
    }
}

bool
TextLayer::paste(LayerGeometryProvider *v, const Clipboard &from,
                 sv_frame_t /* frameOffset */, bool /* interactive */)
{
    auto model = ModelById::getAs<TextModel>(m_model);
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

    double valueMin = 0.0, valueMax = 1.0;
    for (EventVector::const_iterator i = points.begin();
         i != points.end(); ++i) {
        if (i->hasValue()) {
            if (i->getValue() < valueMin) valueMin = i->getValue();
            if (i->getValue() > valueMax) valueMax = i->getValue();
        }
    }
    if (valueMax < valueMin + 1.0) valueMax = valueMin + 1.0;

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
        if (p.hasValue()) {
            newPoint = newPoint.withValue(float((i->getValue() - valueMin) /
                                                (valueMax - valueMin)));
        } else {
            newPoint = newPoint.withValue(0.5f);
        }

        if (!p.hasLabel()) {
            if (p.hasValue()) {
                newPoint = newPoint.withLabel(QString("%1").arg(p.getValue()));
            } else {
                newPoint = newPoint.withLabel(tr("New Point"));
            }
        }
        
        command->add(newPoint);
    }

    finish(command);
    return true;
}

int
TextLayer::getDefaultColourHint(bool darkbg, bool &impose)
{
    impose = false;
    return ColourDatabase::getInstance()->getColourIndex
        (QString(darkbg ? "Bright Orange" : "Orange"));
}

void
TextLayer::toXml(QTextStream &stream,
                 QString indent, QString extraAttributes) const
{
    SingleColourLayer::toXml(stream, indent, extraAttributes);
}

void
TextLayer::setProperties(const QXmlAttributes &attributes)
{
    SingleColourLayer::setProperties(attributes);
}

