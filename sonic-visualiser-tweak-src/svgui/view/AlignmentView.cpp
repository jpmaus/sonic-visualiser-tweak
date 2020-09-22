/* -*- c-basic-offset: 4 indent-tabs-mode: nil -*-  vi:set ts=8 sts=4 sw=4: */

/*
    Sonic Visualiser
    An audio file viewer and annotation editor.
    Centre for Digital Music, Queen Mary, University of London.
    This file copyright 2006-2014 Chris Cannam and QMUL.
    
    This program is free software; you can redistribute it and/or
    modify it under the terms of the GNU General Public License as
    published by the Free Software Foundation; either version 2 of the
    License, or (at your option) any later version.  See the file
    COPYING included with this distribution for more information.
*/

#include "AlignmentView.h"

#include <QPainter>

#include "data/model/SparseOneDimensionalModel.h"

#include "layer/TimeInstantLayer.h"

//#define DEBUG_ALIGNMENT_VIEW 1

using std::vector;
using std::set;

AlignmentView::AlignmentView(QWidget *w) :
    View(w, false),
    m_above(nullptr),
    m_below(nullptr)
{
    setObjectName(tr("AlignmentView"));
}

void
AlignmentView::keyFramesChanged()
{
#ifdef DEBUG_ALIGNMENT_VIEW
    SVCERR << "AlignmentView " << getId() << "::keyFramesChanged" << endl;
#endif
    
    // This is just a notification that we need to rebuild it - so all
    // we do here is clear it, and it'll be rebuilt on demand later
    QMutexLocker locker(&m_keyFrameMutex);
    m_keyFrameMap.clear();
}

void
AlignmentView::globalCentreFrameChanged(sv_frame_t f)
{
    View::globalCentreFrameChanged(f);
    update();
}

void
AlignmentView::viewCentreFrameChanged(View *v, sv_frame_t f)
{
    View::viewCentreFrameChanged(v, f);
    if (v == m_above) {
        m_centreFrame = f;
        update();
    } else if (v == m_below) {
        update();
    }
}

void
AlignmentView::viewManagerPlaybackFrameChanged(sv_frame_t)
{
    update();
}

void
AlignmentView::viewAboveZoomLevelChanged(ZoomLevel level, bool)
{
    m_zoomLevel = level;
    update();
}

void
AlignmentView::viewBelowZoomLevelChanged(ZoomLevel, bool)
{
    update();
}

void
AlignmentView::setViewAbove(View *v)
{
    if (m_above) {
        disconnect(m_above, nullptr, this, nullptr);
    }

    m_above = v;

    if (m_above) {
        connect(m_above,
		SIGNAL(zoomLevelChanged(ZoomLevel, bool)),
                this, 
		SLOT(viewAboveZoomLevelChanged(ZoomLevel, bool)));
        connect(m_above,
                SIGNAL(propertyContainerAdded(PropertyContainer *)),
                this,
                SLOT(keyFramesChanged()));
        connect(m_above,
                SIGNAL(layerModelChanged()),
                this,
                SLOT(keyFramesChanged()));
    }

    keyFramesChanged();
}

void
AlignmentView::setViewBelow(View *v)
{
    if (m_below) {
        disconnect(m_below, nullptr, this, nullptr);
    }

    m_below = v;

    if (m_below) {
        connect(m_below,
		SIGNAL(zoomLevelChanged(ZoomLevel, bool)),
                this, 
		SLOT(viewBelowZoomLevelChanged(ZoomLevel, bool)));
        connect(m_below,
                SIGNAL(propertyContainerAdded(PropertyContainer *)),
                this,
                SLOT(keyFramesChanged()));
        connect(m_below,
                SIGNAL(layerModelChanged()),
                this,
                SLOT(keyFramesChanged()));
    }

    keyFramesChanged();
}

void
AlignmentView::paintEvent(QPaintEvent *)
{
    if (m_above == nullptr || m_below == nullptr || !m_manager) return;
    
#ifdef DEBUG_ALIGNMENT_VIEW
    SVCERR << "AlignmentView " << getId() << "::paintEvent" << endl;
#endif
    
    bool darkPalette = false;
    if (m_manager) darkPalette = m_manager->getGlobalDarkBackground();

    QColor fg, bg;
    if (darkPalette) {
        fg = Qt::gray;
        bg = Qt::black;
    } else {
        fg = Qt::black;
        bg = Qt::gray;
    }

    QPainter paint(this);
    paint.setPen(QPen(fg, 2));
    paint.setBrush(Qt::NoBrush);
    paint.setRenderHint(QPainter::Antialiasing, true);

    paint.fillRect(rect(), bg);

    QMutexLocker locker(&m_keyFrameMutex);

    if (m_keyFrameMap.empty()) {
        reconnectModels();
        buildKeyFrameMap();
    }

#ifdef DEBUG_ALIGNMENT_VIEW
    SVCERR << "AlignmentView " << getId() << "::paintEvent: painting "
           << m_keyFrameMap.size() << " mappings" << endl;
#endif

    for (const auto &km: m_keyFrameMap) {

        sv_frame_t af = km.first;
        sv_frame_t bf = km.second;

        int ax = m_above->getXForFrame(af);
        int bx = m_below->getXForFrame(bf);

        if (ax >= 0 || ax < width() || bx >= 0 || bx < width()) {
            paint.drawLine(ax, 0, bx, height());
        }
    }

    paint.end();
}        

void
AlignmentView::reconnectModels()
{
    vector<ModelId> toConnect { 
        getSalientModel(m_above),
        getSalientModel(m_below)
    };

    for (auto modelId: toConnect) {
        if (auto model = ModelById::get(modelId)) {
            auto referenceId = model->getAlignmentReference();
            if (!referenceId.isNone()) {
                toConnect.push_back(referenceId);
            }
        }
    }

    for (auto modelId: toConnect) {
        if (auto model = ModelById::get(modelId)) {
            auto ptr = model.get();
            disconnect(ptr, 0, this, 0);
            connect(ptr, SIGNAL(modelChanged(ModelId)),
                    this, SLOT(keyFramesChanged()));
            connect(ptr, SIGNAL(completionChanged(ModelId)),
                    this, SLOT(keyFramesChanged()));
            connect(ptr, SIGNAL(alignmentCompletionChanged(ModelId)),
                    this, SLOT(keyFramesChanged()));
        }
    }
}

void
AlignmentView::buildKeyFrameMap()
{
#ifdef DEBUG_ALIGNMENT_VIEW
    SVCERR << "AlignmentView " << getId() << "::buildKeyFrameMap" << endl;
#endif
    
    sv_frame_t resolution = 1;

    set<sv_frame_t> keyFramesBelow;
    for (auto f: getKeyFrames(m_below, resolution)) {
        keyFramesBelow.insert(f);
    }

    vector<sv_frame_t> keyFrames = getKeyFrames(m_above, resolution);

    foreach (sv_frame_t f, keyFrames) {

        sv_frame_t rf = m_above->alignToReference(f);
        sv_frame_t bf = m_below->alignFromReference(rf);

        bool mappedSomething = false;
        
        if (resolution > 1) {
            if (keyFramesBelow.find(bf) == keyFramesBelow.end()) {

                sv_frame_t f1 = f + resolution;
                sv_frame_t rf1 = m_above->alignToReference(f1);
                sv_frame_t bf1 = m_below->alignFromReference(rf1);

                for (sv_frame_t probe = bf + 1; probe <= bf1; ++probe) {
                    if (keyFramesBelow.find(probe) != keyFramesBelow.end()) {
                        m_keyFrameMap.insert({ f, probe });
                        mappedSomething = true;
                    }
                }
            }
        }

        if (!mappedSomething) {
            m_keyFrameMap.insert({ f, bf });
        }
    }

#ifdef DEBUG_ALIGNMENT_VIEW
    SVCERR << "AlignmentView " << getId() << "::buildKeyFrameMap: have "
           << m_keyFrameMap.size() << " mappings" << endl;
#endif
}

vector<sv_frame_t>
AlignmentView::getKeyFrames(View *view, sv_frame_t &resolution)
{
    resolution = 1;
    
    if (!view) {
        return getDefaultKeyFrames();
    }

    ModelId m = getSalientModel(view);
    auto model = ModelById::getAs<SparseOneDimensionalModel>(m);
    if (!model) {
        return getDefaultKeyFrames();
    }

    resolution = model->getResolution();
    
    vector<sv_frame_t> keyFrames;

    EventVector pp = model->getAllEvents();
    for (EventVector::const_iterator pi = pp.begin(); pi != pp.end(); ++pi) {
        keyFrames.push_back(pi->getFrame());
    }

    return keyFrames;
}

vector<sv_frame_t>
AlignmentView::getDefaultKeyFrames()
{
    vector<sv_frame_t> keyFrames;
    return keyFrames;

#ifdef NOT_REALLY
    if (!m_above || !m_manager) return keyFrames;

    sv_samplerate_t rate = m_manager->getMainModelSampleRate();
    if (rate == 0) return keyFrames;

    for (sv_frame_t f = m_above->getModelsStartFrame(); 
         f <= m_above->getModelsEndFrame(); 
         f += sv_frame_t(rate * 5 + 0.5)) {
        keyFrames.push_back(f);
    }
    
    return keyFrames;
#endif
}

ModelId
AlignmentView::getSalientModel(View *view)
{
    ModelId m;
    
    // get the topmost such
    for (int i = 0; i < view->getLayerCount(); ++i) {
        if (qobject_cast<TimeInstantLayer *>(view->getLayer(i))) {
            ModelId mm = view->getLayer(i)->getModel();
            if (ModelById::isa<SparseOneDimensionalModel>(mm)) {
                m = mm;
            }
        }
    }

    return m;
}


