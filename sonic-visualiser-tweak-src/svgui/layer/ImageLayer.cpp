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

#include "ImageLayer.h"

#include "data/model/Model.h"
#include "base/RealTime.h"
#include "base/Profiler.h"
#include "view/View.h"

#include "data/model/ImageModel.h"
#include "data/fileio/FileSource.h"

#include "widgets/ImageDialog.h"
#include "widgets/ProgressDialog.h"

#include <QPainter>
#include <QMouseEvent>
#include <QInputDialog>
#include <QMutexLocker>
#include <QTextStream>
#include <QMessageBox>

#include <iostream>
#include <cmath>

ImageLayer::ImageMap
ImageLayer::m_images;

QMutex
ImageLayer::m_imageMapMutex;

ImageLayer::ImageLayer() :
    m_editing(false),
    m_editingCommand(nullptr)
{
}

ImageLayer::~ImageLayer()
{
    for (FileSourceMap::iterator i = m_fileSources.begin();
         i != m_fileSources.end(); ++i) {
        delete i->second;
    }
}

int
ImageLayer::getCompletion(LayerGeometryProvider *) const
{
    auto model = ModelById::get(m_model);
    if (model) return model->getCompletion();
    else return 0;
}

void
ImageLayer::setModel(ModelId modelId)
{
    auto newModel = ModelById::getAs<ImageModel>(modelId);
    
    if (!modelId.isNone() && !newModel) {
        throw std::logic_error("Not an ImageModel");
    }
    
    if (m_model == modelId) return;
    m_model = modelId;

    if (newModel) {
        connectSignals(m_model);
    }

    emit modelReplaced();
}

Layer::PropertyList
ImageLayer::getProperties() const
{
    return Layer::getProperties();
}

QString
ImageLayer::getPropertyLabel(const PropertyName &) const
{
    return "";
}

Layer::PropertyType
ImageLayer::getPropertyType(const PropertyName &name) const
{
    return Layer::getPropertyType(name);
}

int
ImageLayer::getPropertyRangeAndValue(const PropertyName &name,
                                    int *min, int *max, int *deflt) const
{
    return Layer::getPropertyRangeAndValue(name, min, max, deflt);
}

QString
ImageLayer::getPropertyValueLabel(const PropertyName &name,
                                 int value) const
{
    return Layer::getPropertyValueLabel(name, value);
}

void
ImageLayer::setProperty(const PropertyName &name, int value)
{
    Layer::setProperty(name, value);
}

bool
ImageLayer::getValueExtents(double &, double &, bool &, QString &) const
{
    return false;
}

bool
ImageLayer::isLayerScrollable(const LayerGeometryProvider *) const
{
    return true;
}

EventVector
ImageLayer::getLocalPoints(LayerGeometryProvider *v, int x, int ) const
{
    auto model = ModelById::getAs<ImageModel>(m_model);
    if (!model) return {};

//    SVDEBUG << "ImageLayer::getLocalPoints(" << x << "," << y << "):";
    EventVector points(model->getAllEvents());

    EventVector rv;

    for (EventVector::const_iterator i = points.begin(); i != points.end(); ) {

        Event p(*i);
        int px = v->getXForFrame(p.getFrame());
        if (px > x) break;

        ++i;
        if (i != points.end()) {
            int nx = v->getXForFrame(i->getFrame());
            if (nx < x) {
                // as we aim not to overlap the images, if the following
                // image begins to the left of a point then the current
                // one may be assumed to end to the left of it as well.
                continue;
            }
        }

        // this image is a candidate, test it properly

        int width = 32;
        if (m_scaled[v].find(p.getURI()) != m_scaled[v].end()) {
            width = m_scaled[v][p.getURI()].width();
//            SVDEBUG << "scaled width = " << width << endl;
        }

        if (x >= px && x < px + width) {
            rv.push_back(p);
        }
    }

//    cerr << rv.size() << " point(s)" << endl;

    return rv;
}

QString
ImageLayer::getFeatureDescription(LayerGeometryProvider *v, QPoint &pos) const
{
    int x = pos.x();

    auto model = ModelById::getAs<ImageModel>(m_model);
    if (!model || !model->getSampleRate()) return "";

    EventVector points = getLocalPoints(v, x, pos.y());

    if (points.empty()) {
        if (!model->isReady()) {
            return tr("In progress");
        } else {
            return "";
        }
    }

//    int useFrame = points.begin()->frame;

//    RealTime rt = RealTime::frame2RealTime(useFrame, model->getSampleRate());

    QString text;
/*    
    if (points.begin()->label == "") {
        text = QString(tr("Time:\t%1\nHeight:\t%2\nLabel:\t%3"))
            .arg(rt.toText(true).c_str())
            .arg(points.begin()->height)
            .arg(points.begin()->label);
    }

    pos = QPoint(v->getXForFrame(useFrame),
                 getYForHeight(v, points.begin()->height));
*/
    return text;
}


//!!! too much overlap with TimeValueLayer/TimeInstantLayer/TextLayer

bool
ImageLayer::snapToFeatureFrame(LayerGeometryProvider *v, sv_frame_t &frame,
                               int &resolution,
                               SnapType snap, int ycoord) const
{
    auto model = ModelById::getAs<ImageModel>(m_model);
    if (!model) {
        return Layer::snapToFeatureFrame(v, frame, resolution, snap, ycoord);
    }

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

void
ImageLayer::paint(LayerGeometryProvider *v, QPainter &paint, QRect rect) const
{
    auto model = ModelById::getAs<ImageModel>(m_model);
    if (!model || !model->isOK()) return;

    sv_samplerate_t sampleRate = model->getSampleRate();
    if (!sampleRate) return;

//    Profiler profiler("ImageLayer::paint", true);

//    int x0 = rect.left(), x1 = rect.right();
    int x0 = 0, x1 = v->getPaintWidth();

    sv_frame_t frame0 = v->getFrameForX(x0);
    sv_frame_t frame1 = v->getFrameForX(x1);

    EventVector points(model->getEventsWithin(frame0, frame1 - frame0, 2));
    if (points.empty()) return;

    paint.save();
    paint.setClipRect(rect.x(), 0, rect.width(), v->getPaintHeight());

    QColor penColour;
    penColour = v->getForeground();

    QColor brushColour;
    brushColour = v->getBackground();

    int h, s, val;
    brushColour.getHsv(&h, &s, &val);
    brushColour.setHsv(h, s, 255, 240);

    paint.setPen(penColour);
    paint.setBrush(brushColour);
    paint.setRenderHint(QPainter::Antialiasing, true);

    for (EventVector::const_iterator i = points.begin();
         i != points.end(); ++i) {

        Event p(*i);

        int x = v->getXForFrame(p.getFrame());

        int nx = x + 2000;
        EventVector::const_iterator j = i;
        ++j;
        if (j != points.end()) {
            int jx = v->getXForFrame(j->getFrame());
            if (jx < nx) nx = jx;
        }

        drawImage(v, paint, p, x, nx);
    }

    paint.setRenderHint(QPainter::Antialiasing, false);
    paint.restore();
}

void
ImageLayer::drawImage(LayerGeometryProvider *v, QPainter &paint, const Event &p,
                      int x, int nx) const
{
    QString label = p.getLabel();
    QString imageName = p.getURI();

    QImage image;
    QString additionalText;

    QSize imageSize;
    if (!getImageOriginalSize(imageName, imageSize)) {
        image = QImage(":icons/emptypage.png");
        imageSize = image.size();
        additionalText = imageName;
    }

    int topMargin = 10;
    int bottomMargin = 10;
    int spacing = 5;

    if (v->getPaintHeight() < 100) {
        topMargin = 5;
        bottomMargin = 5;
    }

    int maxBoxHeight = v->getPaintHeight() - topMargin - bottomMargin;

    int availableWidth = nx - x - 3;
    if (availableWidth < 20) availableWidth = 20;

    QRect labelRect;

    if (label != "") {

        int likelyHeight = v->getPaintHeight() / 4;

        int likelyWidth = // available height times image aspect
            ((maxBoxHeight - likelyHeight) * imageSize.width())
            / imageSize.height();

        if (likelyWidth > imageSize.width()) {
            likelyWidth = imageSize.width();
        }

        if (likelyWidth > availableWidth) {
            likelyWidth = availableWidth;
        }

        // Qt 5.13 deprecates QFontMetrics::width(), but its suggested
        // replacement (horizontalAdvance) was only added in Qt 5.11
        // which is too new for us
#pragma GCC diagnostic ignored "-Wdeprecated-declarations"

        int singleWidth = paint.fontMetrics().width(label);
        if (singleWidth < availableWidth && singleWidth < likelyWidth * 2) {
            likelyWidth = singleWidth + 4;
        }

        labelRect = paint.fontMetrics().boundingRect
            (QRect(0, 0, likelyWidth, likelyHeight),
             Qt::AlignCenter | Qt::TextWordWrap, label);

        labelRect.setWidth(labelRect.width() + 6);
    }

    if (image.isNull()) {
        image = getImage(v, imageName,
                         QSize(availableWidth,
                               maxBoxHeight - labelRect.height()));
    }

    int boxWidth = image.width();
    if (boxWidth < labelRect.width()) {
        boxWidth = labelRect.width();
    }

    int boxHeight = image.height();
    if (label != "") {
        boxHeight += labelRect.height() + spacing;
    }

    int division = image.height();

    if (additionalText != "") {

        paint.save();

        QFont font(paint.font());
        font.setItalic(true);
        paint.setFont(font);

        int tw = paint.fontMetrics().width(additionalText);
        if (tw > availableWidth) {
            tw = availableWidth;
        }
        if (boxWidth < tw) {
            boxWidth = tw;
        }
        boxHeight += paint.fontMetrics().height();
        division += paint.fontMetrics().height();
    }                

    bottomMargin = v->getPaintHeight() - topMargin - boxHeight;
    if (bottomMargin > topMargin + v->getPaintHeight()/7) {
        topMargin += v->getPaintHeight()/8;
        bottomMargin -= v->getPaintHeight()/8;
    }

    paint.drawRect(x - 1,
                   topMargin - 1,
                   boxWidth + 2,
                   boxHeight + 2);

    int imageY;
    if (label != "") {
        imageY = topMargin + labelRect.height() + spacing;
    } else {
        imageY = topMargin;
    }

    paint.drawImage(x + (boxWidth - image.width())/2,
                    imageY,
                    image);

    if (additionalText != "") {
        paint.drawText(x,
                       imageY + image.height() + paint.fontMetrics().ascent(),
                       additionalText);
        paint.restore();
    }

    if (label != "") {
        paint.drawLine(x,
                       topMargin + labelRect.height() + spacing,
                       x + boxWidth, 
                       topMargin + labelRect.height() + spacing);

        paint.drawText(QRect(x,
                             topMargin,
                             boxWidth,
                             labelRect.height()),
                       Qt::AlignCenter | Qt::TextWordWrap,
                       label);
    }
}

void
ImageLayer::setLayerDormant(const LayerGeometryProvider *v, bool dormant)
{
    if (dormant) {
        // Delete the images named in the view's scaled map from the
        // general image map as well.  They can always be re-loaded
        // if it turns out another view still needs them.
        QMutexLocker locker(&m_imageMapMutex);
        for (ImageMap::iterator i = m_scaled[v].begin();
             i != m_scaled[v].end(); ++i) {
            m_images.erase(i->first);
        }
        m_scaled.erase(v);
    }
}

//!!! how to reap no-longer-used images?

bool
ImageLayer::getImageOriginalSize(QString name, QSize &size) const
{
//    cerr << "getImageOriginalSize: \"" << name << "\"" << endl;

    QMutexLocker locker(&m_imageMapMutex);
    if (m_images.find(name) == m_images.end()) {
//        cerr << "don't have, trying to open local" << endl;
        m_images[name] = QImage(getLocalFilename(name));
    }
    if (m_images[name].isNull()) {
//        cerr << "null image" << endl;
        return false;
    } else {
        size = m_images[name].size();
        return true;
    }
}

QImage 
ImageLayer::getImage(LayerGeometryProvider *v, QString name, QSize maxSize) const
{
//    SVDEBUG << "ImageLayer::getImage(" << v << ", " << name << ", ("
//              << maxSize.width() << "x" << maxSize.height() << "))" << endl;

    if (!m_scaled[v][name].isNull()  &&
        ((m_scaled[v][name].width()  == maxSize.width() &&
          m_scaled[v][name].height() <= maxSize.height()) ||
         (m_scaled[v][name].width()  <= maxSize.width() &&
          m_scaled[v][name].height() == maxSize.height()))) {
//        cerr << "cache hit" << endl;
        return m_scaled[v][name];
    }

    QMutexLocker locker(&m_imageMapMutex);

    if (m_images.find(name) == m_images.end()) {
        m_images[name] = QImage(getLocalFilename(name));
    }

    if (m_images[name].isNull()) {
//        cerr << "null image" << endl;
        m_scaled[v][name] = QImage();
    } else if (m_images[name].width() <= maxSize.width() &&
               m_images[name].height() <= maxSize.height()) {
        m_scaled[v][name] = m_images[name];
    } else {
        m_scaled[v][name] =
            m_images[name].scaled(maxSize,
                                  Qt::KeepAspectRatio,
                                  Qt::SmoothTransformation);
    }

    return m_scaled[v][name];
}

void
ImageLayer::drawStart(LayerGeometryProvider *v, QMouseEvent *e)
{
//    SVDEBUG << "ImageLayer::drawStart(" << e->x() << "," << e->y() << ")" << endl;

    auto model = ModelById::getAs<ImageModel>(m_model);
    if (!model) {
        SVDEBUG << "ImageLayer::drawStart: no model" << endl;
        return;
    }

    sv_frame_t frame = v->getFrameForX(e->x());
    if (frame < 0) frame = 0;
    frame = frame / model->getResolution() * model->getResolution();

    m_editingPoint = Event(frame);
    m_originalPoint = m_editingPoint;

    if (m_editingCommand) finish(m_editingCommand);
    m_editingCommand = new ChangeEventsCommand(m_model.untyped, "Add Image");
    m_editingCommand->add(m_editingPoint);

    m_editing = true;
}

void
ImageLayer::drawDrag(LayerGeometryProvider *v, QMouseEvent *e)
{
//    SVDEBUG << "ImageLayer::drawDrag(" << e->x() << "," << e->y() << ")" << endl;

    auto model = ModelById::getAs<ImageModel>(m_model);
    if (!model || !m_editing) return;

    sv_frame_t frame = v->getFrameForX(e->x());
    if (frame < 0) frame = 0;
    frame = frame / model->getResolution() * model->getResolution();

    m_editingCommand->remove(m_editingPoint);
    m_editingPoint = m_editingPoint
        .withFrame(frame);
    m_editingCommand->add(m_editingPoint);
}

void
ImageLayer::drawEnd(LayerGeometryProvider *, QMouseEvent *)
{
//    SVDEBUG << "ImageLayer::drawEnd(" << e->x() << "," << e->y() << ")" << endl;
    auto model = ModelById::getAs<ImageModel>(m_model);
    if (!model || !m_editing) return;

    ImageDialog dialog(tr("Select image"), "", "");

    m_editingCommand->remove(m_editingPoint);

    if (dialog.exec() == QDialog::Accepted) {

        checkAddSource(dialog.getImage());

        m_editingPoint = m_editingPoint
            .withURI(dialog.getImage())
            .withLabel(dialog.getLabel());
        m_editingCommand->add(m_editingPoint);
    }

    finish(m_editingCommand);
    m_editingCommand = nullptr;
    m_editing = false;
}

bool
ImageLayer::addImage(sv_frame_t frame, QString url)
{
    QImage image(getLocalFilename(url));
    if (image.isNull()) {
        cerr << "Failed to open image from url \"" << url << "\" (local filename \"" << getLocalFilename(url) << "\"" << endl;
        delete m_fileSources[url];
        m_fileSources.erase(url);
        return false;
    }

    Event point = Event(frame).withURI(url);
    auto command =
        new ChangeEventsCommand(m_model.untyped, "Add Image");
    command->add(point);
    finish(command);
    return true;
}

void
ImageLayer::editStart(LayerGeometryProvider *v, QMouseEvent *e)
{
//    SVDEBUG << "ImageLayer::editStart(" << e->x() << "," << e->y() << ")" << endl;

    auto model = ModelById::getAs<ImageModel>(m_model);
    if (!model) return;

    EventVector points = getLocalPoints(v, e->x(), e->y());
    if (points.empty()) return;

    m_editOrigin = e->pos();
    m_editingPoint = *points.begin();
    m_originalPoint = m_editingPoint;

    if (m_editingCommand) {
        finish(m_editingCommand);
        m_editingCommand = nullptr;
    }

    m_editing = true;
}

void
ImageLayer::editDrag(LayerGeometryProvider *v, QMouseEvent *e)
{
    auto model = ModelById::getAs<ImageModel>(m_model);
    if (!model || !m_editing) return;

    sv_frame_t frameDiff = v->getFrameForX(e->x()) - v->getFrameForX(m_editOrigin.x());
    sv_frame_t frame = m_originalPoint.getFrame() + frameDiff;

    if (frame < 0) frame = 0;
    frame = (frame / model->getResolution()) * model->getResolution();

    if (!m_editingCommand) {
        m_editingCommand = new ChangeEventsCommand(m_model.untyped, tr("Move Image"));
    }

    m_editingCommand->remove(m_editingPoint);
    m_editingPoint = m_editingPoint
        .withFrame(frame);
    m_editingCommand->add(m_editingPoint);
}

void
ImageLayer::editEnd(LayerGeometryProvider *, QMouseEvent *)
{
//    SVDEBUG << "ImageLayer::editEnd(" << e->x() << "," << e->y() << ")" << endl;
    auto model = ModelById::getAs<ImageModel>(m_model);
    if (!model || !m_editing) return;

    if (m_editingCommand) {
        finish(m_editingCommand);
    }
    
    m_editingCommand = nullptr;
    m_editing = false;
}

bool
ImageLayer::editOpen(LayerGeometryProvider *v, QMouseEvent *e)
{
    auto model = ModelById::getAs<ImageModel>(m_model);
    if (!model) return false;

    EventVector points = getLocalPoints(v, e->x(), e->y());
    if (points.empty()) return false;

    QString image = points.begin()->getURI();
    QString label = points.begin()->getLabel();

    ImageDialog dialog(tr("Select image"),
                       image,
                       label);

    if (dialog.exec() == QDialog::Accepted) {

        checkAddSource(dialog.getImage());

        auto command =
            new ChangeEventsCommand(m_model.untyped, tr("Edit Image"));
        command->remove(*points.begin());
        command->add(points.begin()->
                     withURI(dialog.getImage()).withLabel(dialog.getLabel()));
        finish(command);
    }

    return true;
}    

void
ImageLayer::moveSelection(Selection s, sv_frame_t newStartFrame)
{
    auto model = ModelById::getAs<ImageModel>(m_model);
    if (!model) return;

    auto command =
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
ImageLayer::resizeSelection(Selection s, Selection newSize)
{
    auto model = ModelById::getAs<ImageModel>(m_model);
    if (!model) return;

    auto command =
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
ImageLayer::deleteSelection(Selection s)
{
    auto model = ModelById::getAs<ImageModel>(m_model);
    if (!model) return;

    auto command =
        new ChangeEventsCommand(m_model.untyped, tr("Delete Selection"));

    EventVector points =
        model->getEventsStartingWithin(s.getStartFrame(), s.getDuration());

    for (Event p: points) {
        command->remove(p);
    }

    finish(command);
}

void
ImageLayer::copy(LayerGeometryProvider *v, Selection s, Clipboard &to)
{
    auto model = ModelById::getAs<ImageModel>(m_model);
    if (!model) return;

    EventVector points =
        model->getEventsStartingWithin(s.getStartFrame(), s.getDuration());

    for (Event p: points) {
        to.addPoint(p.withReferenceFrame(alignToReference(v, p.getFrame())));
    }
}

bool
ImageLayer::paste(LayerGeometryProvider *v, const Clipboard &from,
                  sv_frame_t /* frameOffset */, bool /* interactive */)
{
    auto model = ModelById::getAs<ImageModel>(m_model);
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

        //!!! inadequate
        
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

QString
ImageLayer::getLocalFilename(QString img) const
{
    if (m_fileSources.find(img) == m_fileSources.end()) {
        checkAddSource(img);
        if (m_fileSources.find(img) == m_fileSources.end()) {
            return img;
        }
    }
    return m_fileSources[img]->getLocalFilename();
}

void
ImageLayer::checkAddSource(QString img) const
{
    SVDEBUG << "ImageLayer::checkAddSource(" << img << "): yes, trying..." << endl;

    if (m_fileSources.find(img) != m_fileSources.end()) {
        return;
    }

    ProgressDialog dialog(tr("Opening image URL..."), true, 2000);
    FileSource *rf = new FileSource(img, &dialog);
    if (rf->isOK()) {
        cerr << "ok, adding it (local filename = " << rf->getLocalFilename() << ")" << endl;
        m_fileSources[img] = rf;
        connect(rf, SIGNAL(ready()), this, SLOT(fileSourceReady()));
    } else {
        delete rf;
    }
}

void
ImageLayer::checkAddSources()
{
    auto model = ModelById::getAs<ImageModel>(m_model);
    const EventVector &points(model->getAllEvents());

    for (EventVector::const_iterator i = points.begin();
         i != points.end(); ++i) {
        
        checkAddSource((*i).getURI());
    }
}

void
ImageLayer::fileSourceReady()
{
//    SVDEBUG << "ImageLayer::fileSourceReady" << endl;

    FileSource *rf = dynamic_cast<FileSource *>(sender());
    if (!rf) return;

    QString img;
    for (FileSourceMap::const_iterator i = m_fileSources.begin();
         i != m_fileSources.end(); ++i) {
        if (i->second == rf) {
            img = i->first;
//            cerr << "it's image \"" << img << "\"" << endl;
            break;
        }
    }
    if (img == "") return;

    QMutexLocker locker(&m_imageMapMutex);
    m_images.erase(img);
    for (ViewImageMap::iterator i = m_scaled.begin(); i != m_scaled.end(); ++i) {
        i->second.erase(img);
        emit modelChanged(getModel());
    }
}

void
ImageLayer::toXml(QTextStream &stream,
                  QString indent, QString extraAttributes) const
{
    Layer::toXml(stream, indent, extraAttributes);
}

void
ImageLayer::setProperties(const QXmlAttributes &)
{
}

